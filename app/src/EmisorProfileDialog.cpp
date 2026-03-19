#include "app/EmisorProfileDialog.h"
#include "app/EmisorPdfExporter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QDate>
#include <QLocale>
#include <QFont>

namespace App {

// ---------------------------------------------------------------------------
// Helpers locales
// ---------------------------------------------------------------------------

static QString tipoDocStr(Calculadora::TipoDocumentoConcesion td) {
    switch (td) {
    case Calculadora::TipoDocumentoConcesion::Factura:        return "Factura";
    case Calculadora::TipoDocumentoConcesion::NotaDeCredito:  return "Nota de credito";
    case Calculadora::TipoDocumentoConcesion::NotaDeRemision: return "Nota de remision";
    case Calculadora::TipoDocumentoConcesion::Otro:           return "Otro";
    }
    return "";
}

static QTableWidgetItem* mkItem(const QString& text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    item->setTextAlignment(align);
    return item;
}

static QTableWidgetItem* mkCentered(const QString& text) {
    return mkItem(text, Qt::AlignCenter);
}

static void colorItem(QTableWidgetItem* item, const QColor& fg) {
    item->setForeground(fg);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

EmisorProfileDialog::EmisorProfileDialog(
        const Calculadora::EmisorRecord&   emisor,
        Calculadora::ConcesionRepository&  concesionRepo,
        Calculadora::ProductoRepository&   productoRepo,
        QWidget* parent)
    : QDialog(parent)
    , m_emisor(emisor)
    , m_concesionRepo(concesionRepo)
    , m_productoRepo(productoRepo)
{
    setWindowTitle(QString("Perfil de Distribuidor — %1").arg(m_emisor.nombreEmisor));
    setMinimumSize(820, 560);
    resize(960, 640);

    // Cargar datos
    const auto all = m_concesionRepo.findByEmisor(m_emisor.id);
    for (const auto& c : all) {
        if (c.activa) {
            m_activas.append(c);
            m_activasCortes.append(m_productoRepo.calcularCorte(c.id));
        }
    }
    m_finalizadas = m_concesionRepo.findFinalizadasByEmisor(m_emisor.id);
    for (const auto& c : m_finalizadas) {
        m_cortes.append(m_productoRepo.calcularCorte(c.id));
    }

    setupUi();
}

// ---------------------------------------------------------------------------
// setupUi
// ---------------------------------------------------------------------------

void EmisorProfileDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    // ---- Fila superior: titulo + botón Exportar PDF ----
    auto* topRow = new QHBoxLayout();
    auto* lblTitulo = new QLabel(QString("<b>%1</b>").arg(m_emisor.nombreEmisor));
    QFont f = lblTitulo->font();
    f.setPointSize(f.pointSize() + 2);
    lblTitulo->setFont(f);
    topRow->addWidget(lblTitulo);
    topRow->addStretch();
    auto* btnPdf = new QPushButton("Exportar PDF");
    connect(btnPdf, &QPushButton::clicked, this, &EmisorProfileDialog::onExportarPdfClicked);
    topRow->addWidget(btnPdf);
    root->addLayout(topRow);

    // ---- Resumen rapido ----
    buildResumenHeader(root);

    // ---- Separador ----
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep);

    // ---- Pestañas ----
    m_tabs = new QTabWidget();
    buildTabActivas();
    buildTabHistorial();
    buildTabReporte();
    root->addWidget(m_tabs, 1);

    // ---- Botón Cerrar ----
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* btnCerrar = new QPushButton("Cerrar");
    connect(btnCerrar, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(btnCerrar);
    root->addLayout(btnRow);
}

// ---------------------------------------------------------------------------
// Resumen rápido
// ---------------------------------------------------------------------------

void EmisorProfileDialog::buildResumenHeader(QLayout* parent) {
    auto* frame = new QFrame();
    frame->setFrameShape(QFrame::StyledPanel);
    auto* grid = new QGridLayout(frame);
    grid->setContentsMargins(10, 6, 10, 6);
    grid->setHorizontalSpacing(24);

    auto addCell = [&](int row, int col, const QString& lbl, const QString& val) {
        auto* l = new QLabel(lbl);
        l->setStyleSheet("color: #777; font-size: 10pt;");
        auto* v = new QLabel(QString("<b>%1</b>").arg(val));
        grid->addWidget(l, row * 2,     col);
        grid->addWidget(v, row * 2 + 1, col);
    };

    double comisionProm = 0.0;
    if (!m_activas.isEmpty()) {
        for (const auto& c : m_activas) comisionProm += c.comisionPct;
        comisionProm /= m_activas.size();
    } else if (!m_finalizadas.isEmpty()) {
        for (const auto& c : m_finalizadas) comisionProm += c.comisionPct;
        comisionProm /= m_finalizadas.size();
    }

    addCell(0, 0, "Concesiones activas",    QString::number(m_activas.size()));
    addCell(0, 1, "Concesiones finalizadas", QString::number(m_finalizadas.size()));
    addCell(0, 2, "Facturación",             m_emisor.facturacion ? "Sí (CFDI)" : "No");
    addCell(0, 3, "Comisión promedio",       QString("%1%").arg(comisionProm, 0, 'f', 1));

    auto* vLayout = qobject_cast<QVBoxLayout*>(parent);
    if (vLayout) vLayout->addWidget(frame);
}

// ---------------------------------------------------------------------------
// Pestaña 1 — Concesiones Activas
// ---------------------------------------------------------------------------

void EmisorProfileDialog::buildTabActivas() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    if (m_activas.isEmpty()) {
        auto* lbl = new QLabel("Este distribuidor no tiene concesiones activas.");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #777; font-style: italic;");
        layout->addWidget(lbl);
        m_tabs->addTab(w, "Concesiones Activas (0)");
        return;
    }

    enum ColA { CA_Folio=0, CA_Tipo, CA_Recepcion, CA_Vencimiento, CA_Dias, CA_Productos, CA_Estado, CA_Progreso };
    auto* table = new QTableWidget(m_activas.size(), 8, w);
    table->setHorizontalHeaderLabels({"Folio", "Tipo Doc.", "Recepción", "Vencimiento",
                                      "Días rest.", "Productos", "Estado", "Progreso"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(CA_Folio,       QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Tipo,        QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Recepcion,   QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Vencimiento, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Dias,        QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Productos,   QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CA_Estado,      QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->setToolTip("Doble clic en una fila para navegar a esa concesión");

    for (int i = 0; i < m_activas.size(); ++i) {
        const auto& c = m_activas[i];
        const auto& corte = m_activasCortes[i];

        int dias = c.diasRestantes();

        // Folio — guarda el ID en UserRole para doble clic
        auto* itemFolio = mkItem(c.folio.isEmpty() ? "(Sin folio)" : c.folio);
        itemFolio->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(c.id)));
        table->setItem(i, CA_Folio, itemFolio);

        table->setItem(i, CA_Tipo,        mkItem(tipoDocStr(c.tipoDocumento)));
        table->setItem(i, CA_Recepcion,   mkCentered(c.fechaRecepcion));
        table->setItem(i, CA_Vencimiento, mkCentered(c.fechaVencimiento));

        // Días restantes — semáforo de color
        QString diasStr = dias >= 0 ? QString::number(dias) : QString("Vencida (%1)").arg(dias);
        auto* itemDias = mkCentered(diasStr);
        if      (dias < 0)   colorItem(itemDias, QColor("#D32F2F"));
        else if (dias <= 14) colorItem(itemDias, QColor("#E65100"));
        else                 colorItem(itemDias, QColor("#2E7D32"));
        table->setItem(i, CA_Dias, itemDias);

        table->setItem(i, CA_Productos, mkCentered(QString::number(corte.cantidadRegistros)));

        // Estado
        QString estadoStr;
        switch (c.status()) {
        case Calculadora::ConcesionStatus::Valido:     estadoStr = "Vigente";      break;
        case Calculadora::ConcesionStatus::VencePronto:estadoStr = "Vence pronto"; break;
        case Calculadora::ConcesionStatus::Vencida:    estadoStr = "Vencida";      break;
        case Calculadora::ConcesionStatus::Pendiente:  estadoStr = "Pendiente";    break;
        }
        auto* itemEstado = mkCentered(estadoStr);
        if (c.status() == Calculadora::ConcesionStatus::Vencida)
            colorItem(itemEstado, QColor("#D32F2F"));
        else if (c.status() == Calculadora::ConcesionStatus::VencePronto)
            colorItem(itemEstado, QColor("#E65100"));
        table->setItem(i, CA_Estado, itemEstado);

        // Barra de progreso
        int pct = 0;
        if (!c.fechaRecepcion.isEmpty() && !c.fechaVencimiento.isEmpty()) {
            QDate rec  = QDate::fromString(c.fechaRecepcion,   Qt::ISODate);
            QDate venc = QDate::fromString(c.fechaVencimiento, Qt::ISODate);
            int totalDias = rec.daysTo(venc);
            if (totalDias > 0) {
                int transcurridos = rec.daysTo(QDate::currentDate());
                pct = qBound(0, transcurridos * 100 / totalDias, 100);
            }
        }
        auto* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(pct);
        bar->setTextVisible(false);
        QString barColor = pct < 60 ? "#4CAF50" : pct < 85 ? "#FF9800" : "#F44336";
        bar->setStyleSheet(QString("QProgressBar::chunk{background:%1;}QProgressBar{border:1px solid #ccc;border-radius:3px;}").arg(barColor));
        table->setCellWidget(i, CA_Progreso, bar);
        table->setRowHeight(i, 28);
    }

    // Doble clic navega a la concesión
    connect(table, &QTableWidget::cellDoubleClicked, this, [this, table](int row, int) {
        auto* item = table->item(row, 0);
        if (item) emit navegarAConcesion(item->data(Qt::UserRole).toLongLong());
    });

    layout->addWidget(table);
    m_tabs->addTab(w, QString("Concesiones Activas (%1)").arg(m_activas.size()));
}

// ---------------------------------------------------------------------------
// Pestaña 2 — Historial de Cortes
// ---------------------------------------------------------------------------

void EmisorProfileDialog::buildTabHistorial() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(8, 8, 8, 8);

    if (m_finalizadas.isEmpty()) {
        auto* lbl = new QLabel("Este distribuidor no tiene concesiones finalizadas.");
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #777; font-style: italic;");
        layout->addWidget(lbl);
        m_tabs->addTab(w, "Historial de Cortes (0)");
        return;
    }

    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v); };

    enum ColH { CH_Folio=0, CH_Fecha, CH_Recibidas, CH_Vendidas, CH_Devueltas,
                CH_Ingresado, CH_Devuelto, CH_PctDev };

    int rows = m_finalizadas.size() + 1; // +1 para fila de totales
    auto* table = new QTableWidget(rows, 8, w);
    table->setHorizontalHeaderLabels({"Folio", "Fecha Cierre", "Recibidas", "Vendidas",
                                      "Devueltas", "Total Ingresado", "Total Devuelto ($)", "% Dev."});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(CH_Folio,     QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CH_Fecha,     QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CH_Recibidas, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CH_Vendidas,  QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CH_Devueltas, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(CH_Ingresado, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(CH_Devuelto,  QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(CH_PctDev,    QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);

    // Acumuladores para totales
    int    sumRec = 0, sumVend = 0, sumDev = 0;
    double sumIng = 0.0, sumDevVal = 0.0;

    for (int i = 0; i < m_finalizadas.size(); ++i) {
        const auto& c = m_finalizadas[i];
        const auto& ct = m_cortes[i];

        int recibidas  = ct.totalUnidadesRecibidas;
        int vendidas   = ct.totalUnidadesVendidas;
        int devueltas  = ct.totalUnidadesDevueltas;
        double ing     = ct.totalPrecioFinal;
        double devVal  = ct.totalDevolucion;
        double pctDev  = (recibidas > 0) ? devueltas * 100.0 / recibidas : 0.0;

        sumRec    += recibidas;
        sumVend   += vendidas;
        sumDev    += devueltas;
        sumIng    += ing;
        sumDevVal += devVal;

        table->setItem(i, CH_Folio,     mkItem(c.folio.isEmpty() ? "(Sin folio)" : c.folio));
        table->setItem(i, CH_Fecha,     mkCentered(c.fechaVencimiento));
        table->setItem(i, CH_Recibidas, mkCentered(QString::number(recibidas)));
        table->setItem(i, CH_Vendidas,  mkCentered(QString::number(vendidas)));
        table->setItem(i, CH_Devueltas, mkCentered(QString::number(devueltas)));
        table->setItem(i, CH_Ingresado, mkItem(fmt(ing), Qt::AlignRight | Qt::AlignVCenter));
        table->setItem(i, CH_Devuelto,  mkItem(fmt(devVal), Qt::AlignRight | Qt::AlignVCenter));

        QString pctStr = QString("%1%").arg(pctDev, 0, 'f', 1);
        auto* itemPct = mkCentered(pctStr);
        if      (pctDev < 20) colorItem(itemPct, QColor("#2E7D32"));
        else if (pctDev < 50) colorItem(itemPct, QColor("#E65100"));
        else                  colorItem(itemPct, QColor("#D32F2F"));
        table->setItem(i, CH_PctDev, itemPct);
        table->setRowHeight(i, 24);
    }

    // Fila de totales
    int last = m_finalizadas.size();
    double pctDevTotal = (sumRec > 0) ? sumDev * 100.0 / sumRec : 0.0;
    QColor totBg("#E3F2FD");
    auto addTotal = [&](int col, const QString& text, Qt::Alignment align = Qt::AlignCenter) {
        auto* item = mkItem(text, align);
        QFont bold = item->font(); bold.setBold(true); item->setFont(bold);
        item->setBackground(totBg);
        table->setItem(last, col, item);
    };
    addTotal(CH_Folio,     "TOTALES");
    addTotal(CH_Fecha,     "");
    addTotal(CH_Recibidas, QString::number(sumRec));
    addTotal(CH_Vendidas,  QString::number(sumVend));
    addTotal(CH_Devueltas, QString::number(sumDev));
    addTotal(CH_Ingresado, fmt(sumIng),    Qt::AlignRight | Qt::AlignVCenter);
    addTotal(CH_Devuelto,  fmt(sumDevVal), Qt::AlignRight | Qt::AlignVCenter);
    addTotal(CH_PctDev,    QString("%1%").arg(pctDevTotal, 0, 'f', 1));
    table->setRowHeight(last, 26);

    layout->addWidget(table);
    m_tabs->addTab(w, QString("Historial de Cortes (%1)").arg(m_finalizadas.size()));
}

// ---------------------------------------------------------------------------
// Pestaña 3 — Reporte Financiero
// ---------------------------------------------------------------------------

void EmisorProfileDialog::buildTabReporte() {
    auto* w = new QWidget();
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(16);

    QLocale loc;
    auto fmt  = [&](double v)  { return loc.toCurrencyString(v); };
    auto pct  = [](double v)   { return QString("%1%").arg(v, 0, 'f', 1); };

    const auto resumen = m_productoRepo.calcularResumenEmisor(m_emisor.id);

    // ---- Grid de métricas ----
    auto* metricsFrame = new QFrame();
    metricsFrame->setFrameShape(QFrame::StyledPanel);
    auto* grid = new QGridLayout(metricsFrame);
    grid->setContentsMargins(16, 12, 16, 12);
    grid->setHorizontalSpacing(32);
    grid->setVerticalSpacing(12);

    struct Metric { QString label; QString value; };
    QList<Metric> metrics = {
        {"Concesiones totales",         QString::number(m_activas.size() + m_finalizadas.size())},
        {"Concesiones finalizadas",      QString::number(resumen.totalConcesiones)},
        {"Rotación promedio",            pct(resumen.rotacionPromedio)},
        {"Tasa de devolución promedio",  pct(resumen.tasaDevolucionPromedio)},
        {"Total acumulado ingresado",    fmt(resumen.totalIngresado)},
        {"Total acumulado comisiones",   fmt(resumen.totalComisiones)},
        {"Total al distribuidor",        fmt(resumen.totalAlDistribuidor)},
        {"Total devuelto (valor)",       fmt(resumen.totalDevolucion)},
    };

    for (int i = 0; i < metrics.size(); ++i) {
        int row = i / 2;
        int col = (i % 2) * 2;
        auto* lbl = new QLabel(metrics[i].label);
        lbl->setStyleSheet("color: #555; font-size: 10pt;");
        auto* val = new QLabel(QString("<b>%1</b>").arg(metrics[i].value));
        QFont vf = val->font(); vf.setPointSize(vf.pointSize() + 1); val->setFont(vf);
        grid->addWidget(lbl, row * 2,     col);
        grid->addWidget(val, row * 2 + 1, col);
    }
    layout->addWidget(metricsFrame);

    // ---- Gráfica de barras (últimas 5 concesiones finalizadas) ----
    if (!m_finalizadas.isEmpty()) {
        auto* barLabel = new QLabel("<b>Ingresos vs. Devoluciones (últimas concesiones)</b>");
        layout->addWidget(barLabel);

        int n = qMin(m_finalizadas.size(), 5);
        auto* barTable = new QTableWidget(n, 4, w);
        barTable->setHorizontalHeaderLabels({"Folio", "Ingresado", "Visual", "Devuelto ($)"});
        barTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        barTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        barTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        barTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        barTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        barTable->setSelectionMode(QAbstractItemView::NoSelection);
        barTable->setShowGrid(false);
        barTable->verticalHeader()->setVisible(false);
        barTable->setMaximumHeight(n * 34 + 28);

        // Mostrar las n más recientes (primeras en m_finalizadas que está ordenado DESC)
        for (int i = 0; i < n; ++i) {
            const auto& c  = m_finalizadas[i];
            const auto& ct = m_cortes[i];

            double ing = ct.totalPrecioFinal;
            double dev = ct.totalDevolucion;
            double total = ing + dev;
            int barPct = (total > 0) ? static_cast<int>(ing * 100.0 / total) : 100;

            barTable->setItem(i, 0, mkItem(c.folio.isEmpty() ? "(Sin folio)" : c.folio));
            barTable->setItem(i, 1, mkItem(loc.toCurrencyString(ing), Qt::AlignRight | Qt::AlignVCenter));

            auto* bar = new QProgressBar();
            bar->setRange(0, 100);
            bar->setValue(barPct);
            bar->setFormat(QString("%1%").arg(barPct));
            bar->setStyleSheet(
                "QProgressBar{border:1px solid #ccc;border-radius:3px;text-align:center;}"
                "QProgressBar::chunk{background:#4CAF50;}");
            barTable->setCellWidget(i, 2, bar);

            barTable->setItem(i, 3, mkItem(loc.toCurrencyString(dev), Qt::AlignRight | Qt::AlignVCenter));
            barTable->setRowHeight(i, 28);
        }
        layout->addWidget(barTable);
    }

    layout->addStretch();
    m_tabs->addTab(w, "Reporte Financiero");
}

// ---------------------------------------------------------------------------
// Exportar PDF
// ---------------------------------------------------------------------------

void EmisorProfileDialog::onExportarPdfClicked() {
    QString fecha = QDate::currentDate().toString("yyyy-MM-dd");
    QString nombre = m_emisor.nombreEmisor;
    nombre.replace(' ', '_');
    QString defaultName = QString("Reporte_%1_%2.pdf").arg(nombre, fecha);

    QString path = QFileDialog::getSaveFileName(
        this, "Guardar reporte PDF", defaultName, "PDF (*.pdf)");
    if (path.isEmpty()) return;

    const auto resumen = m_productoRepo.calcularResumenEmisor(m_emisor.id);

    bool ok = EmisorPdfExporter::exportar(
        m_emisor, m_activas, m_finalizadas, m_cortes, resumen, path);

    if (ok)
        QMessageBox::information(this, "PDF generado",
            QString("Reporte guardado en:\n%1").arg(path));
    else
        QMessageBox::critical(this, "Error", "No se pudo generar el PDF.");
}

} // namespace App
