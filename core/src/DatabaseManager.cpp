#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QUuid>

namespace Calculadora {

DatabaseManager::DatabaseManager(const QString& dbPath)
    : m_dbPath(dbPath)
    , m_connectionName(QUuid::createUuid().toString())
{
}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::initialize() {
    QDir dir = QFileInfo(m_dbPath).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qCritical() << "No se pudo abrir la base de datos:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");

    return runMigrations();
}

bool DatabaseManager::isOpen() const { return m_db.isOpen(); }
QSqlDatabase& DatabaseManager::database() { return m_db; }
const QString& DatabaseManager::connectionName() const { return m_connectionName; }

// ---------------------------------------------------------------------------
// Migraciones
// ---------------------------------------------------------------------------

int DatabaseManager::getSchemaVersion() {
    QSqlQuery q(m_db);
    q.exec("PRAGMA user_version");
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool DatabaseManager::setSchemaVersion(int version) {
    QSqlQuery q(m_db);
    return q.exec(QString("PRAGMA user_version = %1").arg(version));
}

bool DatabaseManager::runMigrations() {
    int current = getSchemaVersion();
    qDebug() << "Schema version:" << current << "-> target:" << SCHEMA_VERSION_CURRENT;

    if (current < 1) {
        if (!migrateV0toV1()) return false;
        setSchemaVersion(1);
        current = 1;
    }
    if (current < 2) {
        if (!migrateV1toV2()) return false;
        setSchemaVersion(2);
        current = 2;
    }
    if (current < 3) {
        if (!migrateV2toV3()) return false;
        setSchemaVersion(3);
        current = 3;
    }
    if (current < 4) {
        if (!migrateV3toV4()) return false;
        setSchemaVersion(4);
        current = 4;
    }
    if (current < 5) {
        if (!migrateV4toV5()) return false;
        setSchemaVersion(5);
        current = 5;
    }
    if (current < 6) {
        if (!migrateV5toV6()) return false;
        setSchemaVersion(6);
    }
    return true;
}

// ---------------------------------------------------------------------------
// V0 -> V1: Schema inicial
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV0toV1() {
    QSqlQuery q(m_db);

    // concesiones — schema inicial (sera reemplazado en V2)
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS concesiones (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            nombre_proveedor  TEXT    NOT NULL,
            fecha_inicio      TEXT,
            fecha_fin         TEXT,
            notas             TEXT,
            activa            INTEGER NOT NULL DEFAULT 1
        )
    )")) {
        qCritical() << "migrateV0toV1 concesiones:" << q.lastError().text();
        return false;
    }

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS productos_calculados (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            fecha             TEXT    NOT NULL DEFAULT (datetime('now','localtime')),
            precio_final      REAL    NOT NULL,
            costo             REAL    NOT NULL,
            comision          REAL    NOT NULL,
            iva_trasladado    REAL    NOT NULL,
            iva_acreditable   REAL    NOT NULL,
            iva_neto_sat      REAL    NOT NULL,
            escenario         TEXT    NOT NULL CHECK(escenario IN ('propio','concesion')),
            tiene_cfdi        INTEGER NOT NULL DEFAULT 0,
            nombre_proveedor  TEXT,
            nombre_vendedor   TEXT,
            concesion_id      INTEGER REFERENCES concesiones(id) ON DELETE SET NULL
        )
    )")) {
        qCritical() << "migrateV0toV1 productos_calculados:" << q.lastError().text();
        return false;
    }

    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_productos_fecha
            ON productos_calculados(fecha DESC)
    )");

    qDebug() << "Migracion V0->V1 completada.";
    return true;
}

// ---------------------------------------------------------------------------
// V1 -> V2: Alineacion con TlacuiaGCL + nombre_producto + isbn
//
// TlacuiaGCL data model (Python/SQLite):
//   grantingEmisor : id, nombre, contacto
//   Concesiones    : id, grantingEmisor_id, folio, fecha_recepcion,
//                    fecha_vencimiento, tipo_documento, status (calculado)
//   Productos      : id, concesion_id, cantidad, descripcion, isbn,
//                    pvp_unitario, precio_neto
//
// Mapeo en esta app:
//   concesiones.emisor_nombre    <- grantingEmisor.nombre
//   concesiones.folio            <- Concesiones.folio
//   concesiones.fecha_recepcion  <- Concesiones.fecha_recepcion
//   concesiones.fecha_vencimiento<- Concesiones.fecha_vencimiento
//   concesiones.tipo_documento   <- Concesiones.tipo_documento
//   productos_calculados.isbn    <- Productos.isbn
//   productos_calculados.precio_final  <- Productos.pvp_unitario (precio publico)
//   productos_calculados.costo         <- Productos.precio_neto  (precio neto)
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV1toV2() {
    QSqlQuery q(m_db);

    // -- productos_calculados: añadir nombre_producto e isbn --
    // SQLite permite ADD COLUMN con valor NULL por defecto en columnas existentes.
    auto addCol = [&](const QString& sql, const QString& desc) -> bool {
        if (!q.exec(sql)) {
            // Si ya existe la columna, SQLite retorna error "duplicate column name"
            // que se puede ignorar (idempotente).
            if (!q.lastError().text().contains("duplicate column name")) {
                qCritical() << desc << q.lastError().text();
                return false;
            }
        }
        return true;
    };

    if (!addCol("ALTER TABLE productos_calculados ADD COLUMN nombre_producto TEXT",
                "ADD nombre_producto:")) return false;
    if (!addCol("ALTER TABLE productos_calculados ADD COLUMN isbn TEXT",
                "ADD isbn:")) return false;

    // -- concesiones: migrar al schema compatible con TlacuiaGCL --
    // Crear tabla nueva con el schema completo, copiar datos existentes, renombrar.
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS concesiones_v2 (
            id                 INTEGER PRIMARY KEY AUTOINCREMENT,
            emisor_nombre      TEXT    NOT NULL,
            emisor_contacto    TEXT,
            folio              TEXT,
            fecha_recepcion    TEXT,
            fecha_vencimiento  TEXT,
            tipo_documento     TEXT    NOT NULL DEFAULT 'Factura'
                               CHECK(tipo_documento IN ('Factura','Nota de credito')),
            notas              TEXT,
            activa             INTEGER NOT NULL DEFAULT 1,
            created_at         TEXT    DEFAULT (datetime('now','localtime'))
        )
    )")) {
        qCritical() << "migrateV1toV2 crear concesiones_v2:" << q.lastError().text();
        return false;
    }

    // Copiar datos existentes (mapeo de columnas renombradas)
    if (!q.exec(R"(
        INSERT OR IGNORE INTO concesiones_v2
            (id, emisor_nombre, fecha_recepcion, fecha_vencimiento, notas, activa)
        SELECT id, nombre_proveedor, fecha_inicio, fecha_fin, notas, activa
        FROM concesiones
    )")) {
        qCritical() << "migrateV1toV2 copiar datos:" << q.lastError().text();
        return false;
    }

    if (!q.exec("DROP TABLE concesiones")) {
        qCritical() << "migrateV1toV2 drop old:" << q.lastError().text();
        return false;
    }
    if (!q.exec("ALTER TABLE concesiones_v2 RENAME TO concesiones")) {
        qCritical() << "migrateV1toV2 rename:" << q.lastError().text();
        return false;
    }

    // Indice para busqueda por vencimiento (relevante para alertas tipo TlacuiaGCL)
    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_concesiones_vencimiento
            ON concesiones(fecha_vencimiento ASC)
    )");

    qDebug() << "Migracion V1->V2 completada (schema TlacuiaGCL).";
    return true;
}

// ---------------------------------------------------------------------------
// V2 -> V3: tipo_producto ('libro' | 'papeleria')
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV2toV3() {
    QSqlQuery q(m_db);
    if (!q.exec("ALTER TABLE productos_calculados ADD COLUMN tipo_producto TEXT NOT NULL DEFAULT 'papeleria'")) {
        if (!q.lastError().text().contains("duplicate column name")) {
            qCritical() << "migrateV2toV3:" << q.lastError().text();
            return false;
        }
    }
    q.exec("CREATE INDEX IF NOT EXISTS idx_productos_tipo ON productos_calculados(tipo_producto)");
    qDebug() << "Migracion V2->V3 completada (tipo_producto).";
    return true;
}

// ---------------------------------------------------------------------------
// V3 -> V4: tabla emisores (distribuidores) + FK emisor_id en concesiones
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV3toV4() {
    QSqlQuery q(m_db);

    // 1. Crear tabla emisores
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS emisores (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            nombre_emisor   TEXT    NOT NULL,
            nombre_vendedor TEXT    NOT NULL DEFAULT '',
            telefono        TEXT,
            email           TEXT,
            notas           TEXT,
            created_at      TEXT DEFAULT (datetime('now','localtime'))
        )
    )")) {
        qCritical() << "migrateV3toV4 crear emisores:" << q.lastError().text();
        return false;
    }

    // 2. Migrar emisores texto plano existentes a la nueva tabla (best-effort)
    q.exec(R"(
        INSERT OR IGNORE INTO emisores (nombre_emisor)
        SELECT DISTINCT emisor_nombre
        FROM concesiones
        WHERE emisor_nombre IS NOT NULL AND emisor_nombre != ''
    )");

    // 3. Añadir columna emisor_id a concesiones (idempotente)
    if (!q.exec("ALTER TABLE concesiones ADD COLUMN emisor_id INTEGER REFERENCES emisores(id) ON DELETE SET NULL")) {
        if (!q.lastError().text().contains("duplicate column name")) {
            qCritical() << "migrateV3toV4 add emisor_id:" << q.lastError().text();
            return false;
        }
    }

    // 4. Vincular emisor_id con registros de texto existentes (best-effort)
    q.exec(R"(
        UPDATE concesiones
        SET emisor_id = (
            SELECT e.id FROM emisores e
            WHERE e.nombre_emisor = concesiones.emisor_nombre
            LIMIT 1
        )
        WHERE emisor_id IS NULL AND emisor_nombre IS NOT NULL AND emisor_nombre != ''
    )");

    // 5. Indices
    q.exec("CREATE INDEX IF NOT EXISTS idx_concesiones_emisor ON concesiones(emisor_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_emisores_nombre ON emisores(nombre_emisor)");

    qDebug() << "Migracion V3->V4 completada (emisores + FK).";
    return true;
}

// ---------------------------------------------------------------------------
// V4 -> V5: cantidad_recibida + cantidad_vendida en productos_calculados
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV4toV5() {
    QSqlQuery q(m_db);

    auto addCol = [&](const QString& sql, const QString& desc) -> bool {
        if (!q.exec(sql)) {
            if (!q.lastError().text().contains("duplicate column name")) {
                qCritical() << desc << q.lastError().text();
                return false;
            }
        }
        return true;
    };

    if (!addCol("ALTER TABLE productos_calculados ADD COLUMN cantidad_recibida INTEGER NOT NULL DEFAULT 1",
                "ADD cantidad_recibida:")) return false;
    if (!addCol("ALTER TABLE productos_calculados ADD COLUMN cantidad_vendida INTEGER NOT NULL DEFAULT 0",
                "ADD cantidad_vendida:")) return false;

    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_productos_concesion
            ON productos_calculados(concesion_id, cantidad_vendida)
    )");

    qDebug() << "Migracion V4->V5 completada (cantidad_recibida, cantidad_vendida).";
    return true;
}

// ---------------------------------------------------------------------------
// V5 -> V6: comision_pct en concesiones + tabla documentos_concesion
// ---------------------------------------------------------------------------
bool DatabaseManager::migrateV5toV6() {
    QSqlQuery q(m_db);

    // 1. comision_pct en concesiones (idempotente vía duplicate column guard)
    if (!q.exec("ALTER TABLE concesiones ADD COLUMN comision_pct REAL NOT NULL DEFAULT 30.0")) {
        if (!q.lastError().text().contains("duplicate column name")) {
            qCritical() << "migrateV5toV6 add comision_pct:" << q.lastError().text();
            return false;
        }
    }

    // 2. Tabla de documentos adjuntos por concesión
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS documentos_concesion (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            concesion_id  INTEGER NOT NULL REFERENCES concesiones(id) ON DELETE CASCADE,
            nombre        TEXT    NOT NULL,
            tipo          TEXT    NOT NULL,
            contenido     BLOB    NOT NULL,
            fecha_adjunto TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
        )
    )")) {
        qCritical() << "migrateV5toV6 crear documentos_concesion:" << q.lastError().text();
        return false;
    }

    q.exec(R"(
        CREATE INDEX IF NOT EXISTS idx_documentos_concesion
            ON documentos_concesion(concesion_id)
    )");

    qDebug() << "Migracion V5->V6 completada (comision_pct, documentos_concesion).";
    return true;
}

} // namespace Calculadora
