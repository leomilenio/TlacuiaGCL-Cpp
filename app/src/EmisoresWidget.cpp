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
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace App {

// ---------------------------------------------------------------------------
// Helper: diálogo simple para crear/editar un EmisorRecord
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
    form->addRow("Nombre *:",  txtNombre);
    form->addRow("Vendedor:",  txtVendedor);
    form->addRow("Telefono:",  txtTel);
    form->addRow("Email:",     txtEmail);
    form->addRow("Notas:",     txtNotas);

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
    return true;
}

// ---------------------------------------------------------------------------
// EmisoresWidget
// ---------------------------------------------------------------------------

EmisoresWidget::EmisoresWidget(Calculadora::EmisorRepository& repo, QWidget* parent)
    : QWidget(parent)
    , m_repo(repo)
{
    setupUi();
    setupConnections();
    refresh();
}

void EmisoresWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    m_model = new QStandardItemModel(0, 5, this);
    m_model->setHorizontalHeaderLabels({"ID", "Distribuidor", "Vendedor", "Telefono", "Email"});

    m_tableView = new QTableView();
    m_tableView->setModel(m_model);
    m_tableView->setColumnHidden(0, true);           // ocultar columna ID
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setHighlightSections(false);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setFocusPolicy(Qt::NoFocus);
    m_tableView->verticalHeader()->setVisible(false);
    layout->addWidget(m_tableView);

    auto* btnRow    = new QHBoxLayout();
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
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::number(e.id))
            << new QStandardItem(e.nombreEmisor)
            << new QStandardItem(e.nombreVendedor)
            << new QStandardItem(e.telefono)
            << new QStandardItem(e.email);
        for (auto* item : row) item->setEditable(false);
        m_model->appendRow(row);
    }
    m_tableView->resizeColumnsToContents();
    m_tableView->horizontalHeader()->setStretchLastSection(true);
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
    int64_t id = m_model->item(row, 0)->text().toLongLong();
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
    QString nombre = m_model->item(row, 1)->text();
    int64_t id     = m_model->item(row, 0)->text().toLongLong();

    auto resp = QMessageBox::question(this, "Confirmar eliminacion",
        QString("¿Eliminar el distribuidor \"%1\"?\n"
                "Las concesiones vinculadas quedaran sin distribuidor asignado.").arg(nombre));
    if (resp != QMessageBox::Yes) return;

    if (!m_repo.remove(id)) {
        QMessageBox::critical(this, "Error", "No se pudo eliminar el distribuidor.");
        return;
    }
    refresh();
}

} // namespace App
