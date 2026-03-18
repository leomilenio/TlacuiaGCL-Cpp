#include "app/HistoryWidget.h"
#include "app/HistoryTableModel.h"
#include "core/ProductoRepository.h"
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>

namespace App {

HistoryWidget::HistoryWidget(Calculadora::ProductoRepository& repo, QWidget* parent)
    : QWidget(parent)
    , m_repo(repo)
{
    setupUi();
    setupConnections();
    refresh();
}

void HistoryWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    m_model     = new HistoryTableModel(this);
    m_tableView = new QTableView();
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->hide();
    m_tableView->setSortingEnabled(false);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setFocusPolicy(Qt::NoFocus);
    m_tableView->horizontalHeader()->setHighlightSections(false);

    m_deleteBtn = new QPushButton("Eliminar seleccionado");

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(m_deleteBtn);

    layout->addWidget(m_tableView);
    layout->addLayout(btnRow);
}

void HistoryWidget::setupConnections() {
    connect(m_deleteBtn, &QPushButton::clicked, this, &HistoryWidget::onDeleteSelected);
}

void HistoryWidget::refresh() {
    m_model->setRecords(m_repo.findAll());
    m_tableView->resizeColumnsToContents();
}

void HistoryWidget::onDeleteSelected() {
    QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    int row = sel.first().row();
    const auto& record = m_model->recordAt(row);

    auto answer = QMessageBox::question(this, "Confirmar eliminacion",
        QString("Eliminar el registro del %1?").arg(record.fecha),
        QMessageBox::Yes | QMessageBox::No);

    if (answer != QMessageBox::Yes) return;

    if (!m_repo.remove(record.id)) {
        QMessageBox::critical(this, "Error", "No se pudo eliminar el registro.");
        return;
    }
    refresh();
}

} // namespace App
