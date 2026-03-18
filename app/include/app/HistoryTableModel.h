#pragma once
#include <QAbstractTableModel>
#include <QList>
#include "core/ProductoRepository.h"

namespace App {

class HistoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Columns {
        Col_Fecha = 0,
        Col_NombreProducto,
        Col_TipoProducto,
        Col_ISBN,
        Col_Escenario,
        Col_CFDI,
        Col_PrecioFinal,
        Col_Costo,
        Col_Comision,
        Col_IvaTrasladado,
        Col_IvaAcreditable,
        Col_IvaNetoPagar,
        Col_Proveedor,
        Col_Vendedor,
        Col_COUNT
    };

    explicit HistoryTableModel(QObject* parent = nullptr);

    void setRecords(QList<Calculadora::ProductoRecord> records);
    const Calculadora::ProductoRecord& recordAt(int row) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QList<Calculadora::ProductoRecord> m_records;
};

} // namespace App
