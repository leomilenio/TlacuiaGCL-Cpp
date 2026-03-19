#include "app/EmisoresWidget.h"
#include "app/EmisorProfileDialog.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>

namespace App {

// ---------------------------------------------------------------------------
// Columnas de la tabla
// ---------------------------------------------------------------------------
enum Col {
    ColId = 0,       // Oculta — almacena el id del emisor
    ColNombre,
    ColVendedor,
    ColTel,
    ColEmail,
    ColFactura,
    ColConcesiones,
    ColAcciones
};
static constexpr int kNumCols = 8;

// ---------------------------------------------------------------------------
// Helper: diálogo para crear/editar un EmisorRecord
// ---------------------------------------------------------------------------
static bool openEmisorDialog(Calculadora::EmisorRecord& rec, QWidget* parent) {
    QDialog dlg(parent);
    dlg.setWindowTitle(rec.id == 0 ? "Nuevo Distribuidor" : "Editar Distribuidor");
    dlg.setMinimumWidth(360);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    auto* txtNombre   = new QLineEdit(rec.nombreEmisor);   txtNombre->setPlaceholderText("Requerido");
    auto* txtVendedor = new QLineEdit(rec.nombreVendedor);
    auto* txtTel      = new QLineEdit(rec.telefono);
    auto* txtEmail    = new QLineEdit(rec.email);
    auto* txtNotas    = new QLineEdit(rec.notas);
    auto* chkFactura  = new QCheckBox("Emite facturas (CFDI)");
    chkFactura->setChecked(rec.facturacion);

    form->addRow("Nombre *:",    txtNombre);
    form->addRow("Vendedor:",    txtVendedor);
    form->addRow("Telefono:",    txtTel);
    form->addRow("Email:",       txtEmail);
    form->addRow("Notas:",       txtNotas);
    form->addRow("Facturacion:", chkFactura);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (txtNombre->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, "Campo requerido", "El nombre del distribuidor es obligatorio.");
            txtNombre->setFocus();
            return;
        }
        dlg.accept();
    });
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return false;

    rec.nombreEmisor   = txtNombre->text().trimmed();
    rec.nombreVendedor = txtVendedor->text().trimmed();
    rec.telefono       = txtTel->text().trimmed();
    rec.email          = txtEmail->text().trimmed();
    rec.notas          = txtNotas->text().trimmed();
    rec.facturacion    = chkFactura->isChecked();
    return true;
}

// ---------------------------------------------------------------------------
// EmisoresWidget
// ---------------------------------------------------------------------------

EmisoresWidget::EmisoresWidget(Calculadora::EmisorRepository&    emisorRepo,
                               Calculadora::ConcesionRepository& concesionRepo,
                               Calculadora::ProductoRepository&  productoRepo,
                               QWidget* parent)
    : QWidget(parent)
    , m_repo(emisorRepo)
    , m_concesionRepo(concesionRepo)
    , m_productoRepo(productoRepo)
{
    setupUi();
    setupConnections();
    refresh();
}

void EmisoresWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    m_table = new QTableWidget(0, kNumCols, this);
    m_table->setHorizontalHeaderLabels(
        {"ID", "Distribuidor", "Vendedor", "Telefono", "Email",
         "Factura", "Concesiones activas", "Acciones"});
    m_table->setColumnHidden(ColId, true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColNombre,      QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColVendedor,    QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColTel,         QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColEmail,       QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColFactura,     QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColConcesiones, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColAcciones,    QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    auto* btnRow  = new QHBoxLayout();
    m_btnNuevo    = new QPushButton("Nuevo");
    m_btnEditar   = new QPushButton("Editar");
    m_btnEliminar = new QPushButton("Eliminar");
    m_btnNuevo->setObjectName("primaryButton");
    m_btnEditar->setEnabled(false);
    m_btnEliminar->setEnabled(false);
    btnRow->addWidget(m_btnNuevo);
    btnRow->addWidget(m_btnEditar);
    btnRow->addWidget(m_btnEliminar);
    btnRow->addStretch();
    layout->addLayout(btnRow);
}

void EmisoresWidget::setupConnections() {
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool sel = (m_table->currentRow() >= 0);
        m_btnEditar->setEnabled(sel);
        m_btnEliminar->setEnabled(sel);
    });
    connect(m_btnNuevo,    &QPushButton::clicked, this, &EmisoresWidget::onNuevoClicked);
    connect(m_btnEditar,   &QPushButton::clicked, this, &EmisoresWidget::onEditarClicked);
    connect(m_btnEliminar, &QPushButton::clicked, this, &EmisoresWidget::onEliminarClicked);
}

void EmisoresWidget::refresh() {
    m_table->setRowCount(0);
    const auto emisores = m_repo.findAll();

    for (const auto& e : emisores) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Columna ID (oculta)
        auto* itemId = new QTableWidgetItem(QString::number(e.id));
        itemId->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        m_table->setItem(row, ColId, itemId);

        // Nombre, Vendedor, Tel, Email
        auto mkItem = [](const QString& t) {
            auto* it = new QTableWidgetItem(t);
            it->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            return it;
        };
        m_table->setItem(row, ColNombre,   mkItem(e.nombreEmisor));
        m_table->setItem(row, ColVendedor, mkItem(e.nombreVendedor));
        m_table->setItem(row, ColTel,      mkItem(e.telefono));
        m_table->setItem(row, ColEmail,    mkItem(e.email));

        // Columna Factura
        auto* itemFactura = mkItem(e.facturacion ? "✓  Factura" : "✗  No factura");
        itemFactura->setForeground(QColor(e.facturacion ? "#43A047" : "#E65100"));
        itemFactura->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, ColFactura, itemFactura);

        // Columna Concesiones activas: puntos de color
        int activas = m_concesionRepo.countActiveByEmisor(e.id);
        QString dotsText;
        if (activas == 0) {
            dotsText = "—";
        } else {
            int dots = qMin(activas, 5);
            for (int d = 0; d < dots; ++d) dotsText += "●";
            if (activas > 5) dotsText += QString("  +%1").arg(activas - 5);
        }
        auto* itemConc = mkItem(dotsText);
        itemConc->setForeground(activas > 0 ? QColor("#43A047") : QColor("#8C8C8C"));
        itemConc->setTextAlignment(Qt::AlignCenter);
        itemConc->setToolTip(QString("%1 concesion%2 activa%3")
            .arg(activas).arg(activas != 1 ? "es" : "").arg(activas != 1 ? "s" : ""));
        m_table->setItem(row, ColConcesiones, itemConc);

        // Columna Acciones: botón "Ver"
        auto* btnVer = new QPushButton();
        btnVer->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        btnVer->setToolTip("Ver perfil del distribuidor");
        btnVer->setFixedSize(28, 24);
        btnVer->setFlat(true);
        int64_t eid = e.id;
        connect(btnVer, &QPushButton::clicked, this, [this, eid]() {
            onVerPerfilClicked(eid);
        });

        auto* container = new QWidget();
        auto* hlay = new QHBoxLayout(container);
        hlay->setContentsMargins(4, 0, 4, 0);
        hlay->setAlignment(Qt::AlignCenter);
        hlay->addWidget(btnVer);
        m_table->setCellWidget(row, ColAcciones, container);

        m_table->setRowHeight(row, 26);
    }
}

void EmisoresWidget::onVerPerfilClicked(int64_t emisorId) {
    Calculadora::EmisorRecord rec = m_repo.findById(emisorId);
    auto* dlg = new EmisorProfileDialog(rec, m_concesionRepo, m_productoRepo, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &EmisorProfileDialog::navegarAConcesion,
            this, &EmisoresWidget::navegarAConcesion);
    dlg->exec();
}

void EmisoresWidget::onNuevoClicked() {
    Calculadora::EmisorRecord rec;
    if (!openEmisorDialog(rec, this)) return;
    if (m_repo.save(rec) < 0) {
        QMessageBox::critical(this, "Error", "No se pudo guardar el distribuidor.");
        return;
    }
    refresh();
}

void EmisoresWidget::onEditarClicked() {
    int row = m_table->currentRow();
    if (row < 0) return;
    int64_t id = m_table->item(row, ColId)->text().toLongLong();
    Calculadora::EmisorRecord rec = m_repo.findById(id);
    if (!openEmisorDialog(rec, this)) return;
    if (!m_repo.update(rec)) {
        QMessageBox::critical(this, "Error", "No se pudo actualizar el distribuidor.");
        return;
    }
    refresh();
}

void EmisoresWidget::onEliminarClicked() {
    int row = m_table->currentRow();
    if (row < 0) return;
    QString nombre = m_table->item(row, ColNombre)->text();
    int64_t id     = m_table->item(row, ColId)->text().toLongLong();

    auto resp = QMessageBox::question(this, "Confirmar eliminacion",
        QString("Eliminar el distribuidor \"%1\"?\n"
                "Las concesiones vinculadas quedaran sin distribuidor asignado.").arg(nombre));
    if (resp != QMessageBox::Yes) return;

    if (!m_repo.remove(id)) {
        QMessageBox::critical(this, "Error", "No se pudo eliminar el distribuidor.");
        return;
    }
    refresh();
}

} // namespace App
