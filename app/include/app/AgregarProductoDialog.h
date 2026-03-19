#pragma once
#include <QDialog>
#include <optional>
#include "core/PriceCalculator.h"
#include "core/CalculationScenario.h"
#include "core/ProductoRepository.h"

class QDoubleSpinBox;
class QSpinBox;
class QRadioButton;
class QPushButton;
class QGroupBox;
class QLabel;
class QLineEdit;
class QButtonGroup;
class QFrame;

namespace App {

// Dialogo de calculo integrado para agregar un producto a una concesion.
// El escenario esta bloqueado a "Concesion" ya que el contexto es una
// concesion activa. El usuario elige Con/Sin CFDI, ingresa el precio neto
// del proveedor, calcula, completa los datos del producto y guarda.
class AgregarProductoDialog : public QDialog {
    Q_OBJECT
public:
    // Modo agregar
    explicit AgregarProductoDialog(int64_t concesionId,
                                   const QString& concesionLabel,
                                   Calculadora::PriceCalculator& calculator,
                                   double comisionPct   = 30.0,
                                   bool   facturacion   = true,
                                   QWidget* parent = nullptr);

    // Modo editar — pre-rellena los datos del producto existente.
    // El usuario debe recalcular antes de poder guardar.
    explicit AgregarProductoDialog(const Calculadora::ProductoRecord& existing,
                                   const QString& concesionLabel,
                                   Calculadora::PriceCalculator& calculator,
                                   double comisionPct,
                                   bool   facturacion,
                                   QWidget* parent = nullptr);

    // Datos del producto a guardar (validos solo si exec() == Accepted)
    [[nodiscard]] QString                             nombreProducto() const;
    [[nodiscard]] Calculadora::TipoProducto           tipoProducto()   const;
    [[nodiscard]] QString                             isbn()           const;
    [[nodiscard]] int64_t                             concesionId()    const;
    [[nodiscard]] int                                 cantidad()       const;
    [[nodiscard]] Calculadora::CalculationResult      calculationResult() const;
    // Solo valido en modo edicion (segundo constructor)
    [[nodiscard]] int64_t                             productoId()     const;

private slots:
    void onCalcularClicked();
    void onTipoProductoChanged();
    void onAgregarClicked();

private:
    void setupUi(const QString& concesionLabel);
    void setupConnections();
    void displayResult(const Calculadora::CalculationResult& r);

    int64_t                      m_concesionId;
    int64_t                      m_productoId  = 0;   // 0 = modo agregar
    double                       m_comisionPct;
    Calculadora::PriceCalculator& m_calculator;
    std::optional<Calculadora::CalculationResult> m_result;

    // Inputs de calculo
    QRadioButton*   m_radioConCFDI  = nullptr;
    QRadioButton*   m_radioSinCFDI  = nullptr;
    QSpinBox*       m_spinCantidad  = nullptr;
    QDoubleSpinBox* m_inputSpin     = nullptr;
    QPushButton*    m_calcBtn       = nullptr;

    // Resultados
    QGroupBox*  m_resultsGroup      = nullptr;
    QLabel*     m_lbPrecioFinal     = nullptr;
    QLabel*     m_lbCosto           = nullptr;
    QLabel*     m_lbComision        = nullptr;
    QLabel*     m_lbIvaTrasladado   = nullptr;
    QLabel*     m_lbIvaAcreditable  = nullptr;
    QLabel*     m_lbIvaNetoPagar    = nullptr;
    QFrame*     m_desgloseFrame     = nullptr;
    QLabel*     m_lbDesglose        = nullptr;
    QLabel*     m_lbIvaCallout      = nullptr;  // Resaltado visual del tipo de IVA

    // Datos del producto
    QRadioButton* m_radioPapeleria  = nullptr;
    QRadioButton* m_radioLibro      = nullptr;
    QLineEdit*    m_txtNombre       = nullptr;
    QLabel*       m_isbnLabel       = nullptr;
    QLineEdit*    m_txtIsbn         = nullptr;

    QPushButton*  m_btnAgregar      = nullptr;
};

} // namespace App
