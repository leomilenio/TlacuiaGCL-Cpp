#pragma once
#include <QWidget>
#include <optional>
#include "core/CalculationScenario.h"

class QDoubleSpinBox;
class QRadioButton;
class QPushButton;
class QGroupBox;
class QFrame;
class QLabel;
class QButtonGroup;

namespace Calculadora {
class PriceCalculator;
class ProductoRepository;
class ConcesionRepository;
}

namespace App {

class CalculatorWidget : public QWidget {
    Q_OBJECT
public:
    explicit CalculatorWidget(Calculadora::PriceCalculator&    calculator,
                               Calculadora::ProductoRepository&  repo,
                               Calculadora::ConcesionRepository& concesionRepo,
                               QWidget* parent = nullptr);

    // Oculta el boton Guardar — para modo "solo calculo" desde Herramientas
    void disableSaving();

signals:
    void calculationSaved();

private slots:
    void onEscenarioChanged();
    void onCalculateClicked();
    void onSaveClicked();
    void onClearClicked();

private:
    void setupUi();
    void setupConnections();
    void displayResult(const Calculadora::CalculationResult& result);
    void clearResults();

    // Input
    QDoubleSpinBox* m_inputSpin       = nullptr;
    QLabel*         m_inputLabel      = nullptr;
    QGroupBox*      m_escenarioGroup  = nullptr;
    QRadioButton*   m_radioPropio     = nullptr;
    QRadioButton*   m_radioConcesion  = nullptr;
    QGroupBox*      m_cfdiGroup       = nullptr;
    QRadioButton*   m_radioConCFDI   = nullptr;
    QRadioButton*   m_radioSinCFDI   = nullptr;
    QButtonGroup*   m_escenarioBtnGrp = nullptr;

    QLabel*      m_ivaInfoLabel = nullptr;   // Nota fiscal dinamica

    // Acciones
    QPushButton* m_calcBtn  = nullptr;
    QPushButton* m_saveBtn  = nullptr;
    QPushButton* m_clearBtn = nullptr;

    // Resultados
    QGroupBox* m_resultsGroup         = nullptr;
    QLabel*    m_lbPrecioFinal        = nullptr;
    QFrame*    m_desgloseFrame        = nullptr;  // Callout — solo visible en Concesion Sin CFDI
    QLabel*    m_lbDesgloseSinCFDI   = nullptr;
    QLabel*    m_labelCosto           = nullptr;  // Label de fila "Costo" (texto dinamico)
    QLabel*    m_labelComision        = nullptr;  // Label de fila "Comision" (texto dinamico)
    QLabel*    m_lbCosto              = nullptr;
    QLabel*    m_lbComision           = nullptr;
    QLabel*    m_lbIvaTrasladado      = nullptr;
    QLabel*    m_lbIvaAcreditable     = nullptr;
    QLabel*    m_lbIvaNetoPagar       = nullptr;

    Calculadora::PriceCalculator&    m_calculator;
    Calculadora::ProductoRepository& m_repo;
    Calculadora::ConcesionRepository& m_concesionRepo;
    std::optional<Calculadora::CalculationResult> m_lastResult;
};

} // namespace App
