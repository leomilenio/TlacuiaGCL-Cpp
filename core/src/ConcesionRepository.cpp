#include "core/ConcesionRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QVariant>
#include <QDebug>

namespace Calculadora {

// ---------------------------------------------------------------------------
// ConcesionRecord — status calculado (logica identica a TlacuiaGCL)
// ---------------------------------------------------------------------------

int ConcesionRecord::diasRestantes() const {
    if (fechaVencimiento.isEmpty()) return 0;
    QDate venc = QDate::fromString(fechaVencimiento, Qt::ISODate);
    if (!venc.isValid()) return 0;
    return QDate::currentDate().daysTo(venc);
}

ConcesionStatus ConcesionRecord::status() const {
    if (fechaVencimiento.isEmpty()) return ConcesionStatus::Pendiente;
    int dias = diasRestantes();
    if (dias < 0)   return ConcesionStatus::Vencida;
    if (dias <= 14) return ConcesionStatus::VencePronto;
    return ConcesionStatus::Valido;
}

// ---------------------------------------------------------------------------
// ConcesionRepository
// ---------------------------------------------------------------------------

// Query base — incluye LEFT JOIN con emisores para llenar nombreEmisor/Vendedor/Facturacion
static const char* BASE_QUERY = R"(
    SELECT c.*, e.nombre_emisor, e.nombre_vendedor, e.facturacion AS emisor_facturacion
    FROM concesiones c
    LEFT JOIN emisores e ON c.emisor_id = e.id
)";

ConcesionRepository::ConcesionRepository(DatabaseManager& dbManager)
    : m_db(dbManager)
{
}

ConcesionRecord ConcesionRepository::mapRow(const QSqlQuery& q) const {
    ConcesionRecord r;
    r.id                   = q.value("id").toLongLong();
    r.emisorId             = q.value("emisor_id").toLongLong();
    // Prefer JOIN-filled nombre; fall back to legacy text column
    QString joinNombre     = q.value("nombre_emisor").toString();
    r.emisorNombre         = joinNombre.isEmpty()
                             ? q.value("emisor_nombre").toString()
                             : joinNombre;
    r.emisorNombreVendedor = q.value("nombre_vendedor").toString();
    r.emisorFacturacion    = q.value("emisor_facturacion").isNull() ? true
                             : q.value("emisor_facturacion").toInt() != 0;
    r.emisorContacto       = q.value("emisor_contacto").toString();
    r.folio                = q.value("folio").toString();
    r.fechaRecepcion       = q.value("fecha_recepcion").toString();
    r.fechaVencimiento     = q.value("fecha_vencimiento").toString();
    r.notas                = q.value("notas").toString();
    r.activa               = q.value("activa").toInt() != 0;
    r.comisionPct          = q.value("comision_pct").isNull()
                             ? 30.0 : q.value("comision_pct").toDouble();
    r.createdAt            = q.value("created_at").toString();

    QString td = q.value("tipo_documento").toString();
    if      (td == "Nota de credito")  r.tipoDocumento = TipoDocumentoConcesion::NotaDeCredito;
    else if (td == "Nota de remision") r.tipoDocumento = TipoDocumentoConcesion::NotaDeRemision;
    else if (td == "Otro")             r.tipoDocumento = TipoDocumentoConcesion::Otro;
    else                               r.tipoDocumento = TipoDocumentoConcesion::Factura;
    return r;
}

QList<ConcesionRecord> ConcesionRepository::findAll() const {
    QSqlQuery q(m_db.database());
    QString sql = QString(BASE_QUERY) + R"(
        ORDER BY
            CASE WHEN c.activa=0 THEN 2
                 WHEN c.fecha_vencimiento < date('now') THEN 1
                 ELSE 0 END ASC,
            c.fecha_vencimiento ASC
    )";
    q.prepare(sql);
    QList<ConcesionRecord> result;
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::findAll:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(mapRow(q));
    return result;
}

QList<ConcesionRecord> ConcesionRepository::findActivas() const {
    QSqlQuery q(m_db.database());
    QString sql = QString(BASE_QUERY) + "WHERE c.activa = 1 ORDER BY c.fecha_vencimiento ASC";
    q.prepare(sql);
    QList<ConcesionRecord> result;
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::findActivas:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(mapRow(q));
    return result;
}

QList<ConcesionRecord> ConcesionRepository::findVencenPronto(int dias) const {
    QSqlQuery q(m_db.database());
    QString sql = QString(BASE_QUERY) + R"(
        WHERE c.activa = 1
          AND c.fecha_vencimiento IS NOT NULL
          AND c.fecha_vencimiento BETWEEN date('now') AND date('now', :dias)
        ORDER BY c.fecha_vencimiento ASC
    )";
    q.prepare(sql);
    q.bindValue(":dias", QString("+%1 days").arg(dias));
    QList<ConcesionRecord> result;
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::findVencenPronto:" << q.lastError().text();
        return result;
    }
    while (q.next()) result.append(mapRow(q));
    return result;
}

ConcesionRecord ConcesionRepository::findById(int64_t id) const {
    QSqlQuery q(m_db.database());
    QString sql = QString(BASE_QUERY) + "WHERE c.id = :id";
    q.prepare(sql);
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    if (q.exec() && q.next()) return mapRow(q);
    return {};
}

int64_t ConcesionRepository::save(const ConcesionRecord& record) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO concesiones
            (emisor_id, emisor_nombre, emisor_contacto, folio,
             fecha_recepcion, fecha_vencimiento, tipo_documento,
             notas, activa, comision_pct)
        VALUES
            (:emisor_id, :emisor_nombre, :emisor_contacto, :folio,
             :fecha_recepcion, :fecha_vencimiento, :tipo_documento,
             :notas, :activa, :comision_pct)
    )");

    auto optStr = [](const QString& s) -> QVariant {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };
    auto optId = [](int64_t id) -> QVariant {
        return id <= 0 ? QVariant(QMetaType(QMetaType::LongLong))
                       : QVariant::fromValue(static_cast<qlonglong>(id));
    };

    q.bindValue(":emisor_id",         optId(record.emisorId));
    q.bindValue(":emisor_nombre",     record.emisorNombre);
    q.bindValue(":emisor_contacto",   optStr(record.emisorContacto));
    q.bindValue(":folio",             optStr(record.folio));
    q.bindValue(":fecha_recepcion",   optStr(record.fechaRecepcion));
    q.bindValue(":fecha_vencimiento", optStr(record.fechaVencimiento));
    q.bindValue(":tipo_documento",
        record.tipoDocumento == TipoDocumentoConcesion::NotaDeCredito
            ? "Nota de credito" : "Factura");
    q.bindValue(":notas",        optStr(record.notas));
    q.bindValue(":activa",       record.activa ? 1 : 0);
    q.bindValue(":comision_pct", record.comisionPct);

    if (!q.exec()) {
        qCritical() << "ConcesionRepo::save:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool ConcesionRepository::update(const ConcesionRecord& r) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        UPDATE concesiones SET
            emisor_id=:emisor_id, emisor_nombre=:emisor_nombre,
            folio=:folio, fecha_recepcion=:frec, fecha_vencimiento=:fvenc,
            tipo_documento=:tipo, notas=:notas, activa=:activa,
            comision_pct=:comision_pct
        WHERE id=:id
    )");
    auto optStr = [](const QString& s) -> QVariant {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };
    auto optId = [](int64_t id) -> QVariant {
        return id <= 0 ? QVariant(QMetaType(QMetaType::LongLong))
                       : QVariant::fromValue(static_cast<qlonglong>(id));
    };
    q.bindValue(":emisor_id",    optId(r.emisorId));
    q.bindValue(":emisor_nombre", r.emisorNombre);
    q.bindValue(":folio",        optStr(r.folio));
    q.bindValue(":frec",         optStr(r.fechaRecepcion));
    q.bindValue(":fvenc",        optStr(r.fechaVencimiento));
    q.bindValue(":tipo",
        r.tipoDocumento == TipoDocumentoConcesion::NotaDeCredito
            ? "Nota de credito" : "Factura");
    q.bindValue(":notas",        optStr(r.notas));
    q.bindValue(":activa",       r.activa ? 1 : 0);
    q.bindValue(":comision_pct", r.comisionPct);
    q.bindValue(":id",           QVariant::fromValue(static_cast<qlonglong>(r.id)));
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::update:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool ConcesionRepository::finalizar(int64_t id) {
    QSqlQuery q(m_db.database());
    q.prepare("UPDATE concesiones SET activa = 0 WHERE id = :id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::finalizar:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

int ConcesionRepository::countActiveByEmisor(int64_t emisorId) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT COUNT(*) FROM concesiones WHERE emisor_id = :eid AND activa = 1");
    q.bindValue(":eid", QVariant::fromValue(static_cast<qlonglong>(emisorId)));
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

bool ConcesionRepository::remove(int64_t id) {
    QSqlQuery q(m_db.database());
    // Nullificar FK en productos_calculados antes de borrar
    q.prepare("UPDATE productos_calculados SET concesion_id=NULL WHERE concesion_id=:id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    q.exec();
    q.prepare("DELETE FROM concesiones WHERE id=:id");
    q.bindValue(":id", QVariant::fromValue(static_cast<qlonglong>(id)));
    if (!q.exec()) {
        qCritical() << "ConcesionRepo::remove:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace Calculadora
