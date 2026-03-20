#pragma once
#include <QString>
#include <cstdint>

namespace Calculadora {

class DatabaseManager;
enum class TipoDocumentoConcesion;

class FolioRepository {
public:
    explicit FolioRepository(DatabaseManager& dbManager);

    // Devuelve el folio de documento de un corte (genera uno la primera vez, idempotente).
    // El tipo solo se usa si es la primera generacion del folio.
    QString getFolioCorte(int64_t concesionId, TipoDocumentoConcesion tipo);

    // Genera un nuevo folio de reporte interno (RI000-A, RI001-A, ..., RI999-A, RI000-B...).
    // Cada llamada produce un folio unico e incremental.
    QString generarFolioRI();

private:
    DatabaseManager& m_db;
};

} // namespace Calculadora
