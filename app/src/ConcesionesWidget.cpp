#include "app/ConcesionesWidget.h"
#include <QDebug>
#include "app/NuevaConcesionDialog.h"
#include "app/AgregarProductoDialog.h"
#include "app/CorteDialog.h"
#include "app/AlertDialog.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTableWidget>
#include <QStyle>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollArea>
#include <QDialog>
#include <QLocale>
#include <QDate>
#include <QLineEdit>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QTemporaryFile>
#include <QDesktopServices>
#include <QUrl>

namespace App {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QString badgeColor(Calculadora::ConcesionStatus status) {
    switch (status) {
    case Calculadora::ConcesionStatus::Valido:      return "#4CAF50";
    case Calculadora::ConcesionStatus::VencePronto:  return "#FFC107";
    case Calculadora::ConcesionStatus::Vencida:      return "#F44336";
    default:                                          return "#9E9E9E";
    }
}

static QString badgeStyle(Calculadora::ConcesionStatus status) {
    QString bg = badgeColor(status);
    QString fg = (status == Calculadora::ConcesionStatus::VencePronto) ? "black" : "white";
    return QString("background:%1; color:%2; padding:2px 8px; border-radius:3px; font-size:11px; font-weight:bold;").arg(bg, fg);
}

static QString statusText(Calculadora::ConcesionStatus status) {
    switch (status) {
    case Calculadora::ConcesionStatus::Valido:      return "Valido";
    case Calculadora::ConcesionStatus::VencePronto:  return "Vence pronto";
    case Calculadora::ConcesionStatus::Vencida:      return "Vencida";
    default:                                          return "Pendiente";
    }
}

static QString diasLabel(const Calculadora::ConcesionRecord& c) {
    if (c.fechaVencimiento.isEmpty()) return "Sin fecha de vencimiento";
    int dias = c.diasRestantes();
    if (dias < 0)
        return QString("Vencio hace %1 dia%2").arg(-dias).arg(-dias == 1 ? "" : "s");
    if (dias == 0)
        return "Vence hoy";
    return QString("%1 dia%2 restante%3").arg(dias).arg(dias == 1 ? "" : "s").arg(dias == 1 ? "" : "s");
}

// ---------------------------------------------------------------------------
// ConcesionesWidget
// ---------------------------------------------------------------------------

ConcesionesWidget::ConcesionesWidget(Calculadora::ConcesionRepository& concesionRepo,
                                     Calculadora::EmisorRepository&    emisorRepo,
                                     Calculadora::ProductoRepository&  productoRepo,
                                     Calculadora::DocumentoRepository& documentoRepo,
                                     Calculadora::PriceCalculator&     calculator,
                                     QWidget* parent)
    : QWidget(parent)
    , m_concesionRepo(concesionRepo)
    , m_emisorRepo(emisorRepo)
    , m_productoRepo(productoRepo)
    , m_documentoRepo(documentoRepo)
    , m_calculator(calculator)
{
    setupUi();
    setupConnections();
    refresh();
}

void ConcesionesWidget::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(12, 10, 12, 12);
    outerLayout->setSpacing(8);

    // ======== HEADER ========
    auto* headerRow = new QHBoxLayout();
    auto* lblTitle  = new QLabel("<b>Concesiones</b>");
    lblTitle->setStyleSheet("font-size: 14px;");
    m_lblFecha = new QLabel(QDate::currentDate().toString("dd/MM/yyyy"));
    m_lblFecha->setStyleSheet("color: #8C8C8C; font-size: 12px;");

    m_btnVerAlertas = new QPushButton("Ver Alertas");
    m_btnVerAlertas->setObjectName("alertButton");

    m_btnNueva = new QPushButton("+ Nueva");
    m_btnNueva->setObjectName("primaryButton");

    headerRow->addWidget(lblTitle);
    headerRow->addStretch();
    headerRow->addWidget(m_lblFecha);
    headerRow->addWidget(m_btnVerAlertas);
    headerRow->addWidget(m_btnNueva);
    outerLayout->addLayout(headerRow);

    auto* headerSep = new QFrame(); headerSep->setFrameShape(QFrame::HLine);
    outerLayout->addWidget(headerSep);

    // ======== SPLITTER ========
    auto* splitter = new QSplitter(Qt::Horizontal);

    // ---- Panel izquierdo: lista ----
    auto* leftPanel  = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Buscador + filtro de estado
    auto* searchRow = new QHBoxLayout();
    m_txtBuscar = new QLineEdit();
    m_txtBuscar->setPlaceholderText("Buscar emisor o folio...");
    m_txtBuscar->setClearButtonEnabled(true);
    m_cmbFiltroStatus = new QComboBox();
    m_cmbFiltroStatus->addItem("Todas",       QVariant(-1));
    m_cmbFiltroStatus->addItem("Activas",     QVariant(0));
    m_cmbFiltroStatus->addItem("Vencidas",    QVariant(1));
    m_cmbFiltroStatus->addItem("Cerradas",    QVariant(2));
    searchRow->addWidget(m_txtBuscar, 1);
    searchRow->addWidget(m_cmbFiltroStatus);
    leftLayout->addLayout(searchRow);

    m_listWidget = new QListWidget();
    m_listWidget->setAlternatingRowColors(false);
    m_listWidget->setFocusPolicy(Qt::StrongFocus);
    m_listWidget->setMinimumWidth(280);
    m_listWidget->setSpacing(2);
    leftLayout->addWidget(m_listWidget);

    splitter->addWidget(leftPanel);

    // ---- Panel derecho: detalle (envuelto en QScrollArea) ----
    auto* rightPanel  = new QWidget();
    rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(8);

    // Detalle de concesion — tabla clave/valor
    m_detailGroup = new QGroupBox("Detalles de la Concesion");
    auto* grpLayout = new QVBoxLayout(m_detailGroup);
    grpLayout->setContentsMargins(0, 6, 0, 0);
    grpLayout->setSpacing(0);

    auto* detailTable = new QTableWidget(11, 2, m_detailGroup);
    detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detailTable->setSelectionMode(QAbstractItemView::NoSelection);
    detailTable->setFocusPolicy(Qt::NoFocus);
    detailTable->setShowGrid(false);
    detailTable->horizontalHeader()->setVisible(false);
    detailTable->verticalHeader()->setVisible(false);
    detailTable->setAlternatingRowColors(true);
    detailTable->setFrameShape(QFrame::NoFrame);
    detailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    detailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    detailTable->setColumnWidth(0, 115);

    const QStringList rowLabels = {
        "Distribuidor", "Vendedor", "Facturacion", "Folio", "Tipo",
        "Recepcion", "Vencimiento", "Estado", "Tiempo", "Comision", "Notas"
    };

    m_lblEmisor        = new QLabel("-");
    m_lblVendedor      = new QLabel("-");
    m_lblFacturacion   = new QLabel("-");
    m_lblFolio         = new QLabel("-");
    m_lblTipo          = new QLabel("-");
    m_lblRecepcion     = new QLabel("-");
    m_lblVencimiento   = new QLabel("-");
    m_lblStatus        = new QLabel("-");
    m_lblDias          = new QLabel("-");
    m_lblComision      = new QLabel("-");
    m_lblNotas         = new QLabel("-");
    m_lblNotas->setWordWrap(true);

    QLabel* valueLabels[11] = {
        m_lblEmisor, m_lblVendedor, m_lblFacturacion, m_lblFolio, m_lblTipo,
        m_lblRecepcion, m_lblVencimiento, m_lblStatus, m_lblDias,
        m_lblComision, m_lblNotas
    };

    int detailTotalHeight = 0;
    for (int i = 0; i < 11; ++i) {
        auto* keyLbl = new QLabel(rowLabels[i] + ":");
        keyLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        // palette(mid) en macOS dark mode es ~#3C3C3C, invisible sobre fondos oscuros.
        // #8C8C8C da contraste legible en modo claro (~3.5:1) y oscuro (~4:1).
        keyLbl->setStyleSheet("color: #8C8C8C; font-size: 12px; padding: 0 10px 0 6px;");
        detailTable->setCellWidget(i, 0, keyLbl);

        valueLabels[i]->setContentsMargins(6, 0, 6, 0);
        valueLabels[i]->setStyleSheet(valueLabels[i]->styleSheet()); // inherit
        detailTable->setCellWidget(i, 1, valueLabels[i]);
        int rowH = (i == 10 ? 36 : 28);
        detailTable->setRowHeight(i, rowH);
        detailTotalHeight += rowH;
    }

    // Fijar la altura exacta al contenido para evitar scroll interno.
    // frameWidth() es 0 porque usamos NoFrame, pero lo incluimos por robustez.
    detailTable->setFixedHeight(detailTotalHeight + detailTable->frameWidth() * 2);
    detailTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    grpLayout->addWidget(detailTable);
    rightLayout->addWidget(m_detailGroup);

    // Mini-tabla de productos vinculados
    auto* prodGroup  = new QGroupBox("Productos vinculados");
    auto* prodLayout = new QVBoxLayout(prodGroup);
    m_productosTable = new QTableWidget(0, 6);
    m_productosTable->setHorizontalHeaderLabels({"Producto", "Tipo", "Qty", "Precio Final", "Fecha", "Acciones"});
    m_productosTable->horizontalHeader()->setStretchLastSection(false);
    m_productosTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_productosTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_productosTable->setColumnWidth(5, 72);
    m_productosTable->horizontalHeader()->setHighlightSections(false);
    m_productosTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_productosTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_productosTable->setAlternatingRowColors(true);
    m_productosTable->setShowGrid(false);
    m_productosTable->setFocusPolicy(Qt::NoFocus);
    m_productosTable->verticalHeader()->setVisible(false);
    m_productosTable->setMinimumHeight(90);
    prodLayout->addWidget(m_productosTable);

    auto* corteBtnRow = new QHBoxLayout();
    m_btnAgregarProducto = new QPushButton("+ Agregar Producto");
    m_btnAgregarProducto->setObjectName("primaryButton");
    m_btnVerCorte        = new QPushButton("Ver Corte");
    m_btnAgregarProducto->setEnabled(false);
    m_btnVerCorte->setEnabled(false);
    corteBtnRow->addWidget(m_btnAgregarProducto);
    corteBtnRow->addStretch();
    corteBtnRow->addWidget(m_btnVerCorte);
    prodLayout->addLayout(corteBtnRow);
    rightLayout->addWidget(prodGroup);

    // Documentos adjuntos
    auto* docGroup  = new QGroupBox("Documentos adjuntos");
    auto* docLayout = new QVBoxLayout(docGroup);
    m_documentosView = new QListWidget();
    m_documentosView->setMaximumHeight(70);
    m_documentosView->setToolTip("Doble clic para abrir el archivo");
    auto* docBtnRow = new QHBoxLayout();
    m_btnAdjuntarDoc = new QPushButton("+ Adjuntar");
    m_btnAbrirDoc    = new QPushButton("Abrir");
    m_btnAdjuntarDoc->setEnabled(false);
    m_btnAbrirDoc->setEnabled(false);
    docBtnRow->addWidget(m_btnAdjuntarDoc);
    docBtnRow->addWidget(m_btnAbrirDoc);
    docBtnRow->addStretch();
    docLayout->addWidget(m_documentosView);
    docLayout->addLayout(docBtnRow);
    rightLayout->addWidget(docGroup);

    // Botones de accion de concesion
    auto* actionRow = new QHBoxLayout();
    m_btnEditar    = new QPushButton("Editar");
    m_btnEliminar  = new QPushButton("Eliminar");
    m_btnEliminar->setObjectName("dangerButton");
    m_btnFinalizar = new QPushButton("Fin de Concesion");
    m_btnEditar->setEnabled(false);
    m_btnEliminar->setEnabled(false);
    m_btnFinalizar->setEnabled(false);
    actionRow->addWidget(m_btnEditar);
    actionRow->addWidget(m_btnEliminar);
    actionRow->addStretch();
    actionRow->addWidget(m_btnFinalizar);
    rightLayout->addLayout(actionRow);

    auto* rightScroll = new QScrollArea();
    rightScroll->setWidget(rightPanel);
    rightScroll->setWidgetResizable(true);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setFrameShape(QFrame::NoFrame);
    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    outerLayout->addWidget(splitter, 1);
}

void ConcesionesWidget::setupConnections() {
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, [this](int) { onSelectionChanged(); });
    connect(m_btnNueva,           &QPushButton::clicked, this, &ConcesionesWidget::onNuevaClicked);
    connect(m_btnEditar,          &QPushButton::clicked, this, &ConcesionesWidget::onEditarClicked);
    connect(m_btnFinalizar,       &QPushButton::clicked, this, &ConcesionesWidget::onFinalizarClicked);
    connect(m_btnEliminar,        &QPushButton::clicked, this, &ConcesionesWidget::onEliminarClicked);
    connect(m_btnAgregarProducto, &QPushButton::clicked, this, &ConcesionesWidget::onAgregarProductoClicked);
    connect(m_btnVerCorte,        &QPushButton::clicked, this, &ConcesionesWidget::onVerCorteClicked);
    connect(m_btnVerAlertas,      &QPushButton::clicked, this, &ConcesionesWidget::onVerAlertasClicked);
    connect(m_btnAdjuntarDoc,     &QPushButton::clicked, this, &ConcesionesWidget::onAdjuntarDocClicked);
    connect(m_btnAbrirDoc,        &QPushButton::clicked, this, &ConcesionesWidget::onAbrirDocClicked);
    connect(m_documentosView, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem*) { onAbrirDocClicked(); });
    connect(m_documentosView, &QListWidget::currentRowChanged,
            this, [this](int row) { m_btnAbrirDoc->setEnabled(row >= 0); });
    // Buscador / filtro
    connect(m_txtBuscar, &QLineEdit::textChanged, this, &ConcesionesWidget::refresh);
    connect(m_cmbFiltroStatus, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConcesionesWidget::refresh);
}

void ConcesionesWidget::refresh() {
    m_listWidget->clear();
    clearDetailPanel();

    const QString filtroTexto  = m_txtBuscar ? m_txtBuscar->text().toLower() : QString();
    const int     filtroStatus = m_cmbFiltroStatus ? m_cmbFiltroStatus->currentData().toInt() : -1;

    const auto concesiones = m_concesionRepo.findAll();
    for (const auto& c : concesiones) {
        QString emisor = c.emisorNombre.isEmpty() ? "(Sin distribuidor)" : c.emisorNombre;
        QString folio  = c.folio.isEmpty()        ? "(Sin folio)"        : c.folio;

        // Filtro de texto (emisor o folio)
        if (!filtroTexto.isEmpty()) {
            if (!emisor.toLower().contains(filtroTexto) &&
                !folio.toLower().contains(filtroTexto))
                continue;
        }

        // Filtro de status: 0=activas, 1=vencidas, 2=cerradas, -1=todas
        if (filtroStatus == 0 && !c.activa) continue;
        if (filtroStatus == 1 && (!c.activa || c.status() != Calculadora::ConcesionStatus::Vencida)) continue;
        if (filtroStatus == 2 && c.activa)  continue;

        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(c.id)));
        item->setSizeHint(QSize(0, 54));
        m_listWidget->addItem(item);

        // --- Widget personalizado para cada fila ---
        auto* rowWidget = new QWidget();
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 6, 0);
        rowLayout->setSpacing(6);

        // Barra de color izquierda (indica estado)
        auto st = c.activa ? c.status() : Calculadora::ConcesionStatus::Pendiente;
        // Para pre-corte usar color naranja en la barra lateral
        const QString barColor = (!c.activa) ? "#9E9E9E"
                               : c.enPrecorte ? "#E65100"
                               : badgeColor(st);
        auto* colorBar = new QFrame();
        colorBar->setFixedWidth(4);
        colorBar->setStyleSheet(
            QString("background-color: %1; border-radius: 2px;").arg(barColor));

        // Texto: emisor + folio en una linea, dias en otra
        auto* textBlock  = new QWidget();
        auto* textLayout = new QVBoxLayout(textBlock);
        textLayout->setContentsMargins(4, 4, 0, 4);
        textLayout->setSpacing(1);

        auto* topLine = new QLabel(
            QString("<b>%1</b>  <span style='color:gray;'>Folio: %2</span>")
                .arg(emisor.toHtmlEscaped(), folio.toHtmlEscaped()));
        topLine->setTextFormat(Qt::RichText);

        QString diasStr;
        if (!c.activa) {
            diasStr = "<span style='color:#9E9E9E;'>Finalizada</span>";
        } else {
            QString color = (st == Calculadora::ConcesionStatus::Vencida) ? "#F44336"
                          : (st == Calculadora::ConcesionStatus::VencePronto) ? "#E65100"
                          : "#4CAF50";
            diasStr = QString("<span style='color:%1; font-size:11px;'>%2</span>")
                        .arg(color, diasLabel(c));
        }
        auto* bottomLine = new QLabel(diasStr);
        bottomLine->setTextFormat(Qt::RichText);

        textLayout->addWidget(topLine);
        textLayout->addWidget(bottomLine);

        // Badge de estado
        QString badgeTxt;
        QString badgeSty;
        if (!c.activa) {
            badgeTxt = "Cerrada";
            badgeSty = "background:#9E9E9E; color:white; padding:2px 8px; border-radius:3px; font-size:11px; font-weight:bold;";
        } else if (c.enPrecorte) {
            badgeTxt = "Pre-corte";
            badgeSty = "background:#E65100; color:white; padding:2px 8px; border-radius:3px; font-size:11px; font-weight:bold;";
        } else {
            badgeTxt = statusText(st);
            badgeSty = badgeStyle(st);
        }
        auto* badge = new QLabel(badgeTxt);
        badge->setStyleSheet(badgeSty);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedHeight(22);
        badge->setMinimumWidth(70);

        rowLayout->addWidget(colorBar);
        rowLayout->addWidget(textBlock, 1);
        rowLayout->addWidget(badge);

        m_listWidget->setItemWidget(item, rowWidget);
    }

    // Actualizar fecha
    if (m_lblFecha)
        m_lblFecha->setText(QDate::currentDate().toString("dd/MM/yyyy"));
}

void ConcesionesWidget::selectById(int64_t concesionId) {
    for (int i = 0; i < m_listWidget->count(); ++i) {
        auto* item = m_listWidget->item(i);
        if (item && item->data(Qt::UserRole).toLongLong() == concesionId) {
            m_listWidget->setCurrentItem(item);
            return;
        }
    }
}

void ConcesionesWidget::onSelectionChanged() {
    auto* item = m_listWidget->currentItem();
    bool hasSel = (item != nullptr);
    m_btnEditar->setEnabled(hasSel);
    m_btnEliminar->setEnabled(hasSel);

    if (!hasSel) {
        m_btnFinalizar->setEnabled(false);
        m_btnAgregarProducto->setEnabled(false);
        m_btnVerCorte->setEnabled(false);
        clearDetailPanel();
        return;
    }

    int64_t id = item->data(Qt::UserRole).toLongLong();
    Calculadora::ConcesionRecord rec = m_concesionRepo.findById(id);
    m_btnFinalizar->setEnabled(rec.activa);
    m_btnAgregarProducto->setEnabled(rec.activa);
    updateDetailPanel(rec);
}

void ConcesionesWidget::updateDetailPanel(const Calculadora::ConcesionRecord& rec) {
    m_lblEmisor->setText(rec.emisorNombre.isEmpty() ? "-" : rec.emisorNombre);
    m_lblVendedor->setText(rec.emisorNombreVendedor.isEmpty() ? "-" : rec.emisorNombreVendedor);
    m_lblFacturacion->setText(rec.emisorFacturacion ? "Proveedor factura" : "Proveedor no factura");
    // #43A047 (verde 600) es visible tanto en modo claro como oscuro.
    // #E65100 (naranja) tiene suficiente luminosidad para ambos modos.
    m_lblFacturacion->setStyleSheet(rec.emisorFacturacion
        ? "color: #43A047; font-size: 12px; padding-left: 6px;"
        : "color: #E65100; font-size: 12px; padding-left: 6px;");
    m_lblFolio->setText(rec.folio.isEmpty() ? "-" : rec.folio);

    static const QMap<Calculadora::TipoDocumentoConcesion, QString> tipoDisplay = {
        { Calculadora::TipoDocumentoConcesion::Factura,        "Factura" },
        { Calculadora::TipoDocumentoConcesion::NotaDeCredito,  "Nota de credito" },
        { Calculadora::TipoDocumentoConcesion::NotaDeRemision, "Nota de remision" },
        { Calculadora::TipoDocumentoConcesion::Otro,           "Otro" },
    };
    m_lblTipo->setText(tipoDisplay.value(rec.tipoDocumento, "Factura"));
    m_lblRecepcion->setText(rec.fechaRecepcion.isEmpty() ? "-" : rec.fechaRecepcion);
    m_lblVencimiento->setText(rec.fechaVencimiento.isEmpty() ? "-" : rec.fechaVencimiento);

    auto st = rec.status();
    m_lblStatus->setText(statusText(st));
    m_lblStatus->setStyleSheet(badgeStyle(st));

    // Dias restantes — prominente
    m_lblDias->setText(diasLabel(rec));
    QString diasColor = (st == Calculadora::ConcesionStatus::Vencida) ? "#F44336"
                      : (st == Calculadora::ConcesionStatus::VencePronto) ? "#E65100"
                      : "palette(window-text)";
    m_lblDias->setStyleSheet(QString("color: %1; font-weight: bold;").arg(diasColor));

    m_lblComision->setText(QString("%1%").arg(rec.comisionPct, 0, 'f', 1));
    m_lblNotas->setText(rec.notas.isEmpty() ? "-" : rec.notas);

    // Cargar documentos adjuntos
    m_documentosView->clear();
    const auto docs = m_documentoRepo.findByConcesion(rec.id);
    for (const auto& d : docs) {
        auto* docItem = new QListWidgetItem(
            QString("[%1]  %2").arg(d.tipo, d.nombre));
        docItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(d.id)));
        m_documentosView->addItem(docItem);
    }
    m_btnAdjuntarDoc->setEnabled(true);
    m_btnAbrirDoc->setEnabled(false);

    // Cargar productos vinculados
    m_productosTable->setRowCount(0);
    const auto vinculadosList = m_productoRepo.findByConcesion(rec.id);
    QLocale loc;
    int vinculados = static_cast<int>(vinculadosList.size());
    bool concesionActiva = rec.activa;
    for (const auto& p : vinculadosList) {
        int row = m_productosTable->rowCount();
        m_productosTable->insertRow(row);

        auto mkItem = [](const QString& txt) {
            auto* it = new QTableWidgetItem(txt);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        m_productosTable->setItem(row, 0, mkItem(p.nombreProducto));
        m_productosTable->setItem(row, 1, mkItem(p.tipoProducto == Calculadora::TipoProducto::Libro ? "Libro" : "Papeleria"));
        m_productosTable->setItem(row, 2, mkItem(QString::number(p.cantidadRecibida)));
        m_productosTable->setItem(row, 3, mkItem(loc.toCurrencyString(p.precioFinal)));
        m_productosTable->setItem(row, 4, mkItem(p.fecha.left(10)));

        // Columna Acciones: botones Editar y Eliminar con iconos Qt estandar
        auto* actWidget  = new QWidget();
        auto* actLayout  = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(4, 2, 4, 2);
        actLayout->setSpacing(4);

        auto* btnEdit = new QPushButton();
        btnEdit->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        btnEdit->setToolTip("Editar producto");
        btnEdit->setFlat(true);
        btnEdit->setFixedSize(28, 28);
        btnEdit->setEnabled(concesionActiva);

        auto* btnDel = new QPushButton();
        btnDel->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        btnDel->setToolTip("Eliminar producto");
        btnDel->setFlat(true);
        btnDel->setFixedSize(28, 28);
        btnDel->setEnabled(concesionActiva);

        int64_t pid = p.id;
        connect(btnEdit, &QPushButton::clicked, this, [this, pid]() { onEditarProducto(pid); });
        connect(btnDel,  &QPushButton::clicked, this, [this, pid]() { onEliminarProducto(pid); });

        actLayout->addWidget(btnEdit);
        actLayout->addWidget(btnDel);
        m_productosTable->setCellWidget(row, 5, actWidget);
        m_productosTable->setRowHeight(row, 34);
    }
    m_productosTable->resizeColumnsToContents();
    m_productosTable->setColumnWidth(5, 72);  // fijar ancho de Acciones
    m_btnVerCorte->setEnabled(vinculados > 0);
}

void ConcesionesWidget::clearDetailPanel() {
    const QString dash = "-";
    m_lblEmisor->setText(dash);
    m_lblVendedor->setText(dash);
    m_lblFolio->setText(dash);
    m_lblTipo->setText(dash);
    m_lblRecepcion->setText(dash);
    m_lblVencimiento->setText(dash);
    m_lblStatus->setText(dash);
    m_lblStatus->setStyleSheet("");
    m_lblDias->setText(dash);
    m_lblDias->setStyleSheet("");
    m_lblComision->setText(dash);
    m_lblNotas->setText(dash);
    m_productosTable->setRowCount(0);
    m_documentosView->clear();
    m_btnAgregarProducto->setEnabled(false);
    m_btnVerCorte->setEnabled(false);
    m_btnAdjuntarDoc->setEnabled(false);
    m_btnAbrirDoc->setEnabled(false);
}

void ConcesionesWidget::onNuevaClicked() {
    NuevaConcesionDialog dlg(m_emisorRepo, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Calculadora::ConcesionRecord rec = dlg.result();

    bool nuevoEmisor = dlg.isNuevoEmisor();
    if (nuevoEmisor) {
        Calculadora::EmisorRecord e;
        e.nombreEmisor   = dlg.nuevoEmisorNombre();
        e.nombreVendedor = dlg.nuevoEmisorVendedor();
        e.telefono       = dlg.nuevoEmisorTelefono();
        e.email          = dlg.nuevoEmisorEmail();
        e.facturacion    = dlg.nuevoEmisorFacturacion();
        int64_t eid = m_emisorRepo.save(e);
        if (eid < 0) {
            QMessageBox::critical(this, "Error", "No se pudo guardar el distribuidor.");
            return;
        }
        rec.emisorId = eid;
    }

    int64_t nuevaId = m_concesionRepo.save(rec);
    if (nuevaId < 0) {
        QMessageBox::critical(this, "Error", "No se pudo guardar la concesion.");
        return;
    }

    // Guardar documentos adjuntos seleccionados en el dialog
    for (const QString& path : dlg.adjuntosSeleccionados()) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QFileInfo fi(path);
        QString tipo = fi.suffix().toLower() == "pdf" ? "PDF" : "Excel";
        if (m_documentoRepo.save(nuevaId, fi.fileName(), tipo, f.readAll()) < 0)
            qWarning() << "ConcesionesWidget: no se pudo guardar el documento:" << fi.fileName();
    }

    if (nuevoEmisor) emit emisorCreado();
    emit concesionesModificadas();
    refresh();
}

void ConcesionesWidget::onEditarClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id  = item->data(Qt::UserRole).toLongLong();
    auto    rec = m_concesionRepo.findById(id);

    NuevaConcesionDialog dlg(rec, m_emisorRepo, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Calculadora::ConcesionRecord updated = dlg.result();
    updated.id = id;

    bool nuevoEmisorEdit = dlg.isNuevoEmisor();
    if (nuevoEmisorEdit) {
        Calculadora::EmisorRecord e;
        e.nombreEmisor   = dlg.nuevoEmisorNombre();
        e.nombreVendedor = dlg.nuevoEmisorVendedor();
        e.telefono       = dlg.nuevoEmisorTelefono();
        e.email          = dlg.nuevoEmisorEmail();
        e.facturacion    = dlg.nuevoEmisorFacturacion();
        int64_t eid = m_emisorRepo.save(e);
        if (eid < 0) {
            QMessageBox::critical(this, "Error", "No se pudo guardar el distribuidor.");
            return;
        }
        updated.emisorId = eid;
    }

    if (!m_concesionRepo.update(updated)) {
        QMessageBox::critical(this, "Error", "No se pudo actualizar la concesion.");
        return;
    }
    if (nuevoEmisorEdit) emit emisorCreado();
    emit concesionesModificadas();
    refresh();
}

void ConcesionesWidget::onFinalizarClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id = item->data(Qt::UserRole).toLongLong();

    auto resp = QMessageBox::question(this, "Finalizar concesion",
        "Marcar esta concesion como finalizada?\n"
        "No se eliminaran los datos; solo se cerrara la concesion.");
    if (resp != QMessageBox::Yes) return;

    if (!m_concesionRepo.finalizar(id)) {
        QMessageBox::critical(this, "Error", "No se pudo finalizar la concesion.");
        return;
    }
    emit concesionesModificadas();
    refresh();
}

void ConcesionesWidget::onEliminarClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id = item->data(Qt::UserRole).toLongLong();

    auto resp = QMessageBox::question(this, "Eliminar concesion",
        "Eliminar permanentemente esta concesion?\n"
        "Los productos vinculados quedaran sin concesion asignada.");
    if (resp != QMessageBox::Yes) return;

    if (!m_concesionRepo.remove(id)) {
        QMessageBox::critical(this, "Error", "No se pudo eliminar la concesion.");
        return;
    }
    emit concesionesModificadas();
    refresh();
}

void ConcesionesWidget::onAgregarProductoClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id  = item->data(Qt::UserRole).toLongLong();
    auto    rec = m_concesionRepo.findById(id);

    QString emisor = rec.emisorNombre.isEmpty() ? "(Sin distribuidor)" : rec.emisorNombre;
    QString folio  = rec.folio.isEmpty()        ? "(Sin folio)"        : rec.folio;
    QString label  = QString("%1 — %2").arg(emisor, folio);

    AgregarProductoDialog dlg(id, label, m_calculator, rec.comisionPct, rec.emisorFacturacion, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const auto& r = dlg.calculationResult();
    Calculadora::ProductoRecord record;
    record.nombreProducto   = dlg.nombreProducto();
    record.tipoProducto     = dlg.tipoProducto();
    record.isbn             = dlg.isbn();
    record.precioFinal      = r.precioFinal;
    record.costo            = r.costo;
    record.comision         = r.comision;
    record.ivaTrasladado    = r.ivaTrasladado;
    record.ivaAcreditable   = r.ivaAcreditable;
    record.ivaNetoPagar     = r.ivaNetoPagar;
    record.escenario        = r.escenario;
    record.tieneCFDI        = r.tieneCFDI;
    record.nombreProveedor  = rec.emisorNombre;
    record.nombreVendedor   = rec.emisorNombreVendedor;
    record.concesionId      = id;
    record.cantidadRecibida = dlg.cantidad();

    if (m_productoRepo.save(record) < 0) {
        QMessageBox::critical(this, "Error", "No se pudo guardar el producto.");
        return;
    }
    emit productoAgregado();
    updateDetailPanel(rec);
}

void ConcesionesWidget::onVerCorteClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id       = item->data(Qt::UserRole).toLongLong();
    auto    rec      = m_concesionRepo.findById(id);
    auto    productos = m_productoRepo.findByConcesion(id);

    if (productos.isEmpty()) {
        QMessageBox::information(this, "Corte de concesion",
            "No hay productos calculados vinculados a esta concesion.");
        return;
    }

    CorteDialog dlg(rec, productos, m_productoRepo, m_concesionRepo, this);
    dlg.exec();
    if (dlg.result() == QDialog::Accepted) {
        emit concesionesModificadas();
        refresh();
    }
}

void ConcesionesWidget::onAdjuntarDocClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t id = item->data(Qt::UserRole).toLongLong();

    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Seleccionar documentos", QString(),
        "Documentos soportados (*.pdf *.xlsx *.xls);;PDF (*.pdf);;Excel (*.xlsx *.xls)");
    for (const QString& path : files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QFileInfo fi(path);
        QString tipo = fi.suffix().toLower() == "pdf" ? "PDF" : "Excel";
        if (m_documentoRepo.save(id, fi.fileName(), tipo, f.readAll()) < 0) {
            QMessageBox::warning(this, "Error",
                QString("No se pudo guardar: %1").arg(fi.fileName()));
        }
    }
    // Refrescar lista de docs
    auto rec = m_concesionRepo.findById(id);
    updateDetailPanel(rec);
}

void ConcesionesWidget::onAbrirDocClicked() {
    auto* docItem = m_documentosView->currentItem();
    if (!docItem) return;
    int64_t docId = docItem->data(Qt::UserRole).toLongLong();

    // Extraer nombre para saber la extensión
    QString nombre = docItem->text();
    // nombre tiene formato "[PDF]  archivo.pdf"
    int lastSpace = nombre.lastIndexOf(' ');
    QString filename = (lastSpace >= 0) ? nombre.mid(lastSpace + 1) : nombre;

    QByteArray contenido = m_documentoRepo.getContenido(docId);
    if (contenido.isEmpty()) {
        QMessageBox::warning(this, "Error", "No se pudo cargar el documento.");
        return;
    }

    // Guardar en archivo temporal y abrir
    QString suffix = QFileInfo(filename).suffix();
    QTemporaryFile tmp;
    tmp.setAutoRemove(false);
    tmp.setFileTemplate(QDir::tempPath() + "/tlacuia_XXXXXX." + suffix);
    if (tmp.open()) {
        tmp.write(contenido);
        tmp.close();
        QDesktopServices::openUrl(QUrl::fromLocalFile(tmp.fileName()));
    } else {
        QMessageBox::warning(this, "Error", "No se pudo crear el archivo temporal.");
    }
}

void ConcesionesWidget::onEditarProducto(int64_t productoId) {
    // Buscar el producto en la concesion actualmente seleccionada
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t concesionId = item->data(Qt::UserRole).toLongLong();
    auto    rec         = m_concesionRepo.findById(concesionId);

    const auto productos = m_productoRepo.findByConcesion(concesionId);
    Calculadora::ProductoRecord existing;
    bool found = false;
    for (const auto& p : productos) {
        if (p.id == productoId) { existing = p; found = true; break; }
    }
    if (!found) return;

    QString emisor = rec.emisorNombre.isEmpty() ? "(Sin distribuidor)" : rec.emisorNombre;
    QString folio  = rec.folio.isEmpty()        ? "(Sin folio)"        : rec.folio;
    QString label  = QString("%1 — %2").arg(emisor, folio);

    AgregarProductoDialog dlg(existing, label, m_calculator, rec.comisionPct, rec.emisorFacturacion, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const auto& r = dlg.calculationResult();
    existing.nombreProducto   = dlg.nombreProducto();
    existing.tipoProducto     = dlg.tipoProducto();
    existing.isbn             = dlg.isbn();
    existing.cantidadRecibida = dlg.cantidad();
    existing.precioFinal      = r.precioFinal;
    existing.costo            = r.costo;
    existing.comision         = r.comision;
    existing.ivaTrasladado    = r.ivaTrasladado;
    existing.ivaAcreditable   = r.ivaAcreditable;
    existing.ivaNetoPagar     = r.ivaNetoPagar;
    existing.escenario        = r.escenario;
    existing.tieneCFDI        = r.tieneCFDI;

    if (!m_productoRepo.update(existing)) {
        QMessageBox::critical(this, "Error", "No se pudo actualizar el producto.");
        return;
    }
    updateDetailPanel(rec);
}

void ConcesionesWidget::onEliminarProducto(int64_t productoId) {
    auto resp = QMessageBox::question(this, "Eliminar producto",
        "Eliminar permanentemente este producto de la concesion?");
    if (resp != QMessageBox::Yes) return;

    if (!m_productoRepo.remove(productoId)) {
        QMessageBox::critical(this, "Error", "No se pudo eliminar el producto.");
        return;
    }

    auto* item = m_listWidget->currentItem();
    if (!item) return;
    int64_t concesionId = item->data(Qt::UserRole).toLongLong();
    auto    rec         = m_concesionRepo.findById(concesionId);
    updateDetailPanel(rec);
}

void ConcesionesWidget::onVerAlertasClicked() {
    // Construir la lista de alertas (misma logica que la splash de arranque)
    QList<Calculadora::ConcesionRecord> alertas;

    const auto todas = m_concesionRepo.findAll();
    for (const auto& c : todas) {
        if (!c.activa) continue;
        auto st = c.status();
        if (st == Calculadora::ConcesionStatus::Vencida ||
            st == Calculadora::ConcesionStatus::VencePronto) {
            alertas.append(c);
        }
    }

    if (alertas.isEmpty()) {
        QMessageBox::information(this, "Alertas de concesiones",
            "No hay alertas activas.\nTodas las concesiones estan al dia.");
        return;
    }

    AlertDialog dlg(alertas, this);
    dlg.exec();
}

} // namespace App
