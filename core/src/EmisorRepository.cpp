#include "core/EmisorRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
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
    r.facturacion    = q.value("facturacion").isNull() ? true
                       : q.value("facturacion").toInt() != 0;
    r.createdAt      = q.value("created_at").toString();
    return r;
}

QList<EmisorRecord> EmisorRepository::findAll() const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM emisores ORDER BY nombre_emisor COLLATE NOCASE");
    QList<EmisorRecord> result;
    if (!q.exec()) {
        qCritical() << "EmisorRepo::findAll:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(mapRow(q));
    return result;
}

EmisorRecord EmisorRepository::findById(int64_t id) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM emisores WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    if (q.exec() && q.next()) return mapRow(q);
    return {};
}

int64_t EmisorRepository::save(const EmisorRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO emisores (nombre_emisor, nombre_vendedor, telefono, email, notas, facturacion)
        VALUES (:nombre, :vendedor, :tel, :email, :notas, :facturacion)
    )");
    auto optStr = [](const QString& s) -> QVariant {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };
    q.bindValue(":nombre",      r.nombreEmisor);
    q.bindValue(":vendedor",    r.nombreVendedor);
    q.bindValue(":tel",         optStr(r.telefono));
    q.bindValue(":email",       optStr(r.email));
    q.bindValue(":notas",       optStr(r.notas));
    q.bindValue(":facturacion", r.facturacion ? 1 : 0);
    if (!q.exec()) {
        qCritical() << "EmisorRepo::save:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool EmisorRepository::update(const EmisorRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        UPDATE emisores SET nombre_emisor=:nombre, nombre_vendedor=:vendedor,
            telefono=:tel, email=:email, notas=:notas, facturacion=:facturacion
        WHERE id=:id
    )");
    auto optStr = [](const QString& s) -> QVariant {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };
    q.bindValue(":nombre",      r.nombreEmisor);
    q.bindValue(":vendedor",    r.nombreVendedor);
    q.bindValue(":tel",         optStr(r.telefono));
    q.bindValue(":email",       optStr(r.email));
    q.bindValue(":notas",       optStr(r.notas));
    q.bindValue(":facturacion", r.facturacion ? 1 : 0);
    q.bindValue(":id",          QVariant::fromValue(static_cast<qlonglong>(r.id)));
    if (!q.exec()) {
        qCritical() << "EmisorRepo::update:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool EmisorRepository::remove(int64_t id) {
    QSqlQuery q(m_db.database());
    // Nullificar FK en concesiones antes de borrar
    q.prepare("UPDATE concesiones SET emisor_id = NULL WHERE emisor_id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    q.exec();
    q.prepare("DELETE FROM emisores WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    if (!q.exec()) {
        qCritical() << "EmisorRepo::remove:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace Calculadora
