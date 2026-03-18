# Plan de Implementación: Sistema de Concesiones y Distribuidores

> **Para el LLM que ejecute este plan**: Este documento es autosuficiente. Contiene el estado actual
> del proyecto, las fuentes de referencia (TlacuiaGCL), y cada paso de implementación con código
> de ejemplo. Lee el documento completo antes de tocar cualquier archivo.

---

## 1. Contexto del Proyecto

**Calculadora para Elementos de Papelería** — Aplicación de escritorio C++20 + Qt 6 + SQLite.

- **Ruta del proyecto**: `calculadora-papeleria/`
- **Build**: `bash build_and_run.sh` (compila + tests GTest + lanza GUI)
- **Plataformas objetivo**: macOS ARM64 y Windows x86-64
- **DB path en macOS**: `~/Library/Application Support/SomosVoces/CalculadoraPapeleria/calculadora.db`

### 1.1 Estructura de Archivos Actual

```
calculadora-papeleria/
├── build_and_run.sh
├── CMakeLists.txt
├── core/
│   ├── CMakeLists.txt
│   ├── include/core/
│   │   ├── CalculationScenario.h    ← structs POD + enums (ivaAbsorbido añadido)
│   │   ├── PriceCalculator.h
│   │   ├── DatabaseManager.h        ← SCHEMA_VERSION_CURRENT = 3
│   │   ├── ProductoRepository.h
│   │   ├── ConcesionRepository.h    ← EXISTE (draft, sin emisor FK)
│   │   └── AppConfig.h
│   └── src/
│       ├── PriceCalculator.cpp
│       ├── DatabaseManager.cpp      ← migraciones V0→V1→V2→V3
│       ├── ProductoRepository.cpp
│       └── ConcesionRepository.cpp  ← EXISTE (findActivas, findVencenPronto, save)
├── capi/
│   ├── include/capi/calculadora_capi.h
│   └── src/calculadora_capi.cpp
├── app/
│   ├── CMakeLists.txt
│   ├── main.cpp                     ← setWindowIcon platform-aware
│   ├── resources/
│   │   ├── icons/icon.icns
│   │   ├── icons/icon.ico
│   │   └── icons.qrc
│   ├── include/app/
│   │   ├── MainWindow.h
│   │   ├── CalculatorWidget.h
│   │   ├── HistoryWidget.h
│   │   ├── HistoryTableModel.h
│   │   └── SaveDialog.h
│   └── src/
│       ├── MainWindow.cpp
│       ├── CalculatorWidget.cpp
│       ├── HistoryWidget.cpp
│       ├── HistoryTableModel.cpp
│       └── SaveDialog.cpp
└── tests/
    └── test_price_calculator.cpp
```

---

## 2. Schema SQLite Actual (V3)

### Tabla `concesiones` (creada en V2, actual)
```sql
CREATE TABLE concesiones (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    emisor_nombre      TEXT    NOT NULL,      -- nombre texto plano (SIN FK a emisores)
    emisor_contacto    TEXT,                  -- telefono/email texto plano
    folio              TEXT,
    fecha_recepcion    TEXT,                  -- ISO 8601 YYYY-MM-DD
    fecha_vencimiento  TEXT,                  -- ISO 8601 YYYY-MM-DD
    tipo_documento     TEXT NOT NULL DEFAULT 'Factura'
                       CHECK(tipo_documento IN ('Factura','Nota de credito')),
    notas              TEXT,
    activa             INTEGER NOT NULL DEFAULT 1,
    created_at         TEXT DEFAULT (datetime('now','localtime'))
);
```

### Tabla `productos_calculados` (actual, V3)
```sql
CREATE TABLE productos_calculados (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    fecha            TEXT    NOT NULL DEFAULT (datetime('now','localtime')),
    nombre_producto  TEXT,
    tipo_producto    TEXT NOT NULL DEFAULT 'papeleria',
    isbn             TEXT,
    precio_final     REAL    NOT NULL,
    costo            REAL    NOT NULL,
    comision         REAL    NOT NULL,
    iva_trasladado   REAL    NOT NULL,
    iva_acreditable  REAL    NOT NULL,
    iva_neto_sat     REAL    NOT NULL,
    escenario        TEXT NOT NULL CHECK(escenario IN ('propio','concesion')),
    tiene_cfdi       INTEGER NOT NULL DEFAULT 0,
    nombre_proveedor TEXT,
    nombre_vendedor  TEXT,
    concesion_id     INTEGER REFERENCES concesiones(id) ON DELETE SET NULL
);
```

**Nota importante**: `concesion_id` FK ya existe en `productos_calculados`. Es el enlace que
permite calcular cortes por concesión. Sin embargo, en la UI actual el `SaveDialog` no ofrece
la opción de seleccionar una concesión — ese vínculo se implementará en este sprint.

---

## 3. Estado Actual del Core de Concesiones

### `ConcesionRecord` (en `ConcesionRepository.h`)
```cpp
struct ConcesionRecord {
    int64_t  id                = 0;
    QString  emisorNombre;          // Texto plano, NO FK a tabla emisores
    QString  emisorContacto;        // Texto plano (telefono/email)
    QString  folio;
    QString  fechaRecepcion;        // YYYY-MM-DD
    QString  fechaVencimiento;      // YYYY-MM-DD
    TipoDocumentoConcesion tipoDocumento = TipoDocumentoConcesion::Factura;
    QString  notas;
    bool     activa            = true;
    QString  createdAt;

    [[nodiscard]] ConcesionStatus status() const;      // Calcula según fechaVencimiento
    [[nodiscard]] int diasRestantes() const;           // Negativo si ya venció
};
```

### Lógica de status (idéntica a TlacuiaGCL)
```cpp
ConcesionStatus ConcesionRecord::status() const {
    if (fechaVencimiento.isEmpty()) return ConcesionStatus::Pendiente;
    int dias = diasRestantes();
    if (dias < 0)   return ConcesionStatus::Vencida;
    if (dias <= 14) return ConcesionStatus::VencePronto;
    return ConcesionStatus::Valido;
}
```

### `ConcesionRepository` — métodos existentes
- `findActivas()` — SELECT WHERE activa=1, ORDER BY emisor_nombre
- `findVencenPronto(int dias = 14)` — BETWEEN date('now') AND date('now', '+N days')
- `save(record)` — INSERT, retorna id o -1

**Lo que falta en el repositorio**:
- `findAll()` — todas las concesiones (activas e inactivas)
- `findById(int64_t id)`
- `update(record)` — UPDATE para edición
- `remove(int64_t id)` — DELETE o marcar activa=0
- `finalizar(int64_t id)` — SET activa=0 (equivalente a `marcar_concesion_como_finalizada` de TlacuiaGCL)

---

## 4. Referencia: TlacuiaGCL (Python/PyQt5)

**TlacuiaGCL** es una aplicación hermana (mismo desarrollador) para gestión de concesiones en
librerías. Es la fuente de referencia para el diseño de datos y UI.

### 4.1 Schema de TlacuiaGCL (relevante para el port)

```sql
-- Emisor/distribuidor (separado de la concesión)
CREATE TABLE grantingEmisor (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre_emisor   TEXT NOT NULL,
    nombre_vendedor TEXT NOT NULL
);

CREATE TABLE Contacto (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    emisor_id           INTEGER NOT NULL REFERENCES grantingEmisor(id),
    numero              INTEGER,
    correo_electronico  TEXT
);

-- Concesión con FK al emisor
CREATE TABLE Concesiones (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    emisor_id         INTEGER NOT NULL REFERENCES grantingEmisor(id),
    tipo              TEXT CHECK(tipo IN ('Nota de credito', 'Factura')),
    folio             TEXT NOT NULL,
    fecha_recepcion   TEXT DEFAULT CURRENT_DATE,
    fecha_vencimiento TEXT NOT NULL,
    finalizada        BOOLEAN DEFAULT 0
);

-- Productos de la concesión (inventario, no cálculos)
CREATE TABLE Productos (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    concesion_id    INTEGER NOT NULL REFERENCES Concesiones(id),
    cantidad        INTEGER NOT NULL,
    descripcion     TEXT NOT NULL,
    isbn            TEXT,
    pvp_unitario    REAL,        -- precio público (lo que se cobra al cliente)
    precio_neto     REAL NOT NULL, -- precio del distribuidor
    precio_total    REAL NOT NULL,
    cantidad_vendida INTEGER DEFAULT 0
);
```

### 4.2 Lógica de Alertas de TlacuiaGCL
- Al iniciar: consultar concesiones con `fecha_vencimiento BETWEEN today AND today+14`
- Si existen → mostrar `AlertDialog` con lista (emisor, folio, días restantes)
- Ordenamiento: "Vence pronto" → "Valido" → "Vencida"
- Colores de badge: verde=#4CAF50, amarillo=#FFC107, rojo=#F44336

### 4.3 Flujo de Creación en TlacuiaGCL
1. Usuario abre `NewConcesionDialog`
2. Ingresa: nombre_emisor, nombre_vendedor (se crea registro en `grantingEmisor`)
3. Ingresa: tipo (Factura/Nota de crédito), folio, fecha_recepcion, fecha_vencimiento (o días)
4. Adjunta documentos (PDF/Excel) — **NO portar esta funcionalidad**
5. Guarda → crea emisor + concesión vinculada

### 4.4 Qué NO portar de TlacuiaGCL
- Documentos adjuntos (BLOB en DB) — demasiado complejo para el scope actual
- ReportesPDF — fuera de scope
- DocumentoProducto (junction table) — fuera de scope
- Extracción de PDF / análisis de congruencia — herramientas avanzadas, fuera de scope

---

## 5. Qué Construir: Port Superficial

### 5.1 Resumen Ejecutivo

| Componente | Acción | Archivos afectados |
|---|---|---|
| Tabla `emisores` | CREAR nueva (V4 migration) | `DatabaseManager.cpp/.h` |
| `EmisorRecord` + `EmisorRepository` | CREAR nuevos | 2 nuevos archivos core |
| Tabla `concesiones` | MIGRAR (añadir `emisor_id` FK) | `DatabaseManager.cpp` |
| `ConcesionRepository` | AMPLIAR (update, remove, finalizar, findById) | `ConcesionRepository.cpp` |
| `ConcesionRecord` | AMPLIAR (añadir `emisorId` y `nombreVendedor`) | `ConcesionRepository.h` |
| `CorteResult` struct | CREAR en ProductoRepository | `ProductoRepository.h/.cpp` |
| `EmisoresWidget` | CREAR nuevo (Tab "Distribuidores") | 2 nuevos archivos app |
| `ConcesionesWidget` | CREAR nuevo (Tab "Concesiones") | 2 nuevos archivos app |
| `NuevaConcesionDialog` | CREAR nuevo | 2 nuevos archivos app |
| `AlertDialog` | CREAR nuevo | 2 nuevos archivos app |
| `MainWindow` | MODIFICAR (tabs + startup alert) | `MainWindow.cpp/.h` |
| `SaveDialog` | MODIFICAR (combo concesión activa) | `SaveDialog.cpp/.h` |

---

## 6. Implementación Paso a Paso

### PASO 1: Migración V3→V4 — Tabla `emisores` y FK en `concesiones`

**Archivo**: `core/include/core/DatabaseManager.h`

Cambiar `SCHEMA_VERSION_CURRENT` de 3 a 4 y declarar el nuevo método:
```cpp
static constexpr int SCHEMA_VERSION_CURRENT = 4;
// añadir en private:
[[nodiscard]] bool migrateV3toV4();
```

**Archivo**: `core/src/DatabaseManager.cpp`

En `runMigrations()` añadir bloque para V4:
```cpp
if (current < 4) {
    if (!migrateV3toV4()) return false;
    setSchemaVersion(4);
}
```

Implementar `migrateV3toV4()`:
```cpp
bool DatabaseManager::migrateV3toV4() {
    QSqlQuery q(m_db);

    // 1. Crear tabla emisores (distribuidores)
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

    // 2. Migrar emisores existentes (texto plano) a la nueva tabla
    //    Usamos INSERT OR IGNORE para idempotencia
    if (!q.exec(R"(
        INSERT OR IGNORE INTO emisores (nombre_emisor, emisor_contacto_legacy)
        SELECT DISTINCT emisor_nombre, emisor_contacto
        FROM concesiones
        WHERE emisor_nombre IS NOT NULL AND emisor_nombre != ''
    )")) {
        // Si falla (columna no existe), continuar — puede que concesiones esté vacía
        qWarning() << "migrateV3toV4 migrar emisores:" << q.lastError().text();
    }

    // 3. Añadir columna emisor_id a concesiones
    if (!q.exec("ALTER TABLE concesiones ADD COLUMN emisor_id INTEGER REFERENCES emisores(id) ON DELETE SET NULL")) {
        if (!q.lastError().text().contains("duplicate column name")) {
            qCritical() << "migrateV3toV4 add emisor_id:" << q.lastError().text();
            return false;
        }
    }

    // 4. Actualizar emisor_id basándose en el texto existente (best-effort)
    q.exec(R"(
        UPDATE concesiones
        SET emisor_id = (
            SELECT e.id FROM emisores e
            WHERE e.nombre_emisor = concesiones.emisor_nombre
            LIMIT 1
        )
        WHERE emisor_id IS NULL AND emisor_nombre IS NOT NULL
    )");

    // 5. Índices
    q.exec("CREATE INDEX IF NOT EXISTS idx_concesiones_emisor ON concesiones(emisor_id)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_emisores_nombre ON emisores(nombre_emisor)");

    qDebug() << "Migracion V3->V4 completada (emisores tabla + FK).";
    return true;
}
```

**Nota**: La columna `emisor_nombre` se CONSERVA en `concesiones` como texto de respaldo. No se
elimina para mantener compatibilidad con registros sin `emisor_id`. Así la migración es segura
aunque la tabla esté vacía o tenga datos inconsistentes.

---

### PASO 2: `EmisorRecord` y `EmisorRepository`

**Archivo nuevo**: `core/include/core/EmisorRepository.h`
```cpp
#pragma once
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <cstdint>

namespace Calculadora {

struct EmisorRecord {
    int64_t id             = 0;
    QString nombreEmisor;       // nombre del distribuidor/editorial
    QString nombreVendedor;     // nombre del representante de ventas
    QString telefono;
    QString email;
    QString notas;
    QString createdAt;
};

class DatabaseManager;

class EmisorRepository {
public:
    explicit EmisorRepository(DatabaseManager& dbManager);

    [[nodiscard]] QList<EmisorRecord> findAll() const;
    [[nodiscard]] EmisorRecord        findById(int64_t id) const;
    [[nodiscard]] int64_t             save(const EmisorRecord& record);
    [[nodiscard]] bool                update(const EmisorRecord& record);
    [[nodiscard]] bool                remove(int64_t id);

private:
    [[nodiscard]] EmisorRecord mapRow(const QSqlQuery& q) const;
    DatabaseManager& m_db;
};

} // namespace Calculadora
```

**Archivo nuevo**: `core/src/EmisorRepository.cpp`
```cpp
#include "core/EmisorRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Calculadora {

EmisorRepository::EmisorRepository(DatabaseManager& dbManager)
    : m_db(dbManager) {}

EmisorRecord EmisorRepository::mapRow(const QSqlQuery& q) const {
    EmisorRecord r;
    r.id             = q.value("id").toLongLong();
    r.nombreEmisor   = q.value("nombre_emisor").toString();
    r.nombreVendedor = q.value("nombre_vendedor").toString();
    r.telefono       = q.value("telefono").toString();
    r.email          = q.value("email").toString();
    r.notas          = q.value("notas").toString();
    r.createdAt      = q.value("created_at").toString();
    return r;
}

QList<EmisorRecord> EmisorRepository::findAll() const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM emisores ORDER BY nombre_emisor COLLATE NOCASE");
    QList<EmisorRecord> result;
    if (!q.exec()) { qCritical() << "EmisorRepo::findAll:" << q.lastError().text(); return result; }
    while (q.next()) result.append(mapRow(q));
    return result;
}

EmisorRecord EmisorRepository::findById(int64_t id) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM emisores WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(id));
    if (q.exec() && q.next()) return mapRow(q);
    return {};
}

int64_t EmisorRepository::save(const EmisorRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO emisores (nombre_emisor, nombre_vendedor, telefono, email, notas)
        VALUES (:nombre, :vendedor, :tel, :email, :notas)
    )");
    q.bindValue(":nombre",   r.nombreEmisor);
    q.bindValue(":vendedor", r.nombreVendedor);
    q.bindValue(":tel",      r.telefono.isEmpty()  ? QVariant() : r.telefono);
    q.bindValue(":email",    r.email.isEmpty()     ? QVariant() : r.email);
    q.bindValue(":notas",    r.notas.isEmpty()     ? QVariant() : r.notas);
    if (!q.exec()) { qCritical() << "EmisorRepo::save:" << q.lastError().text(); return -1; }
    return q.lastInsertId().toLongLong();
}

bool EmisorRepository::update(const EmisorRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        UPDATE emisores SET nombre_emisor=:nombre, nombre_vendedor=:vendedor,
            telefono=:tel, email=:email, notas=:notas
        WHERE id=:id
    )");
    q.bindValue(":nombre",   r.nombreEmisor);
    q.bindValue(":vendedor", r.nombreVendedor);
    q.bindValue(":tel",      r.telefono.isEmpty()  ? QVariant() : r.telefono);
    q.bindValue(":email",    r.email.isEmpty()     ? QVariant() : r.email);
    q.bindValue(":notas",    r.notas.isEmpty()     ? QVariant() : r.notas);
    q.bindValue(":id",       QVariant::fromValue(r.id));
    if (!q.exec()) { qCritical() << "EmisorRepo::update:" << q.lastError().text(); return false; }
    return q.numRowsAffected() > 0;
}

bool EmisorRepository::remove(int64_t id) {
    QSqlQuery q(m_db.database());
    // Null out FK en concesiones antes de borrar (PRAGMA foreign_keys=ON ya lo haría,
    // pero lo hacemos explícito para claridad)
    q.prepare("UPDATE concesiones SET emisor_id = NULL WHERE emisor_id = :id");
    q.bindValue(":id", QVariant::fromValue(id));
    q.exec();
    // Borrar emisor
    q.prepare("DELETE FROM emisores WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(id));
    if (!q.exec()) { qCritical() << "EmisorRepo::remove:" << q.lastError().text(); return false; }
    return q.numRowsAffected() > 0;
}

} // namespace Calculadora
```

---

### PASO 3: Ampliar `ConcesionRecord` y `ConcesionRepository`

**Archivo**: `core/include/core/ConcesionRepository.h`

Añadir campos a `ConcesionRecord`:
```cpp
struct ConcesionRecord {
    int64_t  id                = 0;
    int64_t  emisorId          = 0;       // FK a emisores.id (NUEVO)
    QString  emisorNombre;                // JOIN-filled al leer (mantener por legibilidad UI)
    QString  emisorNombreVendedor;        // JOIN-filled (NUEVO)
    QString  emisorContacto;             // Legado, conservar por compatibilidad
    QString  folio;
    QString  fechaRecepcion;
    QString  fechaVencimiento;
    TipoDocumentoConcesion tipoDocumento = TipoDocumentoConcesion::Factura;
    QString  notas;
    bool     activa            = true;
    QString  createdAt;

    [[nodiscard]] ConcesionStatus status() const;
    [[nodiscard]] int diasRestantes() const;
};
```

Ampliar `ConcesionRepository`:
```cpp
class ConcesionRepository {
public:
    explicit ConcesionRepository(DatabaseManager& dbManager);

    [[nodiscard]] QList<ConcesionRecord> findAll() const;           // NUEVO
    [[nodiscard]] QList<ConcesionRecord> findActivas() const;
    [[nodiscard]] QList<ConcesionRecord> findVencenPronto(int dias = 14) const;
    [[nodiscard]] ConcesionRecord        findById(int64_t id) const; // NUEVO
    [[nodiscard]] int64_t                save(const ConcesionRecord& record);
    [[nodiscard]] bool                   update(const ConcesionRecord& record); // NUEVO
    [[nodiscard]] bool                   remove(int64_t id);                    // NUEVO
    [[nodiscard]] bool                   finalizar(int64_t id);                 // NUEVO

private:
    [[nodiscard]] ConcesionRecord mapRow(const QSqlQuery& query) const;
    DatabaseManager& m_db;
};
```

**Archivo**: `core/src/ConcesionRepository.cpp`

Actualizar `mapRow()` para incluir el JOIN con emisores:
```cpp
// La query base para mapRow debe hacer LEFT JOIN con emisores:
// SELECT c.*, e.nombre_emisor, e.nombre_vendedor
// FROM concesiones c
// LEFT JOIN emisores e ON c.emisor_id = e.id

ConcesionRecord ConcesionRepository::mapRow(const QSqlQuery& q) const {
    ConcesionRecord r;
    r.id                    = q.value("id").toLongLong();
    r.emisorId              = q.value("emisor_id").toLongLong();
    r.emisorNombre          = q.value("nombre_emisor").toString();      // del JOIN
    r.emisorNombreVendedor  = q.value("nombre_vendedor").toString();    // del JOIN
    r.emisorContacto        = q.value("emisor_contacto").toString();    // legado
    r.folio                 = q.value("folio").toString();
    r.fechaRecepcion        = q.value("fecha_recepcion").toString();
    r.fechaVencimiento      = q.value("fecha_vencimiento").toString();
    r.notas                 = q.value("notas").toString();
    r.activa                = q.value("activa").toInt() != 0;
    r.createdAt             = q.value("created_at").toString();
    QString td = q.value("tipo_documento").toString();
    r.tipoDocumento = (td == "Nota de credito")
                      ? TipoDocumentoConcesion::NotaDeCredito
                      : TipoDocumentoConcesion::Factura;
    return r;
}
```

**Query base** que deben usar todos los métodos `find*` (para que `mapRow` funcione):
```sql
SELECT c.*, e.nombre_emisor, e.nombre_vendedor
FROM concesiones c
LEFT JOIN emisores e ON c.emisor_id = e.id
```

Implementar los métodos nuevos:
```cpp
QList<ConcesionRecord> ConcesionRepository::findAll() const {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT c.*, e.nombre_emisor, e.nombre_vendedor
        FROM concesiones c
        LEFT JOIN emisores e ON c.emisor_id = e.id
        ORDER BY
            CASE WHEN c.activa=0 THEN 2
                 WHEN c.fecha_vencimiento < date('now') THEN 1
                 ELSE 0 END ASC,
            c.fecha_vencimiento ASC
    )");
    // ... exec + mapRow loop
}

bool ConcesionRepository::update(const ConcesionRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        UPDATE concesiones SET
            emisor_id=:eid, folio=:folio,
            fecha_recepcion=:frec, fecha_vencimiento=:fvenc,
            tipo_documento=:tipo, notas=:notas, activa=:activa
        WHERE id=:id
    )");
    // bindValues...
}

bool ConcesionRepository::finalizar(int64_t id) {
    QSqlQuery q(m_db.database());
    q.prepare("UPDATE concesiones SET activa = 0 WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(id));
    return q.exec() && q.numRowsAffected() > 0;
}

bool ConcesionRepository::remove(int64_t id) {
    QSqlQuery q(m_db.database());
    // Null out FK en productos_calculados
    q.prepare("UPDATE productos_calculados SET concesion_id=NULL WHERE concesion_id=:id");
    q.bindValue(":id", QVariant::fromValue(id));
    q.exec();
    q.prepare("DELETE FROM concesiones WHERE id=:id");
    q.bindValue(":id", QVariant::fromValue(id));
    return q.exec() && q.numRowsAffected() > 0;
}
```

---

### PASO 4: `CorteResult` en `ProductoRepository`

Un "corte de venta" es el cierre financiero de una concesión: suma de todos los
`productos_calculados` vinculados a esa concesión.

**Archivo**: `core/include/core/ProductoRepository.h`

Añadir struct y método:
```cpp
struct CorteResult {
    int64_t concesionId      = 0;
    int     cantidadRegistros = 0;
    double  totalPrecioFinal  = 0.0;
    double  totalCosto        = 0.0;   // Lo que se paga al proveedor
    double  totalComision     = 0.0;   // Lo que se queda la librería
    double  totalIvaTrasladado = 0.0;
    double  totalIvaAcreditable = 0.0;
    double  totalIvaNetoPagar  = 0.0;
    bool    isValid           = false;
};

// En la clase ProductoRepository:
[[nodiscard]] CorteResult calcularCorte(int64_t concesionId) const;
```

**Archivo**: `core/src/ProductoRepository.cpp`

Implementar con una sola query de agregación:
```cpp
CorteResult ProductoRepository::calcularCorte(int64_t concesionId) const {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT
            COUNT(*)            AS cnt,
            SUM(precio_final)   AS sum_precio_final,
            SUM(costo)          AS sum_costo,
            SUM(comision)       AS sum_comision,
            SUM(iva_trasladado) AS sum_iva_trasladado,
            SUM(iva_acreditable)AS sum_iva_acreditable,
            SUM(iva_neto_sat)   AS sum_iva_neto
        FROM productos_calculados
        WHERE concesion_id = :cid
    )");
    q.bindValue(":cid", QVariant::fromValue(concesionId));
    CorteResult r;
    r.concesionId = concesionId;
    if (!q.exec() || !q.next()) return r;
    r.cantidadRegistros   = q.value("cnt").toInt();
    r.totalPrecioFinal    = q.value("sum_precio_final").toDouble();
    r.totalCosto          = q.value("sum_costo").toDouble();
    r.totalComision       = q.value("sum_comision").toDouble();
    r.totalIvaTrasladado  = q.value("sum_iva_trasladado").toDouble();
    r.totalIvaAcreditable = q.value("sum_iva_acreditable").toDouble();
    r.totalIvaNetoPagar   = q.value("sum_iva_neto").toDouble();
    r.isValid             = (r.cantidadRegistros > 0);
    return r;
}
```

---

### PASO 5: `AlertDialog`

Equivalente al `AlertDialog` de TlacuiaGCL, adaptado a Qt Widgets nativo (sin hardcodear colores).

**Archivo**: `app/include/app/AlertDialog.h`
```cpp
#pragma once
#include <QDialog>
#include "core/ConcesionRepository.h"

namespace App {

class AlertDialog : public QDialog {
    Q_OBJECT
public:
    explicit AlertDialog(const QList<Calculadora::ConcesionRecord>& concesiones,
                         QWidget* parent = nullptr);
};

} // namespace App
```

**Archivo**: `app/src/AlertDialog.cpp`
```cpp
#include "app/AlertDialog.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

namespace App {

AlertDialog::AlertDialog(const QList<Calculadora::ConcesionRecord>& concesiones, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Alertas de Concesiones");
    setMinimumWidth(500);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* titleLabel = new QLabel(
        QString("<b>%1 concesion(es) próximas a vencer o vencidas</b>")
            .arg(concesiones.size()));
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto* list = new QListWidget();
    list->setAlternatingRowColors(true);
    list->setFocusPolicy(Qt::NoFocus);

    for (const auto& c : concesiones) {
        int dias = c.diasRestantes();
        QString statusText;
        if (dias < 0)
            statusText = QString("VENCIDA hace %1 días").arg(-dias);
        else if (dias == 0)
            statusText = "Vence HOY";
        else
            statusText = QString("Vence en %1 días").arg(dias);

        QString text = QString("%1  —  Folio: %2  —  %3")
            .arg(c.emisorNombre.isEmpty() ? "(Sin emisor)" : c.emisorNombre,
                 c.folio.isEmpty() ? "(Sin folio)" : c.folio,
                 statusText);
        list->addItem(text);
    }

    layout->addWidget(list);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* btnOk = new QPushButton("Entendido");
    btnOk->setDefault(true);
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(btnOk);
    layout->addLayout(btnRow);
}

} // namespace App
```

---

### PASO 6: `NuevaConcesionDialog`

Diálogo para crear y editar concesiones. Incluye selector de emisor existente O campo para crear uno nuevo al vuelo.

**Archivo**: `app/include/app/NuevaConcesionDialog.h`
```cpp
#pragma once
#include <QDialog>
#include "core/ConcesionRepository.h"
#include "core/EmisorRepository.h"

class QComboBox;
class QLineEdit;
class QDateEdit;
class QRadioButton;
class QSpinBox;
class QGroupBox;

namespace Calculadora {
class DatabaseManager;
}

namespace App {

class NuevaConcesionDialog : public QDialog {
    Q_OBJECT
public:
    // Para crear nueva concesión
    explicit NuevaConcesionDialog(Calculadora::EmisorRepository& emisorRepo,
                                  QWidget* parent = nullptr);
    // Para editar concesión existente
    explicit NuevaConcesionDialog(const Calculadora::ConcesionRecord& record,
                                  Calculadora::EmisorRepository& emisorRepo,
                                  QWidget* parent = nullptr);

    [[nodiscard]] Calculadora::ConcesionRecord result() const;

private slots:
    void onEmisorSelectionChanged(int index);
    void onFechaToggled();
    void onAcceptClicked();

private:
    void setupUi();
    void loadEmisores();
    void populateFrom(const Calculadora::ConcesionRecord& record);

    Calculadora::EmisorRepository& m_emisorRepo;
    bool m_editMode = false;
    int64_t m_editId = 0;

    // Widgets emisor
    QComboBox*    m_cmbEmisor       = nullptr;  // Lista emisores existentes + "Nuevo..."
    QGroupBox*    m_grpNuevoEmisor  = nullptr;  // Visible solo si se selecciona "Nuevo..."
    QLineEdit*    m_txtNombreEmisor = nullptr;
    QLineEdit*    m_txtNombreVendedor = nullptr;
    QLineEdit*    m_txtTelefono     = nullptr;
    QLineEdit*    m_txtEmail        = nullptr;

    // Widgets concesión
    QComboBox*    m_cmbTipo         = nullptr;  // Factura / Nota de credito
    QLineEdit*    m_txtFolio        = nullptr;
    QDateEdit*    m_dateFechaRec    = nullptr;
    QRadioButton* m_rdDias          = nullptr;
    QRadioButton* m_rdFechaExacta   = nullptr;
    QSpinBox*     m_spnDias         = nullptr;
    QDateEdit*    m_dateFechaVenc   = nullptr;
    QLineEdit*    m_txtNotas        = nullptr;
};

} // namespace App
```

**Lógica clave de `NuevaConcesionDialog`**:
- El combo de emisores tiene como primer item "(Nuevo emisor...)" con `itemData(-1)`
- Al seleccionar "Nuevo emisor", aparece el `QGroupBox` de campos de nuevo emisor
- Al seleccionar un emisor existente, se oculta ese groupbox
- El toggle de fecha (días vs fecha exacta) funciona igual que en TlacuiaGCL:
  - Por días: calcula `fecha_vencimiento = fecha_recepcion + N días` al guardar
  - Por fecha exacta: usa el `QDateEdit` directamente
- `onAcceptClicked()` valida: folio no vacío; si nuevo emisor, nombre no vacío

---

### PASO 7: `EmisoresWidget` (Tab "Distribuidores")

CRUD simple de emisores. Layout: tabla arriba, formulario de edición abajo (o panel derecho).

**Archivo**: `app/include/app/EmisoresWidget.h`
```cpp
#pragma once
#include <QWidget>
#include "core/EmisorRepository.h"

class QTableView;
class QPushButton;
class QStandardItemModel;

namespace App {

class EmisoresWidget : public QWidget {
    Q_OBJECT
public:
    explicit EmisoresWidget(Calculadora::EmisorRepository& repo, QWidget* parent = nullptr);

    void refresh();

private slots:
    void onNuevoClicked();
    void onEditarClicked();
    void onEliminarClicked();

private:
    void setupUi();
    void setupConnections();

    Calculadora::EmisorRepository& m_repo;
    QTableView*        m_tableView  = nullptr;
    QStandardItemModel* m_model     = nullptr;
    QPushButton*       m_btnNuevo   = nullptr;
    QPushButton*       m_btnEditar  = nullptr;
    QPushButton*       m_btnEliminar = nullptr;
};

} // namespace App
```

**Columnas de la tabla**:
`ID (oculto) | Nombre Distribuidor | Vendedor | Teléfono | Email`

**Lógica**:
- "Nuevo" → abre `EmisorFormDialog` (diálogo simple con los 4 campos)
- "Editar" → abre el mismo dialog pre-rellenado con la fila seleccionada
- "Eliminar" → confirma con `QMessageBox::question`, llama `repo.remove(id)`, muestra
  advertencia si el emisor tiene concesiones vinculadas (query COUNT antes de borrar)

---

### PASO 8: `ConcesionesWidget` (Tab "Concesiones")

Es el corazón del port. Replica la UI de TlacuiaGCL: lista con badges de color + detalle + acciones.

**Archivo**: `app/include/app/ConcesionesWidget.h`
```cpp
#pragma once
#include <QWidget>
#include "core/ConcesionRepository.h"
#include "core/EmisorRepository.h"
#include "core/ProductoRepository.h"

class QListWidget;
class QPushButton;
class QLabel;
class QGroupBox;
class QTableView;
class QStandardItemModel;

namespace App {

class ConcesionesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConcesionesWidget(Calculadora::ConcesionRepository& concesionRepo,
                               Calculadora::EmisorRepository& emisorRepo,
                               Calculadora::ProductoRepository& productoRepo,
                               QWidget* parent = nullptr);

    void refresh();

private slots:
    void onSelectionChanged();
    void onNuevaClicked();
    void onEditarClicked();
    void onFinalizarClicked();
    void onEliminarClicked();
    void onVerCorteClicked();

private:
    void setupUi();
    void setupConnections();
    void updateDetailPanel(const Calculadora::ConcesionRecord& record);
    void addConcesionItem(const Calculadora::ConcesionRecord& record);

    Calculadora::ConcesionRepository& m_concesionRepo;
    Calculadora::EmisorRepository&    m_emisorRepo;
    Calculadora::ProductoRepository&  m_productoRepo;

    // Panel izquierdo — lista
    QListWidget*  m_listWidget   = nullptr;
    QPushButton*  m_btnNueva     = nullptr;
    QPushButton*  m_btnEditar    = nullptr;
    QPushButton*  m_btnFinalizar = nullptr;
    QPushButton*  m_btnEliminar  = nullptr;

    // Panel derecho — detalle
    QGroupBox*   m_detailGroup   = nullptr;
    QLabel*      m_lblEmisor     = nullptr;
    QLabel*      m_lblFolio      = nullptr;
    QLabel*      m_lblTipo       = nullptr;
    QLabel*      m_lblRecepcion  = nullptr;
    QLabel*      m_lblVencimiento = nullptr;
    QLabel*      m_lblStatus     = nullptr;
    QLabel*      m_lblNotas      = nullptr;

    // Panel derecho — productos vinculados
    QTableView*         m_productosView  = nullptr;
    QStandardItemModel* m_productosModel = nullptr;
    QPushButton*        m_btnVerCorte    = nullptr;
};

} // namespace App
```

**Layout de `ConcesionesWidget`**:
```
┌─────────────────────────────────────────────────────────────────┐
│  [QSplitter horizontal]                                         │
│  ┌────────────────────┐  ┌──────────────────────────────────┐  │
│  │ Lista concesiones  │  │ Panel de detalle                 │  │
│  │                    │  │  Emisor:      ACME Editorial     │  │
│  │ [ACME  F-001 ●Val] │  │  Vendedor:    Juan Pérez        │  │
│  │ [Beta  F-002 ●Venc]│  │  Folio:       F-001             │  │
│  │ [Gamma F-003 ●Ven] │  │  Tipo:        Factura           │  │
│  │                    │  │  Recepción:   2026-01-15        │  │
│  │                    │  │  Vencimiento: 2026-03-15        │  │
│  │                    │  │  Status:      [● Válido]        │  │
│  │                    │  │  ────────────────────────────── │  │
│  │                    │  │  Productos vinculados (N)       │  │
│  │                    │  │  [tabla mini de productos]      │  │
│  │                    │  │                                  │  │
│  │                    │  │           [Ver Corte]            │  │
│  └────────────────────┘  └──────────────────────────────────┘  │
│  [Nueva] [Editar] [Finalizar] [Eliminar]                        │
└─────────────────────────────────────────────────────────────────┘
```

**Badges de status** (usando `QLabel` con stylesheet):
```cpp
// Colores usando palette-aware aproximado:
// No usar colores hardcodeados en dark/light. Usar los mismos colores semánticos
// de TlacuiaGCL pero como QPalette highlight roles donde sea posible.
// Para los badges de estado, un QLabel con objectName + stylesheet es aceptable
// porque son indicadores semánticos (verde=bueno, amarillo=advertencia, rojo=error)
// que deben ser reconocibles independientemente del tema.

QString badgeStyle(Calculadora::ConcesionStatus status) {
    switch (status) {
    case ConcesionStatus::Valido:
        return "background:#4CAF50; color:white; padding:2px 6px; border-radius:3px;";
    case ConcesionStatus::VencePronto:
        return "background:#FFC107; color:black; padding:2px 6px; border-radius:3px;";
    case ConcesionStatus::Vencida:
        return "background:#F44336; color:white; padding:2px 6px; border-radius:3px;";
    default:
        return "background:#9E9E9E; color:white; padding:2px 6px; border-radius:3px;";
    }
}
```

**Panel de detalle — productos vinculados**:
Query para llenar la mini-tabla:
```sql
SELECT nombre_producto, tipo_producto, precio_final, costo, comision, fecha
FROM productos_calculados
WHERE concesion_id = :cid
ORDER BY fecha DESC
```

**Botón "Ver Corte"**: llama `productoRepo.calcularCorte(concesionId)` y muestra
un `QDialog` sencillo con el resumen:
```
┌──────────────────────────────────────────────┐
│  Corte de Concesión — ACME Editorial F-001   │
│  ────────────────────────────────────────    │
│  Registros:         12                       │
│  Total Precio Final: $1,245.60               │
│  Total Costo:        $600.00  ← al proveedor │
│  Total Comisión:     $360.00  ← a la librería│
│  ────────────────────────────────────────    │
│  IVA Trasladado:     $172.45                 │
│  IVA Acreditable:    $83.20                  │
│  IVA Neto a SAT:     $89.25                  │
│                                     [Cerrar] │
└──────────────────────────────────────────────┘
```

---

### PASO 9: Modificar `SaveDialog` — Vincular producto a concesión

Al guardar un cálculo, el usuario debería poder opcionalmente vincularlo a una concesión activa.

**Archivo**: `app/include/app/SaveDialog.h`

Añadir:
```cpp
// Dependencia nueva
#include "core/ConcesionRepository.h"
// Constructor actualizado:
explicit SaveDialog(double precioFinal,
                    Calculadora::ConcesionRepository& concesionRepo,
                    QWidget* parent = nullptr);
// Getter nuevo:
[[nodiscard]] std::optional<int64_t> concesionId() const;
// Miembro nuevo:
QComboBox* m_cmbConcesion = nullptr;
```

**En `setupUi()`**: añadir sección "Concesión (opcional)" con un `QComboBox` que lista
concesiones activas: `"(Ninguna)"` como primer item, luego `"ACME F-001 (Válido)"`, etc.

**En `CalculatorWidget::onSaveClicked()`**: pasar `m_concesionRepo` al `SaveDialog` y
leer `dlg.concesionId()` para asignarlo a `record.concesionId`.

**Nota**: El campo `concesionId` ya existe en `ProductoRecord` como `std::optional<int64_t>`.

---

### PASO 10: Modificar `MainWindow` — Nuevas tabs + startup alert

**Archivo**: `app/include/app/MainWindow.h`

Añadir includes y miembros:
```cpp
#include "core/EmisorRepository.h"
// Miembros nuevos:
std::unique_ptr<Calculadora::EmisorRepository>    m_emisorRepo;
std::unique_ptr<Calculadora::ConcesionRepository> m_concesionRepo;
// Widgets nuevos:
App::ConcesionesWidget* m_concesionesTab  = nullptr;
App::EmisoresWidget*    m_emisoresTab     = nullptr;
```

**Archivo**: `app/src/MainWindow.cpp`

En `initDatabase()`: inicializar los nuevos repositorios.

En `setupCentralWidget()`: añadir dos pestañas nuevas:
```cpp
m_concesionesTab = new ConcesionesWidget(*m_concesionRepo, *m_emisorRepo, *m_productoRepo, m_tabs);
m_emisoresTab    = new EmisoresWidget(*m_emisorRepo, m_tabs);
m_tabs->addTab(m_calculatorTab,   "Calculadora");
m_tabs->addTab(m_historyTab,      "Historial");
m_tabs->addTab(m_concesionesTab,  "Concesiones");
m_tabs->addTab(m_emisoresTab,     "Distribuidores");
```

**Startup alert** en `MainWindow::MainWindow()` (después de `show()`):
```cpp
// Verificar concesiones por vencer al iniciar (como TlacuiaGCL)
auto porVencer = m_concesionRepo->findVencenPronto(14);
// También incluir las ya vencidas en el alert
auto vencidas = m_concesionRepo->findAll();  // filtrar activas vencidas
QList<Calculadora::ConcesionRecord> alertas;
for (auto& c : porVencer) alertas.append(c);
for (auto& c : vencidas) {
    if (c.activa && c.status() == Calculadora::ConcesionStatus::Vencida)
        alertas.append(c);
}
if (!alertas.isEmpty()) {
    AlertDialog dlg(alertas, this);
    dlg.exec();
}
```

---

### PASO 11: Actualizar `CMakeLists.txt` del app target

Añadir los nuevos archivos fuente a `add_executable(CalculadoraPapeleria ...)`:
```cmake
include/app/AlertDialog.h
include/app/NuevaConcesionDialog.h
include/app/EmisoresWidget.h
include/app/ConcesionesWidget.h
src/AlertDialog.cpp
src/NuevaConcesionDialog.cpp
src/EmisoresWidget.cpp
src/ConcesionesWidget.cpp
```

Y en `core/CMakeLists.txt`, añadir `EmisorRepository` a `target_sources` de `core_lib`:
```cmake
src/EmisorRepository.cpp
```

---

## 7. Notas Importantes para el LLM Ejecutor

### 7.1 Patrón existente de repositorios — SEGUIR ESTE PATRÓN
Todos los repositorios del proyecto siguen el mismo patrón:
1. Reciben `DatabaseManager&` en constructor (no crean su propia conexión)
2. Usan `m_db.database()` para obtener la `QSqlDatabase&`
3. `mapRow(const QSqlQuery&)` convierte una fila a struct
4. Retornan `-1` en `save()` si falla (no lanzan excepciones)
5. Retornan `false` en `update()`/`remove()` si falla

### 7.2 Migración de DB — REGLA CRÍTICA
- Nunca modificar migraciones anteriores (V0-V3)
- Solo añadir `migrateV3toV4()` y actualizar `SCHEMA_VERSION_CURRENT` a 4
- Las migraciones deben ser idempotentes (ignorar "duplicate column name")
- Usar el patrón CREATE_NEW / INSERT_FROM / DROP / RENAME para restructuraciones complejas

### 7.3 MOC de Qt — REGLA CRÍTICA
Todos los headers con `Q_OBJECT` deben estar en la lista de fuentes del `add_executable()` en
`CMakeLists.txt`. Si se añade un widget nuevo con `Q_OBJECT` y no se añade su `.h` al cmake,
el linker fallará con error de "undefined reference to vtable". Ver la lista existente en
`app/CMakeLists.txt` como referencia.

### 7.4 La lógica de status ya está implementada en C++
`ConcesionRecord::status()` y `diasRestantes()` ya replican exactamente el `_calcular_status()`
de TlacuiaGCL. No reescribir esta lógica.

### 7.5 Stylesheet para badges de status
Los colores semánticos (verde/amarillo/rojo) para los badges son una excepción aceptable a la
regla "no hardcodear colores" porque son indicadores semánticos estándar (como los semáforos).
Usar los mismos valores que TlacuiaGCL: `#4CAF50`, `#FFC107`, `#F44336`.

### 7.6 El `QSplitter` para `ConcesionesWidget`
Usar `QSplitter(Qt::Horizontal)` para dividir la lista y el panel de detalle. Esto da al usuario
control sobre el ancho de cada panel. `QSplitter` está en `<QSplitter>`.

### 7.7 `QStandardItemModel` para tablas de solo lectura
Para las mini-tablas (distribuidores, productos en detalle de concesión), usar
`QStandardItemModel` directamente en lugar de crear `QAbstractTableModel` personalizado,
ya que son vistas de solo lectura sin lógica especial.

### 7.8 Verificar build en cada paso
Ejecutar `bash build_and_run.sh` al terminar cada paso. Si falla, corregir antes de avanzar
al siguiente paso. Los 10 tests unitarios deben seguir pasando en todos los pasos.

### 7.9 Concesión en `SaveDialog` — opcional
La vinculación de productos a concesiones en `SaveDialog` es la funcionalidad que CIERRA el
ciclo: producto calculado → guardado → vinculado a concesión → aparece en corte de venta.
Es el paso más importante del flujo completo. No omitirlo.

### 7.10 Orden de implementación es el orden de dependencias
- `EmisorRepository` debe existir antes de `ConcesionesWidget` (la usa)
- `NuevaConcesionDialog` debe existir antes de `ConcesionesWidget` (la lanza)
- `ConcesionesWidget` debe existir antes de `MainWindow` (es una pestaña)
- La migración V4 debe ir antes de todo lo demás (los repos la necesitan)

---

## 8. Verificación Final

Al terminar todos los pasos, verificar:

```bash
bash build_and_run.sh
```

Checklist manual en la GUI:
1. **Distribuidores tab**: Crear un distribuidor "ACME Editorial" con vendedor "Juan Pérez"
2. **Concesiones tab**: Crear concesión → seleccionar ACME → folio "F-001" → 30 días
3. **Calculadora tab**: Calcular una concesión → Guardar → en SaveDialog seleccionar F-001
4. **Concesiones tab**: Seleccionar F-001 → panel derecho muestra el producto → "Ver Corte"
5. **Corte**: Muestra el desglose (1 registro, costo/comisión/IVA del producto guardado)
6. **Reiniciar la app**: Si F-001 vence en ≤14 días → debe aparecer el `AlertDialog` al inicio

---

## 9. Referencia de Archivos del Proyecto Original (TlacuiaGCL)

- Schema completo: `app/models/database.py` (clase `ConcesionesDB`)
- Diálogo de nueva concesión: `app/views/dialogs/concession_dialog.py` (clase `NewConcesionDialog`)
- Alert dialog: `app/views/dialogs/alert_dialog.py` (clase `AlertDialog`)
- Item de lista con badges: `app/views/components/concession_item.py` (clase `ConcesionItem`)
- Repo: `https://github.com/leomilenio/TlacuiaGCL`
