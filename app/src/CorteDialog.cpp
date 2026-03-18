#include "app/CorteDialog.h"
#include "app/CortePdfExporter.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QLocale>
#include <QDate>

namespace App {

// Indices de columna en la tabla
static constexpr int COL_PRODUCTO  = 0;
static constexpr int COL_REC       = 1;
static constexpr int COL_VEND      = 2;
static constexpr int COL_DEV       = 3;
static constexpr int COL_NETO      = 4;
static constexpr int COL_SUBTOTAL  = 5;

CorteDialog::CorteDialog(const Calculadora::ConcesionRecord&       concesion,
                         const QList<Calculadora::ProductoRecord>& productos,
                         Calculadora::ProductoRepository&          productoRepo,
                         Calculadora::ConcesionRepository&         concesionRepo,
                         QWidget* parent)
    : QDialog(parent)
    , m_concesion(concesion)
    , m_productos(productos)
    , m_productoRepo(productoRepo)
    , m_concesionRepo(concesionRepo)
{
    setupUi();
    setupConnections();
    recalcularTotales();
}

void CorteDialog::setupUi() {
    QString emisor = m_concesion.emisorNombre.isEmpty() ? "(Sin distribuidor)" : m_concesion.emisorNombre;
    QString folio  = m_concesion.folio.isEmpty()        ? "(Sin folio)"        : m_concesion.folio;
    setWindowTitle(QString("Corte — %1  %2").arg(emisor, folio));
    setMinimumWidth(680);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(8);

    // Titulo
    auto* title = new QLabel(QString("<b>Corte de Concesion</b><br>%1 — %2").arg(emisor, folio));
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    auto addSep = [&]() {
        auto* s = new QFrame(); s->setFrameShape(QFrame::HLine);
        mainLayout->addWidget(s);
    };
    addSep();

    // Tabla editable
    m_tabla = new QTableWidget(static_cast<int>(m_productos.size()), 6, this);
    m_tabla->setHorizontalHeaderLabels({"Producto", "Rec", "Vendida", "Dev", "Precio Neto", "Subtotal"});
    m_tabla->horizontalHeader()->setStretchLastSection(true);
    m_tabla->horizontalHeader()->setHighlightSections(false);
    m_tabla->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabla->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabla->setAlternatingRowColors(true);
    m_tabla->setShowGrid(false);
    m_tabla->verticalHeader()->setVisible(false);

    QLocale loc;
    for (int i = 0; i < m_productos.size(); ++i) {
        const auto& p = m_productos[i];

        // Producto
        auto* itemNombre = new QTableWidgetItem(p.nombreProducto);
        itemNombre->setFlags(itemNombre->flags() & ~Qt::ItemIsEditable);
        m_tabla->setItem(i, COL_PRODUCTO, itemNombre);

        // Qty Recibida (solo lectura)
        auto* itemRec = new QTableWidgetItem(QString::number(p.cantidadRecibida));
        itemRec->setTextAlignment(Qt::AlignCenter);
        itemRec->setFlags(itemRec->flags() & ~Qt::ItemIsEditable);
        m_tabla->setItem(i, COL_REC, itemRec);

        // Qty Vendida (editable con QSpinBox)
        auto* spin = new QSpinBox();
        spin->setMinimum(0);
        spin->setMaximum(p.cantidadRecibida);
        spin->setValue(p.cantidadVendida);
        spin->setAlignment(Qt::AlignCenter);
        m_tabla->setCellWidget(i, COL_VEND, spin);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &CorteDialog::onCantidadesChanged);

        // Qty Devuelta (calculado, solo lectura)
        int devuelta = p.cantidadRecibida - p.cantidadVendida;
        auto* itemDev = new QTableWidgetItem(QString::number(devuelta));
        itemDev->setTextAlignment(Qt::AlignCenter);
        itemDev->setFlags(itemDev->flags() & ~Qt::ItemIsEditable);
        m_tabla->setItem(i, COL_DEV, itemDev);

        // Precio Neto
        auto* itemNeto = new QTableWidgetItem(loc.toCurrencyString(p.costo));
        itemNeto->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemNeto->setFlags(itemNeto->flags() & ~Qt::ItemIsEditable);
        m_tabla->setItem(i, COL_NETO, itemNeto);

        // Subtotal (costo * cantidadVendida)
        double subtotal = p.costo * p.cantidadVendida;
        auto* itemSub = new QTableWidgetItem(loc.toCurrencyString(subtotal));
        itemSub->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        itemSub->setFlags(itemSub->flags() & ~Qt::ItemIsEditable);
        m_tabla->setItem(i, COL_SUBTOTAL, itemSub);
    }

    m_tabla->resizeColumnsToContents();
    m_tabla->horizontalHeader()->setSectionResizeMode(COL_PRODUCTO, QHeaderView::Stretch);
    mainLayout->addWidget(m_tabla);

    addSep();

    // Seccion de totales
    auto* totalesForm = new QFormLayout();
    totalesForm->setLabelAlignment(Qt::AlignRight);
    m_lblPagoTotal = new QLabel("-"); m_lblPagoTotal->setAlignment(Qt::AlignRight);
    m_lblDevTotal  = new QLabel("-"); m_lblDevTotal->setAlignment(Qt::AlignRight);
    m_lblIvaTrasl  = new QLabel("-"); m_lblIvaTrasl->setAlignment(Qt::AlignRight);
    m_lblIvaAcred  = new QLabel("-"); m_lblIvaAcred->setAlignment(Qt::AlignRight);
    m_lblIvaNeto   = new QLabel("-"); m_lblIvaNeto->setAlignment(Qt::AlignRight);

    QFont bf = m_lblPagoTotal->font(); bf.setBold(true);
    m_lblPagoTotal->setFont(bf);

    totalesForm->addRow("Total a pagar al distribuidor:",   m_lblPagoTotal);
    totalesForm->addRow("Total devoluciones (piezas):",     m_lblDevTotal);
    totalesForm->addRow("IVA Trasladado:",                  m_lblIvaTrasl);
    totalesForm->addRow("IVA Acreditable:",                 m_lblIvaAcred);
    totalesForm->addRow("IVA Neto a SAT:",                  m_lblIvaNeto);
    mainLayout->addLayout(totalesForm);

    addSep();

    // Botones
    auto* btnRow = new QHBoxLayout();
    m_btnPdf      = new QPushButton("Exportar PDF");
    m_btnConfirmar = new QPushButton("Confirmar");
    auto* btnCancel = new QPushButton("Cancelar");
    m_btnConfirmar->setObjectName("primaryButton");
    m_btnConfirmar->setDefault(true);
    btnRow->addWidget(m_btnPdf);
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(m_btnConfirmar);
    mainLayout->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void CorteDialog::setupConnections() {
    connect(m_btnPdf,       &QPushButton::clicked, this, &CorteDialog::onExportarPdfClicked);
    connect(m_btnConfirmar, &QPushButton::clicked, this, &CorteDialog::onConfirmarClicked);
}

void CorteDialog::onCantidadesChanged() {
    // Actualizar columnas Qty Devuelta y Subtotal en tiempo real
    QLocale loc;
    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (!spin) continue;
        int vendida  = spin->value();
        int recibida = m_productos[i].cantidadRecibida;
        int devuelta = recibida - vendida;

        if (auto* itDev = m_tabla->item(i, COL_DEV))
            itDev->setText(QString::number(devuelta));

        double subtotal = m_productos[i].costo * vendida;
        if (auto* itSub = m_tabla->item(i, COL_SUBTOTAL))
            itSub->setText(loc.toCurrencyString(subtotal));
    }
    recalcularTotales();
}

void CorteDialog::recalcularTotales() {
    double pagoTotal = 0.0;
    int    devTotal  = 0;
    double ivaTrasl  = 0.0;
    double ivaAcred  = 0.0;
    double ivaNeto   = 0.0;

    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        int vendida = spin ? spin->value() : m_productos[i].cantidadVendida;
        int devuelta = m_productos[i].cantidadRecibida - vendida;

        pagoTotal += m_productos[i].costo * vendida;
        devTotal  += devuelta;
        ivaTrasl  += m_productos[i].ivaTrasladado;
        ivaAcred  += m_productos[i].ivaAcreditable;
        ivaNeto   += m_productos[i].ivaNetoPagar;
    }

    QLocale loc;
    m_lblPagoTotal->setText(loc.toCurrencyString(pagoTotal));
    m_lblDevTotal->setText(QString::number(devTotal) + " piezas");
    m_lblIvaTrasl->setText(loc.toCurrencyString(ivaTrasl));
    m_lblIvaAcred->setText(loc.toCurrencyString(ivaAcred));
    m_lblIvaNeto->setText(loc.toCurrencyString(ivaNeto));
}

void CorteDialog::onExportarPdfClicked() {
    QString emisor = m_concesion.emisorNombre.isEmpty() ? "SinDistribuidor" : m_concesion.emisorNombre;
    QString folio  = m_concesion.folio.isEmpty()        ? "SinFolio"        : m_concesion.folio;
    QString fecha  = QDate::currentDate().toString("yyyy-MM-dd");
    QString nombreSugerido = QString("Corte_%1_%2_%3.pdf")
                             .arg(emisor.simplified().replace(' ', '_'), folio, fecha);

    QString filePath = QFileDialog::getSaveFileName(
        this, "Guardar corte PDF", nombreSugerido, "PDF (*.pdf)");
    if (filePath.isEmpty()) return;

    // Construir lista de productos con las cantidades actuales del dialogo
    QList<Calculadora::ProductoRecord> productosActuales = m_productos;
    for (int i = 0; i < productosActuales.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (spin) productosActuales[i].cantidadVendida = spin->value();
    }

    Calculadora::CorteResult corte = m_productoRepo.calcularCorte(m_concesion.id);

    if (!CortePdfExporter::exportar(m_concesion, productosActuales, corte, filePath)) {
        QMessageBox::critical(this, "Error", "No se pudo generar el PDF.");
    } else {
        QMessageBox::information(this, "PDF generado",
            QString("Corte guardado en:\n%1").arg(filePath));
    }
}

void CorteDialog::onConfirmarClicked() {
    // Guardar cantidades vendidas en DB
    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (!spin) continue;
        m_productoRepo.updateCantidadVendida(m_productos[i].id, spin->value());
    }
    // Finalizar la concesion
    m_concesionRepo.finalizar(m_concesion.id);
    accept();
}

} // namespace App
