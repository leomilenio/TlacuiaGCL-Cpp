#pragma once
#include <QString>
#include <QSqlDatabase>

namespace Calculadora {

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
};

} // namespace Calculadora
