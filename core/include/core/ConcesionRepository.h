#pragma once
// Modelo de Concesiones compatible con TlacuiaGCL (futuro port a C++).
//
// TlacuiaGCL (Python/PyQt5) define:
//   grantingEmisor : nombre, contacto
//   Concesiones    : folio, fecha_recepcion, fecha_vencimiento,
//                    tipo_documento, status (calculado en app layer)
//   Status logic   : "Vence pronto" si vence en <=14 dias, "Vencida", "Valido", "Pendiente"
//
// El campo concesion_id en productos_calculados apuntara a esta tabla cuando
// TlacuiaGCL sea migrado a C++ y ambas herramientas compartan base de datos.
#include <QList>
#include <QString>
#include <QDate>
#include <QSqlQuery>
#include <cstdint>

namespace Calculadora {

// Tipos de documento.
// Si el proveedor factura: Factura | NotaDeCredito
// Si el proveedor NO factura: NotaDeRemision | Otro
enum class TipoDocumentoConcesion {
    Factura,
    NotaDeCredito,
    NotaDeRemision,
    Otro
};

// Status calculado (no almacenado en DB) — logica identica a TlacuiaGCL
enum class ConcesionStatus {
    Valido,
    VencePronto,   // Vence en <= 14 dias
    Vencida,
    Pendiente      // Sin fecha de vencimiento definida
};

struct ConcesionRecord {
    int64_t  id                    = 0;
    int64_t  emisorId              = 0;       // FK a emisores.id
    QString  emisorNombre;                    // JOIN-filled al leer (o texto legado)
    QString  emisorNombreVendedor;            // JOIN-filled al leer
    bool     emisorFacturacion = true;        // JOIN-filled: true = proveedor emite CFDI
    QString  emisorContacto;                 // Legado — texto plano sin FK
    QString  folio;                          // numero de referencia del documento
    QString  fechaRecepcion;                 // ISO 8601 (YYYY-MM-DD)
    QString  fechaVencimiento;               // ISO 8601 — base para calculo de status
    TipoDocumentoConcesion tipoDocumento = TipoDocumentoConcesion::Factura;
    QString  notas;
    bool     activa            = true;
    double   comisionPct       = 30.0;  // % de comision acordada (default 30%)
    QString  createdAt;
    QString  folioDocumento;  // folio del documento de corte (asignado por FolioRepository)

    // Status calculado — replica la logica de TlacuiaGCL:
    // "Vence pronto" si dias_restantes <= 14
    [[nodiscard]] ConcesionStatus status() const;
    [[nodiscard]] int diasRestantes() const;   // Negativo si ya vencio
};

class DatabaseManager;

class ConcesionRepository {
public:
    explicit ConcesionRepository(DatabaseManager& dbManager);

    [[nodiscard]] QList<ConcesionRecord> findAll()                        const;
    [[nodiscard]] QList<ConcesionRecord> findActivas()                    const;
    [[nodiscard]] QList<ConcesionRecord> findVencenPronto(int dias = 14)  const;
    [[nodiscard]] ConcesionRecord        findById(int64_t id)             const;
    [[nodiscard]] int64_t                save(const ConcesionRecord& record);
    [[nodiscard]] bool                   update(const ConcesionRecord& record);
    [[nodiscard]] bool                   remove(int64_t id);
    [[nodiscard]] bool                   finalizar(int64_t id);
    // Numero de concesiones activas vinculadas a un emisor.
    [[nodiscard]] int                    countActiveByEmisor(int64_t emisorId) const;
    // Todas las concesiones de un emisor (activas + finalizadas).
    [[nodiscard]] QList<ConcesionRecord> findByEmisor(int64_t emisorId) const;
    // Solo las concesiones finalizadas (activa = 0) de un emisor, ordenadas por fecha DESC.
    [[nodiscard]] QList<ConcesionRecord> findFinalizadasByEmisor(int64_t emisorId) const;

    // Acceso al DatabaseManager subyacente (para crear repos auxiliares en dialogs).
    DatabaseManager& database() { return m_db; }

private:
    [[nodiscard]] ConcesionRecord mapRow(const QSqlQuery& query) const;
    DatabaseManager& m_db;
};

} // namespace Calculadora
