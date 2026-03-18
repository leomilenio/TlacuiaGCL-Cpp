#pragma once
#include <QDialog>
#include <QString>
#include <optional>
#include <cstdint>
#include "core/CalculationScenario.h"
#include "core/ConcesionRepository.h"

class QLineEdit;
class QPushButton;
class QRadioButton;
class QFormLayout;
class QLabel;
class QComboBox;

namespace App {

class SaveDialog : public QDialog {
    Q_OBJECT
public:
    explicit SaveDialog(double precioFinal,
                        Calculadora::ConcesionRepository& concesionRepo,
                        QWidget* parent = nullptr);

    [[nodiscard]] QString                       nombreProducto() const;
    [[nodiscard]] Calculadora::TipoProducto     tipoProducto()   const;
    [[nodiscard]] QString                       isbn()            const;
    [[nodiscard]] QString                       nombreProveedor() const;
    [[nodiscard]] QString                       nombreVendedor()  const;
    [[nodiscard]] std::optional<int64_t>        concesionId()     const;

private slots:
    void onTipoProductoChanged();
    void onAcceptClicked();

private:
    void setupUi(double precioFinal);

    // Tipo de producto
    QRadioButton* m_radioPapeleria  = nullptr;
    QRadioButton* m_radioLibro      = nullptr;

    // Campos del producto
    QLineEdit*    m_productoEdit    = nullptr;  // Requerido siempre
    QLabel*       m_isbnLabel       = nullptr;
    QLineEdit*    m_isbnEdit        = nullptr;  // Requerido si Libro
    QLabel*       m_isbnReqLabel    = nullptr;  // Indicador "*"

    // Datos opcionales
    QLineEdit*    m_proveedorEdit   = nullptr;
    QLineEdit*    m_vendedorEdit    = nullptr;

    // Vinculacion opcional a concesion
    QComboBox*    m_cmbConcesion    = nullptr;

    QPushButton*  m_acceptBtn       = nullptr;
};

} // namespace App
