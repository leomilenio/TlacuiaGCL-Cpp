#pragma once
#include <QDialog>
#include <QList>
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"

class QLabel;
class QTableWidget;
class QPushButton;

namespace App {

// Dialogo de corte de concesion.
// Muestra una tabla editable (cantidad vendida por producto), calcula totales
// en tiempo real, permite exportar el corte a PDF y confirmar el cierre.
class CorteDialog : public QDialog {
    Q_OBJECT
public:
    explicit CorteDialog(const Calculadora::ConcesionRecord&   concesion,
                         const QList<Calculadora::ProductoRecord>& productos,
                         Calculadora::ProductoRepository&      productoRepo,
                         Calculadora::ConcesionRepository&     concesionRepo,
                         QWidget* parent = nullptr);

private slots:
    void onCantidadesChanged();
    void onExportarPdfClicked();
    void onConfirmarClicked();

private:
    void setupUi();
    void setupConnections();
    void recalcularTotales();

    const Calculadora::ConcesionRecord         m_concesion;
    QList<Calculadora::ProductoRecord>         m_productos;
    Calculadora::ProductoRepository&           m_productoRepo;
    Calculadora::ConcesionRepository&          m_concesionRepo;

    QTableWidget* m_tabla         = nullptr;
    QLabel*       m_lblPagoTotal  = nullptr;
    QLabel*       m_lblDevTotal   = nullptr;
    QLabel*       m_lblGanancia   = nullptr;
    QLabel*       m_lblIvaTrasl   = nullptr;
    QLabel*       m_lblIvaAcred   = nullptr;
    QLabel*       m_lblIvaNeto    = nullptr;
    QPushButton*  m_btnPdf        = nullptr;
    QPushButton*  m_btnConfirmar  = nullptr;
};

} // namespace App
