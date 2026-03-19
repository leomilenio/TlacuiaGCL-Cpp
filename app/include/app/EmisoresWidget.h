#pragma once
#include <QWidget>
#include "core/EmisorRepository.h"
#include "core/ConcesionRepository.h"

class QTableView;
class QPushButton;
class QStandardItemModel;

namespace App {

class EmisoresWidget : public QWidget {
    Q_OBJECT
public:
    explicit EmisoresWidget(Calculadora::EmisorRepository&   emisorRepo,
                            Calculadora::ConcesionRepository& concesionRepo,
                            QWidget* parent = nullptr);
    void refresh();

private slots:
    void onNuevoClicked();
    void onEditarClicked();
    void onEliminarClicked();

private:
    void setupUi();
    void setupConnections();

    Calculadora::EmisorRepository&    m_repo;
    Calculadora::ConcesionRepository& m_concesionRepo;
    QTableView*         m_tableView   = nullptr;
    QStandardItemModel* m_model       = nullptr;
    QPushButton*        m_btnNuevo    = nullptr;
    QPushButton*        m_btnEditar   = nullptr;
    QPushButton*        m_btnEliminar = nullptr;
};

} // namespace App
