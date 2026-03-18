#include "core/ProductoRepository.h"
#include "core/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace Calculadora {

ProductoRepository::ProductoRepository(DatabaseManager& dbManager)
    : m_db(dbManager)
{
}

int64_t ProductoRepository::save(const ProductoRecord& record) {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        INSERT INTO productos_calculados
            (nombre_producto, tipo_producto, isbn,
             precio_final, costo, comision,
             iva_trasladado, iva_acreditable, iva_neto_sat,
             escenario, tiene_cfdi,
             nombre_proveedor, nombre_vendedor, concesion_id,
             cantidad_recibida)
        VALUES
            (:nombre_producto, :tipo_producto, :isbn,
             :precio_final, :costo, :comision,
             :iva_trasladado, :iva_acreditable, :iva_neto_sat,
             :escenario, :tiene_cfdi,
             :nombre_proveedor, :nombre_vendedor, :concesion_id,
             :cantidad_recibida)
    )");

    auto optStr = [](const QString& s) -> QVariant {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };

    q.bindValue(":nombre_producto",  record.nombreProducto);
    q.bindValue(":tipo_producto",
        record.tipoProducto == TipoProducto::Libro ? "libro" : "papeleria");
    q.bindValue(":isbn",             optStr(record.isbn));
    q.bindValue(":precio_final",     record.precioFinal);
    q.bindValue(":costo",            record.costo);
    q.bindValue(":comision",         record.comision);
    q.bindValue(":iva_trasladado",   record.ivaTrasladado);
    q.bindValue(":iva_acreditable",  record.ivaAcreditable);
    q.bindValue(":iva_neto_sat",     record.ivaNetoPagar);
    q.bindValue(":escenario",        record.escenario == Escenario::ProductoPropio
                                         ? "propio" : "concesion");
    q.bindValue(":tiene_cfdi",       record.tieneCFDI ? 1 : 0);
    q.bindValue(":nombre_proveedor", optStr(record.nombreProveedor));
    q.bindValue(":nombre_vendedor",  optStr(record.nombreVendedor));
    q.bindValue(":concesion_id",     record.concesionId.has_value()
                                         ? QVariant(static_cast<qlonglong>(*record.concesionId))
                                         : QVariant(QMetaType(QMetaType::LongLong)));
    q.bindValue(":cantidad_recibida", record.cantidadRecibida);

    if (!q.exec()) {
        qCritical() << "Error guardando producto:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

ProductoRecord ProductoRepository::mapRow(const QSqlQuery& q) const {
    ProductoRecord r;
    r.id              = q.value("id").toLongLong();
    r.fecha           = q.value("fecha").toString();
    r.nombreProducto  = q.value("nombre_producto").toString();
    r.tipoProducto    = q.value("tipo_producto").toString() == "libro"
                            ? TipoProducto::Libro : TipoProducto::Papeleria;
    r.isbn            = q.value("isbn").toString();
    r.precioFinal     = q.value("precio_final").toDouble();
    r.costo           = q.value("costo").toDouble();
    r.comision        = q.value("comision").toDouble();
    r.ivaTrasladado   = q.value("iva_trasladado").toDouble();
    r.ivaAcreditable  = q.value("iva_acreditable").toDouble();
    r.ivaNetoPagar    = q.value("iva_neto_sat").toDouble();
    r.escenario       = q.value("escenario").toString() == "propio"
                            ? Escenario::ProductoPropio : Escenario::Concesion;
    r.tieneCFDI       = q.value("tiene_cfdi").toInt() != 0;
    r.nombreProveedor = q.value("nombre_proveedor").toString();
    r.nombreVendedor  = q.value("nombre_vendedor").toString();
    QVariant cid      = q.value("concesion_id");
    if (!cid.isNull()) r.concesionId = cid.toLongLong();
    r.cantidadRecibida = q.value("cantidad_recibida").toInt();
    r.cantidadVendida  = q.value("cantidad_vendida").toInt();
    return r;
}

QList<ProductoRecord> ProductoRepository::findAll() const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM productos_calculados ORDER BY fecha DESC");
    QList<ProductoRecord> result;
    if (!q.exec()) {
        qCritical() << "Error consultando productos:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(mapRow(q));
    }
    return result;
}

QList<ProductoRecord> ProductoRepository::findPage(int limit, int offset) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM productos_calculados ORDER BY fecha DESC LIMIT :limit OFFSET :offset");
    q.bindValue(":limit",  limit);
    q.bindValue(":offset", offset);
    QList<ProductoRecord> result;
    if (!q.exec()) {
        qCritical() << "Error en findPage:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(mapRow(q));
    }
    return result;
}

bool ProductoRepository::remove(int64_t id) {
    QSqlQuery q(m_db.database());
    q.prepare("DELETE FROM productos_calculados WHERE id = :id");
    q.bindValue(":id", static_cast<qlonglong>(id));
    if (!q.exec()) {
        qCritical() << "Error eliminando producto:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

CorteResult ProductoRepository::calcularCorte(int64_t concesionId) const {
    QSqlQuery q(m_db.database());
    q.prepare(R"(
        SELECT
            COUNT(*)                                            AS cnt,
            SUM(cantidad_recibida)                             AS sum_qty_rec,
            SUM(cantidad_vendida)                              AS sum_qty_vend,
            SUM(cantidad_recibida - cantidad_vendida)          AS sum_qty_dev,
            SUM(costo * cantidad_vendida)                             AS sum_pago_dist,
            SUM(costo * (cantidad_recibida - cantidad_vendida))      AS sum_devolucion,
            SUM(precio_final * cantidad_vendida)                     AS sum_precio_final,
            SUM((precio_final - costo) * cantidad_vendida)           AS sum_ganancia,
            SUM(costo)                                               AS sum_costo,
            SUM(comision)                                            AS sum_comision,
            SUM(iva_trasladado)                                      AS sum_iva_trasladado,
            SUM(iva_acreditable)                                     AS sum_iva_acreditable,
            SUM(iva_neto_sat)                                        AS sum_iva_neto
        FROM productos_calculados
        WHERE concesion_id = :cid
    )");
    q.bindValue(":cid", QVariant::fromValue(static_cast<qlonglong>(concesionId)));
    CorteResult r;
    r.concesionId = concesionId;
    if (!q.exec() || !q.next()) {
        qCritical() << "Error en calcularCorte:" << q.lastError().text();
        return r;
    }
    r.cantidadRegistros        = q.value("cnt").toInt();
    r.totalUnidadesRecibidas   = q.value("sum_qty_rec").toInt();
    r.totalUnidadesVendidas    = q.value("sum_qty_vend").toInt();
    r.totalUnidadesDevueltas   = q.value("sum_qty_dev").toInt();
    r.totalPagoAlDistribuidor  = q.value("sum_pago_dist").toDouble();
    r.totalDevolucion          = q.value("sum_devolucion").toDouble();
    r.totalPrecioFinal         = q.value("sum_precio_final").toDouble();
    r.gananciaEstimada         = q.value("sum_ganancia").toDouble();
    r.totalCosto               = q.value("sum_costo").toDouble();
    r.totalComision            = q.value("sum_comision").toDouble();
    r.totalIvaTrasladado       = q.value("sum_iva_trasladado").toDouble();
    r.totalIvaAcreditable      = q.value("sum_iva_acreditable").toDouble();
    r.totalIvaNetoPagar        = q.value("sum_iva_neto").toDouble();
    r.isValid                  = (r.cantidadRegistros > 0);
    return r;
}

QList<ProductoRecord> ProductoRepository::findByConcesion(int64_t concesionId) const {
    QSqlQuery q(m_db.database());
    q.prepare("SELECT * FROM productos_calculados WHERE concesion_id = :cid ORDER BY id ASC");
    q.bindValue(":cid", QVariant::fromValue(static_cast<qlonglong>(concesionId)));
    QList<ProductoRecord> result;
    if (!q.exec()) {
        qCritical() << "Error en findByConcesion:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.append(mapRow(q));
    }
    return result;
}

bool ProductoRepository::updateCantidadVendida(int64_t productoId, int cantidadVendida) {
    QSqlQuery q(m_db.database());
    q.prepare("UPDATE productos_calculados SET cantidad_vendida = :qty WHERE id = :id");
    q.bindValue(":qty", cantidadVendida);
    q.bindValue(":id",  static_cast<qlonglong>(productoId));
    if (!q.exec()) {
        qCritical() << "Error en updateCantidadVendida:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

} // namespace Calculadora
