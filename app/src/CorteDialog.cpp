#include "app/CorteDialog.h"
#include <QDebug>
#include "app/CortePdfExporter.h"
#include "core/FolioRepository.h"
#include "core/LibreriaConfigRepository.h"
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
    const bool cerrada  = !m_concesion.activa;
    const bool precorte = m_concesion.activa && m_concesion.enPrecorte;
    const bool activa   = m_concesion.activa && !m_concesion.enPrecorte;

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

    // Banner según modo
    if (cerrada) {
        auto* banner = new QLabel(
            "\u26A0  Concesion cerrada \u2014 solo lectura. El corte no puede modificarse.");
        banner->setStyleSheet(
            "background:#FFF3E0; color:#E65100; padding:6px 10px;"
            "border-radius:4px; font-weight:bold;");
        mainLayout->addWidget(banner);
    } else if (precorte) {
        auto* banner = new QLabel(
            "\u26A0  PRE-CORTE \u2014 Revise las cantidades y los documentos antes de finalizar.");
        banner->setStyleSheet(
            "background:#FFF3E0; color:#E65100; padding:6px 10px;"
            "border-radius:4px; font-weight:bold;");
        mainLayout->addWidget(banner);
    }

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

    // Deshabilitar edicion solo si la concesion ya esta cerrada
    if (cerrada) {
        for (int i = 0; i < m_tabla->rowCount(); ++i) {
            if (auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND)))
                spin->setEnabled(false);
        }
    }

    mainLayout->addWidget(m_tabla);

    addSep();

    // Seccion de totales
    auto* totalesForm = new QFormLayout();
    totalesForm->setLabelAlignment(Qt::AlignRight);
    m_lblPagoTotal = new QLabel("-"); m_lblPagoTotal->setAlignment(Qt::AlignRight);
    m_lblDevTotal  = new QLabel("-"); m_lblDevTotal->setAlignment(Qt::AlignRight);
    m_lblGanancia  = new QLabel("-"); m_lblGanancia->setAlignment(Qt::AlignRight);
    m_lblIvaTrasl  = new QLabel("-"); m_lblIvaTrasl->setAlignment(Qt::AlignRight);
    m_lblIvaAcred  = new QLabel("-"); m_lblIvaAcred->setAlignment(Qt::AlignRight);
    m_lblIvaNeto   = new QLabel("-"); m_lblIvaNeto->setAlignment(Qt::AlignRight);

    QFont bf = m_lblPagoTotal->font(); bf.setBold(true);
    m_lblPagoTotal->setFont(bf);
    QFont gf = m_lblGanancia->font(); gf.setBold(true);
    m_lblGanancia->setFont(gf);
    m_lblGanancia->setStyleSheet("color: #2E7D32;");

    totalesForm->addRow("Total a pagar al distribuidor:",   m_lblPagoTotal);
    totalesForm->addRow("Total devoluciones (piezas):",     m_lblDevTotal);
    totalesForm->addRow("Ganancia estimada (comision):",    m_lblGanancia);
    totalesForm->addRow("IVA Trasladado:",                  m_lblIvaTrasl);
    totalesForm->addRow("IVA Acreditable:",                 m_lblIvaAcred);
    totalesForm->addRow("IVA Neto a SAT:",                  m_lblIvaNeto);
    mainLayout->addLayout(totalesForm);

    addSep();

    // Botones — fila diferente según modo
    auto* btnRow = new QHBoxLayout();
    m_btnPdfInterno   = new QPushButton("Corte interno");
    m_btnPdfProveedor = new QPushButton("Corte proveedor");
    btnRow->addWidget(m_btnPdfInterno);
    btnRow->addWidget(m_btnPdfProveedor);
    btnRow->addStretch();

    if (cerrada) {
        auto* btnCerrar = new QPushButton("Cerrar");
        btnRow->addWidget(btnCerrar);
        connect(btnCerrar, &QPushButton::clicked, this, &QDialog::reject);
    } else {
        auto* btnCancel = new QPushButton("Cancelar");
        connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
        btnRow->addWidget(btnCancel);

        if (activa) {
            m_btnGuardarPrecorte = new QPushButton("Guardar pre-corte \u2192");
            m_btnGuardarPrecorte->setObjectName("primaryButton");
            m_btnGuardarPrecorte->setDefault(true);
            btnRow->addWidget(m_btnGuardarPrecorte);
        } else { // precorte
            m_btnFinalizar = new QPushButton("\u26A0 Finalizar corte");
            m_btnFinalizar->setObjectName("primaryButton");
            m_btnFinalizar->setDefault(true);
            btnRow->addWidget(m_btnFinalizar);
        }
    }

    mainLayout->addLayout(btnRow);
}

void CorteDialog::setupConnections() {
    connect(m_btnPdfInterno,   &QPushButton::clicked, this, [this]() { onExportarPdf(true);  });
    connect(m_btnPdfProveedor, &QPushButton::clicked, this, [this]() { onExportarPdf(false); });
    if (m_btnGuardarPrecorte)
        connect(m_btnGuardarPrecorte, &QPushButton::clicked, this, &CorteDialog::onGuardarPrecorteClicked);
    if (m_btnFinalizar)
        connect(m_btnFinalizar, &QPushButton::clicked, this, &CorteDialog::onFinalizarCorteClicked);
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
    double ganancia  = 0.0;
    double ivaTrasl  = 0.0;
    double ivaAcred  = 0.0;
    double ivaNeto   = 0.0;

    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        int vendida  = spin ? spin->value() : m_productos[i].cantidadVendida;
        int devuelta = m_productos[i].cantidadRecibida - vendida;

        pagoTotal += m_productos[i].costo * vendida;
        devTotal  += devuelta;
        ganancia  += m_productos[i].comision * vendida;
        ivaTrasl  += m_productos[i].ivaTrasladado;
        ivaAcred  += m_productos[i].ivaAcreditable;
        ivaNeto   += m_productos[i].ivaNetoPagar;
    }

    QLocale loc;
    m_lblPagoTotal->setText(loc.toCurrencyString(pagoTotal));
    m_lblDevTotal->setText(QString::number(devTotal) + " piezas");
    m_lblGanancia->setText(loc.toCurrencyString(ganancia));
    m_lblIvaTrasl->setText(loc.toCurrencyString(ivaTrasl));
    m_lblIvaAcred->setText(loc.toCurrencyString(ivaAcred));
    m_lblIvaNeto->setText(loc.toCurrencyString(ivaNeto));
}

void CorteDialog::onExportarPdf(bool interno) {
    QString emisor = m_concesion.emisorNombre.isEmpty() ? "SinDistribuidor" : m_concesion.emisorNombre;
    QString folio  = m_concesion.folio.isEmpty()        ? "SinFolio"        : m_concesion.folio;
    QString fecha  = QDate::currentDate().toString("yyyy-MM-dd");
    QString prefijo = interno ? "CorteCI" : "CorteP";
    QString nombreSugerido = QString("%1_%2_%3_%4.pdf")
                             .arg(prefijo, emisor.simplified().replace(' ', '_'), folio, fecha);

    QString filePath = QFileDialog::getSaveFileName(
        this, interno ? "Guardar corte interno PDF" : "Guardar corte proveedor PDF",
        nombreSugerido, "PDF (*.pdf)");
    if (filePath.isEmpty()) return;

    // Construir lista de productos con las cantidades actuales del dialogo
    QList<Calculadora::ProductoRecord> productosActuales = m_productos;
    for (int i = 0; i < productosActuales.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (spin) productosActuales[i].cantidadVendida = spin->value();
    }

    Calculadora::CorteResult corte = m_productoRepo.calcularCorte(m_concesion.id);
    // Recalcular ganancia con los valores actuales del dialogo (pueden diferir de la DB)
    double gananciaActual = 0.0;
    for (const auto& p : productosActuales)
        gananciaActual += p.comision * p.cantidadVendida;
    corte.gananciaEstimada = gananciaActual;

    // Cargar config de libreria y generar/obtener folio del corte
    Calculadora::LibreriaConfigRepository cfgRepo(m_concesionRepo.database());
    Calculadora::FolioRepository          folioRepo(m_concesionRepo.database());
    const auto    config      = cfgRepo.load();
    const QString baseFolio   = folioRepo.getFolioCorte(m_concesion.id, m_concesion.tipoDocumento);
    const QString folioDoc    = interno ? ("CI-" + baseFolio) : baseFolio;

    const bool includeFirmas = QMessageBox::question(
        this, "Sección de firmas",
        "¿Desea incluir la sección de firmas en el PDF?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;

    const bool preview = m_concesion.activa;  // true para activa y pre-corte
    bool ok = interno
        ? CortePdfExporter::exportar(m_concesion, productosActuales, corte, config, folioDoc, filePath, includeFirmas, preview)
        : CortePdfExporter::exportarProveedor(m_concesion, productosActuales, corte, config, folioDoc, filePath, includeFirmas, preview);

    if (!ok) {
        QMessageBox::critical(this, "Error", "No se pudo generar el PDF.");
    } else {
        QMessageBox::information(this, "PDF generado",
            QString("Corte guardado en:\n%1").arg(filePath));
    }
}

void CorteDialog::onGuardarPrecorteClicked() {
    // Guardar cantidades vendidas en DB
    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (!spin) continue;
        m_productoRepo.updateCantidadVendida(m_productos[i].id, spin->value());
    }
    // Marcar la concesion como en pre-corte
    if (!m_concesionRepo.guardarPrecorte(m_concesion.id))
        qWarning() << "CorteDialog: no se pudo guardar el pre-corte id=" << m_concesion.id;
    accept();
}

void CorteDialog::onFinalizarCorteClicked() {
    int ret = QMessageBox::warning(
        this, "Finalizar corte",
        "¿Está seguro de finalizar el corte?\n\n"
        "Esta acción cerrará definitivamente la concesión y no podrá\n"
        "modificarse. Asegúrese de haber revisado los documentos de\n"
        "pre-corte antes de continuar.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    // Guardar cantidades vendidas en DB
    for (int i = 0; i < m_productos.size(); ++i) {
        auto* spin = qobject_cast<QSpinBox*>(m_tabla->cellWidget(i, COL_VEND));
        if (!spin) continue;
        m_productoRepo.updateCantidadVendida(m_productos[i].id, spin->value());
    }
    // Finalizar la concesion
    if (!m_concesionRepo.finalizar(m_concesion.id))
        qWarning() << "CorteDialog: no se pudo finalizar la concesion id=" << m_concesion.id;
    accept();
}

} // namespace App
