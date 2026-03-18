#include "app/HistoryTableModel.h"
#include <QLocale>

namespace App {

HistoryTableModel::HistoryTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void HistoryTableModel::setRecords(QList<Calculadora::ProductoRecord> records) {
    beginResetModel();
    m_records = std::move(records);
    endResetModel();
}

const Calculadora::ProductoRecord& HistoryTableModel::recordAt(int row) const {
    return m_records.at(row);
}

int HistoryTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_records.size();
}

int HistoryTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : Col_COUNT;
}

QVariant HistoryTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size()) return {};
    if (role != Qt::DisplayRole) return {};

    const auto& r = m_records.at(index.row());
    QLocale loc;

    switch (index.column()) {
    case Col_Fecha:          return r.fecha;
    case Col_NombreProducto: return r.nombreProducto;
    case Col_TipoProducto:   return r.tipoProducto == Calculadora::TipoProducto::Libro
                                    ? "Libro" : "Papeleria";
    case Col_ISBN:           return r.isbn;
    case Col_Escenario:      return r.escenario == Calculadora::Escenario::ProductoPropio
                                    ? "Propio" : "Concesion";
    case Col_CFDI:           return r.tieneCFDI ? "Si" : "No";
    case Col_PrecioFinal:    return loc.toCurrencyString(r.precioFinal);
    case Col_Costo:          return loc.toCurrencyString(r.costo);
    case Col_Comision:       return loc.toCurrencyString(r.comision);
    case Col_IvaTrasladado:  return loc.toCurrencyString(r.ivaTrasladado);
    case Col_IvaAcreditable: return r.tieneCFDI ? loc.toCurrencyString(r.ivaAcreditable)
                                                 : "N/A";
    case Col_IvaNetoPagar:   return loc.toCurrencyString(r.ivaNetoPagar);
    case Col_Proveedor:      return r.nombreProveedor;
    case Col_Vendedor:       return r.nombreVendedor;
    default:                 return {};
    }
}

QVariant HistoryTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
    switch (section) {
    case Col_Fecha:          return "Fecha";
    case Col_NombreProducto: return "Producto";
    case Col_TipoProducto:   return "Tipo";
    case Col_ISBN:           return "ISBN";
    case Col_Escenario:      return "Escenario";
    case Col_CFDI:           return "CFDI";
    case Col_PrecioFinal:    return "Precio Final";
    case Col_Costo:          return "Costo";
    case Col_Comision:       return "Comision";
    case Col_IvaTrasladado:  return "IVA Trasladado";
    case Col_IvaAcreditable: return "IVA Acreditable";
    case Col_IvaNetoPagar:   return "IVA Neto SAT";
    case Col_Proveedor:      return "Proveedor";
    case Col_Vendedor:       return "Vendedor";
    default:                 return {};
    }
}

} // namespace App
