#pragma once
#include <QList>
#include <QString>
#include <QByteArray>
#include <cstdint>

namespace Calculadora {

struct DocumentoRecord {
    int64_t   id           = 0;
    int64_t   concesionId  = 0;
    QString   nombre;          // nombre del archivo (sin ruta)
    QString   tipo;            // "PDF" | "Excel"
    QString   fechaAdjunto;
    // contenido NO se carga con findByConcesion para no llenar memoria;
    // usa DocumentoRepository::getContenido(id) cuando lo necesites
};

class DatabaseManager;

class DocumentoRepository {
public:
    explicit DocumentoRepository(DatabaseManager& dbManager);

    // Guarda el blob; devuelve el id asignado o -1 en error
    [[nodiscard]] int64_t save(int64_t concesionId,
                               const QString& nombre,
                               const QString& tipo,
                               const QByteArray& contenido);

    // Devuelve metadatos (sin contenido) para todos los documentos de una concesion
    [[nodiscard]] QList<DocumentoRecord> findByConcesion(int64_t concesionId) const;

    // Carga el blob de un documento específico
    [[nodiscard]] QByteArray getContenido(int64_t docId) const;

    [[nodiscard]] bool remove(int64_t docId);

private:
    DatabaseManager& m_db;
};

} // namespace Calculadora
