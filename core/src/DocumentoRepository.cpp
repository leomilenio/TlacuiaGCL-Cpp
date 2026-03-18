#include "core/DocumentoRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace Calculadora {

DocumentoRepository::DocumentoRepository(DatabaseManager& dbManager)
    : m_db(dbManager)
{
}

int64_t DocumentoRepository::save(int64_t concesionId,
                                   const QString& nombre,
                                   const QString& tipo,
                                   const QByteArray& contenido)
{
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO documentos_concesion (concesion_id, nombre, tipo, contenido)
        VALUES (:cid, :nombre, :tipo, :contenido)
    )");
    q.bindValue(":cid",      QVariant::fromValue(static_cast<qlonglong>(concesionId)));
    q.bindValue(":nombre",   nombre);
    q.bindValue(":tipo",     tipo);
    q.bindValue(":contenido", contenido);
    if (!q.exec()) {
        qCritical() << "DocumentoRepo::save:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

QList<DocumentoRecord> DocumentoRepository::findByConcesion(int64_t concesionId) const {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT id, concesion_id, nombre, tipo, fecha_adjunto
        FROM documentos_concesion
        WHERE concesion_id = :cid
        ORDER BY id ASC
    )");
    q.bindValue(":cid", QVariant::fromValue(static_cast<qlonglong>(concesionId)));
    QList<DocumentoRecord> result;
    if (!q.exec()) {
        qCritical() << "DocumentoRepo::findByConcesion:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        DocumentoRecord r;
        r.id           = q.value("id").toLongLong();
        r.concesionId  = q.value("concesion_id").toLongLong();
        r.nombre       = q.value("nombre").toString();
        r.tipo         = q.value("tipo").toString();
        r.fechaAdjunto = q.value("fecha_adjunto").toString();
        result.append(r);
    }
    return result;
}

QByteArray DocumentoRepository::getContenido(int64_t docId) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT contenido FROM documentos_concesion WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(docId)));
    if (q.exec() && q.next())
        return q.value("contenido").toByteArray();
    return {};
}

bool DocumentoRepository::remove(int64_t docId) {
    QSqlQuery q(m_db.database());
    q.prepare("DELETE FROM documentos_concesion WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(docId)));
    if (!q.exec()) {
        qCritical() << "DocumentoRepo::remove:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace Calculadora
