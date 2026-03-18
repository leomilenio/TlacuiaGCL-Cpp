#include "app/CalculatorWidget.h"
#include "app/SaveDialog.h"
#include "core/PriceCalculator.h"
#include "core/ProductoRepository.h"
#include "core/ConcesionRepository.h"
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QLocale>

namespace App {

CalculatorWidget::CalculatorWidget(Calculadora::PriceCalculator&    calculator,
                                    Calculadora::ProductoRepository&  repo,
                                    Calculadora::ConcesionRepository& concesionRepo,
                                    QWidget* parent)
    : QWidget(parent)
    , m_calculator(calculator)
    , m_repo(repo)
    , m_concesionRepo(concesionRepo)
{
    setupUi();
    setupConnections();
    onEscenarioChanged(); // Estado inicial correcto
}

void CalculatorWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 12);
    mainLayout->setSpacing(10);

    // --- Escenario ---
    m_escenarioGroup = new QGroupBox("Tipo de producto");
    auto* escLayout  = new QHBoxLayout(m_escenarioGroup);
    m_radioPropio    = new QRadioButton("Producto propio");
    m_radioConcesion = new QRadioButton("Concesion");
    m_radioPropio->setChecked(true);
    m_escenarioBtnGrp = new QButtonGroup(this);
    m_escenarioBtnGrp->addButton(m_radioPropio,    0);
    m_escenarioBtnGrp->addButton(m_radioConcesion, 1);
    escLayout->addWidget(m_radioPropio);
    escLayout->addWidget(m_radioConcesion);
    escLayout->addStretch();

    // --- CFDI: aplica a AMBOS escenarios (Producto Propio y Concesion) ---
    // Fundamento: LIVA Art. 4 y 5 — el IVA pagado al proveedor solo es Acreditable
    // cuando se cuenta con un CFDI valido (CFF Art. 29-A) y el gasto es deducible para ISR.
    m_cfdiGroup   = new QGroupBox("Comprobante fiscal del proveedor (CFDI)");
    auto* cfdiLay = new QVBoxLayout(m_cfdiGroup);
    m_radioConCFDI = new QRadioButton(
        "Con CFDI — IVA pagado al proveedor es Acreditable  (LIVA Art. 4 y 5)");
    m_radioSinCFDI = new QRadioButton(
        "Sin CFDI — IVA no acreditable, se absorbe como costo");
    m_radioConCFDI->setChecked(true);
    m_radioConCFDI->setToolTip(
        "El proveedor emite CFDI valido (CFF Art. 29-A).\n"
        "El IVA pagado reduce su obligacion fiscal:\n"
        "  IVA a enterar = IVA Trasladado (ventas) - IVA Acreditable (compras)\n"
        "Requisitos: gasto deducible ISR; pagos >$2,000 via sistema financiero.");
    m_radioSinCFDI->setToolTip(
        "No existe CFDI del proveedor.\n"
        "El IVA pagado no puede acreditarse y se absorbe en el costo del producto,\n"
        "elevando el precio final necesario para mantener los mismos margenes.");

    // Nota legal resumida (actualizada segun escenario)
    m_ivaInfoLabel = new QLabel();
    m_ivaInfoLabel->setWordWrap(true);
    {
        QFont f = m_ivaInfoLabel->font();
        f.setPointSize(qMax(f.pointSize() - 1, 9));
        m_ivaInfoLabel->setFont(f);
    }

    cfdiLay->addWidget(m_radioConCFDI);
    cfdiLay->addWidget(m_radioSinCFDI);
    cfdiLay->addWidget(m_ivaInfoLabel);

    // --- Input de precio ---
    auto* inputGroup  = new QGroupBox("Precio de entrada");
    auto* inputLayout = new QFormLayout(inputGroup);
    m_inputLabel = new QLabel("Precio final de venta ($):");
    m_inputSpin  = new QDoubleSpinBox();
    m_inputSpin->setDecimals(2);
    m_inputSpin->setMinimum(0.01);
    m_inputSpin->setMaximum(999999.99);
    m_inputSpin->setPrefix("$ ");
    m_inputSpin->setValue(0.0);
    m_inputSpin->setGroupSeparatorShown(true);
    m_inputSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    inputLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    inputLayout->addRow(m_inputLabel, m_inputSpin);

    // --- Botones ---
    auto* btnLayout = new QHBoxLayout();
    m_calcBtn  = new QPushButton("Calcular");
    m_saveBtn  = new QPushButton("Guardar");
    m_clearBtn = new QPushButton("Limpiar");
    m_calcBtn->setObjectName("primaryButton");
    m_saveBtn->setEnabled(false);
    btnLayout->addWidget(m_calcBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_clearBtn);

    // --- Resultados ---
    m_resultsGroup = new QGroupBox("Desglose del precio");
    auto* resLayout = new QVBoxLayout(m_resultsGroup);
    resLayout->setContentsMargins(12, 8, 12, 12);
    resLayout->setSpacing(6);

    // Precio final — destacado
    auto* precioRow = new QHBoxLayout();
    auto* precioRowLabel = new QLabel("Precio final de venta:");
    m_lbPrecioFinal = new QLabel("-");
    m_lbPrecioFinal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont boldFont = m_lbPrecioFinal->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 1);
    m_lbPrecioFinal->setFont(boldFont);
    precioRowLabel->setFont(boldFont);
    precioRow->addWidget(precioRowLabel);
    precioRow->addStretch();
    precioRow->addWidget(m_lbPrecioFinal);
    resLayout->addLayout(precioRow);

    // Separador
    auto* resSep1 = new QFrame(); resSep1->setFrameShape(QFrame::HLine);
    resLayout->addWidget(resSep1);

    // Callout "Desglose de pasos" — solo visible en Concesion Sin CFDI
    // (estilo en tlacuia.qss via #desgloseFrame)
    m_desgloseFrame = new QFrame();
    m_desgloseFrame->setObjectName("desgloseFrame");
    auto* desgloseInner = new QVBoxLayout(m_desgloseFrame);
    desgloseInner->setContentsMargins(10, 8, 10, 8);
    desgloseInner->setSpacing(2);

    m_lbDesgloseSinCFDI = new QLabel();
    m_lbDesgloseSinCFDI->setWordWrap(false);
    m_lbDesgloseSinCFDI->setTextFormat(Qt::RichText);
    desgloseInner->addWidget(m_lbDesgloseSinCFDI);

    m_desgloseFrame->setVisible(false);
    resLayout->addWidget(m_desgloseFrame);

    // Componentes del precio
    auto* compForm = new QFormLayout();
    compForm->setLabelAlignment(Qt::AlignLeft);
    auto makeLabel = [] { auto* l = new QLabel("-"); l->setAlignment(Qt::AlignRight); return l; };
    m_lbCosto     = makeLabel();
    m_lbComision  = makeLabel();
    m_labelCosto    = new QLabel("Costo (54%):");
    m_labelComision = new QLabel("Comision (30%):");
    compForm->addRow(m_labelCosto,    m_lbCosto);
    compForm->addRow(m_labelComision, m_lbComision);
    resLayout->addLayout(compForm);

    // Separador IVA
    auto* resSep2 = new QFrame(); resSep2->setFrameShape(QFrame::HLine);
    resLayout->addWidget(resSep2);

    // Seccion IVA
    auto* ivaForm = new QFormLayout();
    ivaForm->setLabelAlignment(Qt::AlignLeft);
    m_lbIvaTrasladado  = makeLabel();
    m_lbIvaAcreditable = makeLabel();
    m_lbIvaNetoPagar   = makeLabel();
    m_lbIvaNetoPagar->setFont(boldFont);

    ivaForm->addRow("IVA Trasladado — cobrado al cliente (16%):",    m_lbIvaTrasladado);
    ivaForm->addRow("IVA Acreditable — pagado con CFDI al proveedor:", m_lbIvaAcreditable);
    ivaForm->addRow("IVA neto a enterar a SAT:",                     m_lbIvaNetoPagar);
    resLayout->addLayout(ivaForm);

    mainLayout->addWidget(m_escenarioGroup);
    mainLayout->addWidget(m_cfdiGroup);
    mainLayout->addWidget(inputGroup);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(m_resultsGroup);
    mainLayout->addStretch();
}

void CalculatorWidget::setupConnections() {
    connect(m_escenarioBtnGrp, &QButtonGroup::idToggled,
            this, [this](int, bool) { onEscenarioChanged(); });
    // Limpiar resultado si cambia el tipo de CFDI (el calculo previo ya no aplica)
    connect(m_radioConCFDI, &QRadioButton::toggled,
            this, [this]() { onEscenarioChanged(); });
    connect(m_calcBtn,  &QPushButton::clicked, this, &CalculatorWidget::onCalculateClicked);
    connect(m_saveBtn,  &QPushButton::clicked, this, &CalculatorWidget::onSaveClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &CalculatorWidget::onClearClicked);
}

void CalculatorWidget::onEscenarioChanged() {
    bool isConcesion = m_radioConcesion->isChecked();

    if (isConcesion) {
        m_inputLabel->setText("Precio neto del proveedor sin IVA ($):");
        // En concesion sin CFDI el IVA se suma al costo efectivo antes de calcular.
        // En concesion con CFDI el precio neto ES el 54% y el IVA es acreditable.
        m_ivaInfoLabel->setText(
            "Con CFDI: precioFinal = precioNeto / 0.54  |  "
            "IVA Acreditable = precioNeto x 16%\n"
            "Sin CFDI: costoEfectivo = precioNeto x 1.16  |  "
            "precioFinal = costoEfectivo / 0.54  |  IVA Acreditable = $0");
    } else {
        m_inputLabel->setText("Precio final de venta ($):");
        // En producto propio el usuario fija el precio; el IVA Acreditable depende
        // de si tuvo CFDI de compra. Se estima como: costo_implicito (54%) x 16%.
        m_ivaInfoLabel->setText(
            "Con CFDI de compra: IVA Acreditable = costo implicito (54%) x 16%\n"
            "Sin CFDI de compra: IVA Acreditable = $0  (todo el IVA se entera a SAT)\n"
            "Req. Art. 5 LIVA: CFDI valido, gasto deducible ISR, pago >$2,000 via banco.");
    }
    clearResults();
}

void CalculatorWidget::onCalculateClicked() {
    double inputVal = m_inputSpin->value();
    Calculadora::CalculationResult result;

    if (m_radioPropio->isChecked()) {
        result = m_calculator.calcularProductoPropio(inputVal, m_radioConCFDI->isChecked());
    } else if (m_radioConCFDI->isChecked()) {
        result = m_calculator.calcularConcesionConCFDI(inputVal);
    } else {
        result = m_calculator.calcularConcesionSinCFDI(inputVal);
    }

    if (!result.isValid) {
        QMessageBox::warning(this, "Entrada invalida",
                             QString::fromStdString(result.errorMessage));
        return;
    }

    displayResult(result);
    m_lastResult = result;
    m_saveBtn->setEnabled(true);
}

void CalculatorWidget::onSaveClicked() {
    if (!m_lastResult.has_value()) return;

    SaveDialog dlg(m_lastResult->precioFinal, m_concesionRepo, this);
    if (dlg.exec() != QDialog::Accepted) return;

    Calculadora::ProductoRecord record;
    record.nombreProducto  = dlg.nombreProducto();
    record.tipoProducto    = dlg.tipoProducto();
    record.isbn            = dlg.isbn();
    record.precioFinal     = m_lastResult->precioFinal;
    record.costo           = m_lastResult->costo;
    record.comision        = m_lastResult->comision;
    record.ivaTrasladado   = m_lastResult->ivaTrasladado;
    record.ivaAcreditable  = m_lastResult->ivaAcreditable;
    record.ivaNetoPagar    = m_lastResult->ivaNetoPagar;
    record.escenario       = m_lastResult->escenario;
    record.tieneCFDI       = m_lastResult->tieneCFDI;
    record.nombreProveedor = dlg.nombreProveedor();
    record.nombreVendedor  = dlg.nombreVendedor();
    record.concesionId     = dlg.concesionId();

    int64_t newId = m_repo.save(record);
    if (newId < 0) {
        QMessageBox::critical(this, "Error", "No se pudo guardar el registro.");
        return;
    }

    m_saveBtn->setEnabled(false);
    emit calculationSaved();
    QMessageBox::information(this, "Guardado", "Registro guardado correctamente.");
}

void CalculatorWidget::disableSaving() {
    m_saveBtn->setVisible(false);
}

void CalculatorWidget::onClearClicked() {
    m_inputSpin->setValue(0.0);
    m_lastResult.reset();
    m_saveBtn->setEnabled(false);
    clearResults();
}

void CalculatorWidget::displayResult(const Calculadora::CalculationResult& r) {
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v); };

    // Labels dinamicos segun escenario
    if (r.escenario == Calculadora::Escenario::Concesion) {
        m_labelCosto->setText("Costo del proveedor:");
        m_labelComision->setText("Comision acordada (30%):");
    } else {
        m_labelCosto->setText("Costo (54%):");
        m_labelComision->setText("Comision (30%):");
    }

    m_lbPrecioFinal->setText(fmt(r.precioFinal));
    m_lbCosto->setText(fmt(r.costo));
    m_lbComision->setText(fmt(r.comision));
    m_lbIvaTrasladado->setText(fmt(r.ivaTrasladado));
    m_lbIvaAcreditable->setText(r.tieneCFDI ? fmt(r.ivaAcreditable) : "$0.00  (sin CFDI — no acreditable)");
    m_lbIvaNetoPagar->setText(fmt(r.ivaNetoPagar));

    // Callout de pasos — solo para Concesion Sin CFDI
    const bool esConcesionSinCFDI =
        r.escenario == Calculadora::Escenario::Concesion && !r.tieneCFDI;
    if (esConcesionSinCFDI) {
        // r.costo == precioNeto (modelo markup); r.ivaAbsorbido = precioNeto * 0.16
        const double costoReal = r.costo + r.ivaAbsorbido;
        m_lbDesgloseSinCFDI->setText(
            QString("<b>Como el IVA no es acreditable, la libreria lo absorbe:</b><br>"
                    "<tt>&nbsp;&nbsp;Paso 1 &mdash; IVA absorbido:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                        "%1 &times; 0.16 = %2&nbsp;&nbsp;(costo real: %3)</tt><br>"
                    "<tt>&nbsp;&nbsp;Paso 2 &mdash; Precio de venta:&nbsp;&nbsp;&nbsp;&nbsp;"
                        "%1 &times; 1.30 = %4&nbsp;&nbsp;(con comision)</tt><br>"
                    "<tt>&nbsp;&nbsp;Paso 3 &mdash; Precio final al cliente: "
                        "%4 &times; 1.16 = %5</tt>")
                .arg(fmt(r.costo), fmt(r.ivaAbsorbido), fmt(costoReal),
                     fmt(r.costo + r.comision), fmt(r.precioFinal)));
    }
    m_desgloseFrame->setVisible(esConcesionSinCFDI);
}

void CalculatorWidget::clearResults() {
    const QString dash = "-";
    m_labelCosto->setText("Costo (54%):");
    m_labelComision->setText("Comision (30%):");
    m_lbPrecioFinal->setText(dash);
    m_desgloseFrame->setVisible(false);
    m_lbCosto->setText(dash);
    m_lbComision->setText(dash);
    m_lbIvaTrasladado->setText(dash);
    m_lbIvaAcreditable->setText(dash);
    m_lbIvaNetoPagar->setText(dash);
}

} // namespace App
