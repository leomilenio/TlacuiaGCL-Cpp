#pragma once
#include <QWidget>
#include "core/EmisorRepository.h"
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"

class QTableWidget;
class QPushButton;

namespace App {

class EmisoresWidget : public QWidget {
    Q_OBJECT
public:
    explicit EmisoresWidget(Calculadora::EmisorRepository&    emisorRepo,
                            Calculadora::ConcesionRepository& concesionRepo,
                            Calculadora::ProductoRepository&  productoRepo,
                            QWidget* parent = nullptr);
    void refresh();

signals:
    void navegarAConcesion(int64_t concesionId);

private slots:
    void onNuevoClicked();
    void onEditarClicked();
    void onEliminarClicked();
    void onVerPerfilClicked(int64_t emisorId);

private:
    void setupUi();
    void setupConnections();

    Calculadora::EmisorRepository&    m_repo;
    Calculadora::ConcesionRepository& m_concesionRepo;
    Calculadora::ProductoRepository&  m_productoRepo;
    QTableWidget*  m_table        = nullptr;
    QPushButton*   m_btnNuevo     = nullptr;
    QPushButton*   m_btnEditar    = nullptr;
    QPushButton*   m_btnEliminar  = nullptr;
};

} // namespace App
