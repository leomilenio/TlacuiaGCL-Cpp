#include "app/EmisoresWidget.h"
#include <QTableView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace App {

// ---------------------------------------------------------------------------
// Columnas del modelo (la col 0 "ID" queda oculta)
// ---------------------------------------------------------------------------
enum Col { ColId = 0, ColNombre, ColVendedor, ColTel, ColEmail, ColFactura, ColConcesiones };
static constexpr int kNumCols = 7;

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

EmisoresWidget::EmisoresWidget(Calculadora::EmisorRepository&   emisorRepo,
                               Calculadora::ConcesionRepository& concesionRepo,
                               QWidget* parent)
    : QWidget(parent)
    , m_repo(emisorRepo)
    , m_concesionRepo(concesionRepo)
{
    setupUi();
    setupConnections();
    refresh();
}

void EmisoresWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    m_model = new QStandardItemModel(0, kNumCols, this);
    m_model->setHorizontalHeaderLabels(
        {"ID", "Distribuidor", "Vendedor", "Telefono", "Email", "Factura", "Concesiones activas"});

    m_tableView = new QTableView();
    m_tableView->setModel(m_model);
    m_tableView->setColumnHidden(ColId, true);
    m_tableView->horizontalHeader()->setStretchLastSection(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColNombre,    QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColVendedor,  QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColTel,       QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColEmail,     QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColFactura,   QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(ColConcesiones, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setHighlightSections(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setFocusPolicy(Qt::NoFocus);
    m_tableView->verticalHeader()->setVisible(false);
    layout->addWidget(m_tableView);

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
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
                bool sel = m_tableView->currentIndex().isValid();
                m_btnEditar->setEnabled(sel);
                m_btnEliminar->setEnabled(sel);
            });
    connect(m_btnNuevo,    &QPushButton::clicked, this, &EmisoresWidget::onNuevoClicked);
    connect(m_btnEditar,   &QPushButton::clicked, this, &EmisoresWidget::onEditarClicked);
    connect(m_btnEliminar, &QPushButton::clicked, this, &EmisoresWidget::onEliminarClicked);
}

void EmisoresWidget::refresh() {
    m_model->removeRows(0, m_model->rowCount());
    const auto emisores = m_repo.findAll();
    for (const auto& e : emisores) {
        // --- Columna Factura ---
        auto* itemFactura = new QStandardItem(e.facturacion ? "✓  Factura" : "✗  No factura");
        itemFactura->setForeground(QColor(e.facturacion ? "#43A047" : "#E65100"));
        itemFactura->setTextAlignment(Qt::AlignCenter);
        itemFactura->setEditable(false);

        // --- Columna Concesiones activas: puntos de color ---
        int activas = m_concesionRepo.countActiveByEmisor(e.id);
        QString dotsText;
        if (activas == 0) {
            dotsText = "—";
        } else {
            int dots = qMin(activas, 5);
            for (int d = 0; d < dots; ++d) dotsText += "●";
            if (activas > 5) dotsText += QString("  +%1").arg(activas - 5);
        }
        auto* itemConcesiones = new QStandardItem(dotsText);
        itemConcesiones->setForeground(activas > 0 ? QColor("#43A047") : QColor("#8C8C8C"));
        itemConcesiones->setTextAlignment(Qt::AlignCenter);
        itemConcesiones->setToolTip(QString("%1 concesion%2 activa%3")
            .arg(activas).arg(activas != 1 ? "es" : "").arg(activas != 1 ? "s" : ""));
        itemConcesiones->setEditable(false);

        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(e.id))
            << new QStandardItem(e.nombreEmisor)
            << new QStandardItem(e.nombreVendedor)
            << new QStandardItem(e.telefono)
            << new QStandardItem(e.email)
            << itemFactura
            << itemConcesiones;
        for (int i = 0; i < ColFactura; ++i) row[i]->setEditable(false);
        m_model->appendRow(row);
    }
    m_tableView->resizeColumnsToContents();
    // Nombre sigue siendo la columna elástica
    m_tableView->horizontalHeader()->setSectionResizeMode(ColNombre, QHeaderView::Stretch);
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
    int row = m_tableView->currentIndex().row();
    if (row < 0) return;
    int64_t id = m_model->item(row, ColId)->text().toLongLong();
    Calculadora::EmisorRecord rec = m_repo.findById(id);
    if (!openEmisorDialog(rec, this)) return;
    if (!m_repo.update(rec)) {
        QMessageBox::critical(this, "Error", "No se pudo actualizar el distribuidor.");
        return;
    }
    refresh();
}

void EmisoresWidget::onEliminarClicked() {
    int row = m_tableView->currentIndex().row();
    if (row < 0) return;
    QString nombre = m_model->item(row, ColNombre)->text();
    int64_t id     = m_model->item(row, ColId)->text().toLongLong();

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
