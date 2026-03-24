#pragma once
#include <QString>
#include <QSqlDatabase>
#include <QThread>

namespace Calculadora {

// NOTA DE THREAD SAFETY
// ---------------------
// DatabaseManager NO es thread-safe. Toda llamada a initialize(), database(),
// checkpointWal() y los métodos de repositorios derivados DEBE realizarse
// desde el mismo hilo que construyó la instancia (normalmente el hilo principal).
//
// Qt documenta que QSqlDatabase y QSqlQuery sólo pueden usarse desde el hilo
// que abrió la conexión. Usar database() desde otro hilo causará un crash o
// comportamiento indefinido.
//
// Si en el futuro se requieren operaciones en background (p.ej. exportación
// asíncrona), crear una conexión independiente con un DatabaseManager propio
// en ese hilo, o usar QMetaObject::invokeMethod() para despachar al hilo dueño.


// Versiones del schema:
//   1 — Schema inicial
//   2 — nombre_producto + isbn en productos_calculados;
//       concesiones alineada con modelo TlacuiaGCL
//   3 — tipo_producto en productos_calculados ('libro' | 'papeleria')
//   4 — tabla emisores (distribuidores) + FK emisor_id en concesiones
//   5 — cantidad_recibida + cantidad_vendida en productos_calculados (Sprint 2)
//   6 — comision_pct en concesiones (Sprint 5) + tabla documentos_concesion (Sprint 6)
//   7 — facturacion en emisores; tipo_documento ampliado ('Nota de remision','Otro')
//   8 — tabla app_config (datos e identidad visual de la libreria)
//   9 — folio_documento en concesiones; tabla folio_counters
static constexpr int SCHEMA_VERSION_CURRENT = 9;

class DatabaseManager {
public:
    explicit DatabaseManager(const QString& dbPath);
    ~DatabaseManager();

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool isOpen() const;

    // Consolida el WAL en el archivo principal de la DB.
    // Llamar después de operaciones de escritura masiva (migraciones, importaciones).
    void checkpointWal();

    QSqlDatabase&  database();
    const QString& connectionName() const;
    const QString& dbPath() const { return m_dbPath; }

private:
    [[nodiscard]] bool runMigrations();
    [[nodiscard]] bool migrateV0toV1();   // Schema inicial
    [[nodiscard]] bool migrateV1toV2();   // nombre_producto + concesiones TlacuiaGCL
    [[nodiscard]] bool migrateV2toV3();   // tipo_producto
    [[nodiscard]] bool migrateV3toV4();   // tabla emisores + FK en concesiones
    [[nodiscard]] bool migrateV4toV5();   // cantidad_recibida + cantidad_vendida
    [[nodiscard]] bool migrateV5toV6();   // comision_pct + documentos_concesion
    [[nodiscard]] bool migrateV6toV7();   // facturacion en emisores; tipo_documento ampliado
    [[nodiscard]] bool migrateV7toV8();   // tabla app_config (datos e identidad de la libreria)
    [[nodiscard]] bool migrateV8toV9();

    [[nodiscard]] int  getSchemaVersion();
    bool               setSchemaVersion(int version);

    QString       m_dbPath;
    QSqlDatabase  m_db;
    QString       m_connectionName;
    QThread*      m_ownerThread = nullptr;  // hilo que construyó la instancia
};

} // namespace Calculadora
