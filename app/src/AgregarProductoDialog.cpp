#include "app/AgregarProductoDialog.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QButtonGroup>
#include <QFrame>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLocale>

namespace App {

AgregarProductoDialog::AgregarProductoDialog(int64_t concesionId,
                                             const QString& concesionLabel,
                                             Calculadora::PriceCalculator& calculator,
                                             double comisionPct,
                                             bool   facturacion,
                                             QWidget* parent)
    : QDialog(parent)
    , m_concesionId(concesionId)
    , m_comisionPct(comisionPct)
    , m_calculator(calculator)
{
    setupUi(concesionLabel);
    setupConnections();
    setWindowTitle("Agregar Producto a Concesion");

    // Pre-seleccionar CFDI segun la propiedad del distribuidor
    if (facturacion) {
        m_radioConCFDI->setChecked(true);
    } else {
        m_radioSinCFDI->setChecked(true);
    }
}

AgregarProductoDialog::AgregarProductoDialog(const Calculadora::ProductoRecord& existing,
                                             const QString& concesionLabel,
                                             Calculadora::PriceCalculator& calculator,
                                             double comisionPct,
                                             bool   facturacion,
                                             QWidget* parent)
    : QDialog(parent)
    , m_concesionId(existing.concesionId.value_or(0))
    , m_productoId(existing.id)
    , m_comisionPct(comisionPct)
    , m_calculator(calculator)
{
    setupUi(concesionLabel);
    setupConnections();
    setWindowTitle("Editar Producto");
    m_btnAgregar->setText("Guardar cambios");

    // Pre-seleccionar CFDI
    if (existing.tieneCFDI)
        m_radioConCFDI->setChecked(true);
    else
        m_radioSinCFDI->setChecked(true);

    // Pre-seleccionar tipo de producto
    if (existing.tipoProducto == Calculadora::TipoProducto::Libro) {
        m_radioLibro->setChecked(true);
        m_isbnLabel->setVisible(true);
        m_txtIsbn->setVisible(true);
        m_txtIsbn->setText(existing.isbn);
    } else {
        m_radioPapeleria->setChecked(true);
    }

    // Pre-rellenar datos del producto
    m_txtNombre->setText(existing.nombreProducto);
    m_spinCantidad->setValue(existing.cantidadRecibida);

    // El campo `costo` en DB siempre almacena el precioNeto del proveedor (pre-IVA),
    // tanto para Con CFDI como Sin CFDI (ver PriceCalculator: r.costo = precioNetoProveedor).
    m_inputSpin->setValue(existing.costo);

    // Nota: el usuario debe presionar "Calcular" antes de poder guardar
    // (m_btnAgregar permanece deshabilitado hasta que se calcule)
}

void AgregarProductoDialog::setupUi(const QString& concesionLabel) {
    setMinimumWidth(460);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(10);

    // Encabezado — concesion a la que se vincula
    auto* header = new QLabel(QString("Concesion: <b>%1</b>").arg(concesionLabel));
    header->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(header);

    auto addSep = [&]() {
        auto* s = new QFrame(); s->setFrameShape(QFrame::HLine);
        mainLayout->addWidget(s);
    };
    addSep();

    // ---- CFDI ----
    auto* cfdiGroup  = new QGroupBox("Comprobante fiscal del proveedor (CFDI)");
    auto* cfdiLayout = new QVBoxLayout(cfdiGroup);
    m_radioConCFDI = new QRadioButton("Con CFDI — IVA Acreditable (LIVA Art. 4 y 5)");
    m_radioSinCFDI = new QRadioButton("Sin CFDI — IVA se absorbe como costo");
    m_radioConCFDI->setChecked(true);
    auto* cfdiBtnGrp = new QButtonGroup(this);
    cfdiBtnGrp->addButton(m_radioConCFDI, 0);
    cfdiBtnGrp->addButton(m_radioSinCFDI, 1);
    cfdiLayout->addWidget(m_radioConCFDI);
    cfdiLayout->addWidget(m_radioSinCFDI);
    mainLayout->addWidget(cfdiGroup);

    // ---- Cantidad recibida ----
    auto* cantGroup  = new QGroupBox("Cantidad recibida");
    auto* cantLayout = new QHBoxLayout(cantGroup);
    m_spinCantidad = new QSpinBox();
    m_spinCantidad->setMinimum(1);
    m_spinCantidad->setMaximum(9999);
    m_spinCantidad->setValue(1);
    m_spinCantidad->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cantLayout->addWidget(new QLabel("Piezas:"));
    cantLayout->addWidget(m_spinCantidad);
    mainLayout->addWidget(cantGroup);

    // ---- Precio neto ----
    auto* inputGroup  = new QGroupBox("Precio neto del proveedor (sin IVA)");
    auto* inputLayout = new QHBoxLayout(inputGroup);
    m_inputSpin = new QDoubleSpinBox();
    m_inputSpin->setDecimals(2);
    m_inputSpin->setMinimum(0.01);
    m_inputSpin->setMaximum(999999.99);
    m_inputSpin->setPrefix("$ ");
    m_inputSpin->setValue(0.0);
    m_inputSpin->setGroupSeparatorShown(true);
    m_inputSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_calcBtn = new QPushButton("Calcular");
    inputLayout->addWidget(m_inputSpin);
    inputLayout->addWidget(m_calcBtn);
    mainLayout->addWidget(inputGroup);

    // ---- Resultados ----
    m_resultsGroup = new QGroupBox("Desglose del precio");
    m_resultsGroup->setVisible(false);
    auto* resLayout = new QVBoxLayout(m_resultsGroup);
    resLayout->setContentsMargins(10, 8, 10, 10);
    resLayout->setSpacing(4);

    auto* precioRow = new QHBoxLayout();
    auto* precioLbl = new QLabel("Precio final al cliente:");
    m_lbPrecioFinal = new QLabel("-");
    m_lbPrecioFinal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont bf = m_lbPrecioFinal->font(); bf.setBold(true); bf.setPointSize(bf.pointSize() + 1);
    m_lbPrecioFinal->setFont(bf); precioLbl->setFont(bf);
    precioRow->addWidget(precioLbl); precioRow->addStretch(); precioRow->addWidget(m_lbPrecioFinal);
    resLayout->addLayout(precioRow);

    auto* sep = new QFrame(); sep->setFrameShape(QFrame::HLine); resLayout->addWidget(sep);

    // Callout Sin CFDI (estilo en tlacuia.qss via #desgloseFrame)
    m_desgloseFrame = new QFrame();
    m_desgloseFrame->setObjectName("desgloseFrame");
    auto* desgloseInner = new QVBoxLayout(m_desgloseFrame);
    desgloseInner->setContentsMargins(10, 6, 10, 6);
    m_lbDesglose = new QLabel();
    m_lbDesglose->setWordWrap(false);
    m_lbDesglose->setTextFormat(Qt::RichText);
    desgloseInner->addWidget(m_lbDesglose);
    m_desgloseFrame->setVisible(false);
    resLayout->addWidget(m_desgloseFrame);

    // Callout visual — tipo de IVA aplicado
    m_lbIvaCallout = new QLabel();
    m_lbIvaCallout->setWordWrap(true);
    m_lbIvaCallout->setTextFormat(Qt::RichText);
    m_lbIvaCallout->setVisible(false);
    resLayout->addWidget(m_lbIvaCallout);

    auto* compForm = new QFormLayout();
    compForm->setLabelAlignment(Qt::AlignLeft);
    auto mkLbl = [] { auto* l = new QLabel("-"); l->setAlignment(Qt::AlignRight); return l; };
    m_lbCosto          = mkLbl();
    m_lbComision       = mkLbl();
    m_lbIvaTrasladado  = mkLbl();
    m_lbIvaAcreditable = mkLbl();
    m_lbIvaNetoPagar   = mkLbl();
    m_lbIvaNetoPagar->setFont(bf);
    compForm->addRow("Costo del proveedor:",          m_lbCosto);
    compForm->addRow(QString("Comision acordada (%1%):").arg(m_comisionPct, 0, 'f', 1),
                     m_lbComision);
    compForm->addRow("IVA Trasladado (16%):",         m_lbIvaTrasladado);
    compForm->addRow("IVA Acreditable:",              m_lbIvaAcreditable);
    compForm->addRow("IVA Neto a enterar a SAT:",     m_lbIvaNetoPagar);
    resLayout->addLayout(compForm);

    mainLayout->addWidget(m_resultsGroup);

    addSep();

    // ---- Datos del producto ----
    auto* prodGroup  = new QGroupBox("Datos del producto");
    auto* prodLayout = new QVBoxLayout(prodGroup);

    auto* tipoRow = new QHBoxLayout();
    m_radioPapeleria = new QRadioButton("Articulo de papeleria");
    m_radioLibro     = new QRadioButton("Libro");
    m_radioPapeleria->setChecked(true);
    auto* tipoBtnGrp = new QButtonGroup(this);
    tipoBtnGrp->addButton(m_radioPapeleria, 0);
    tipoBtnGrp->addButton(m_radioLibro,     1);
    tipoRow->addWidget(m_radioPapeleria);
    tipoRow->addWidget(m_radioLibro);
    tipoRow->addStretch();
    prodLayout->addLayout(tipoRow);

    auto* prodForm = new QFormLayout();
    prodForm->setLabelAlignment(Qt::AlignRight);
    m_txtNombre = new QLineEdit(); m_txtNombre->setPlaceholderText("Nombre del producto *");
    m_isbnLabel = new QLabel("ISBN *:");
    m_txtIsbn   = new QLineEdit(); m_txtIsbn->setPlaceholderText("Ej. 978-607-XXX-XXX-X");
    m_txtIsbn->setMaxLength(17);
    prodForm->addRow("Nombre *:", m_txtNombre);
    prodForm->addRow(m_isbnLabel, m_txtIsbn);
    m_isbnLabel->setVisible(false); m_txtIsbn->setVisible(false);
    prodLayout->addLayout(prodForm);

    mainLayout->addWidget(prodGroup);

    // ---- Botones ----
    auto* btnRow    = new QHBoxLayout();
    m_btnAgregar    = new QPushButton("Agregar a Concesion");
    auto* btnCancel = new QPushButton("Cancelar");
    m_btnAgregar->setObjectName("primaryButton");
    m_btnAgregar->setDefault(true);
    m_btnAgregar->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(m_btnAgregar);
    mainLayout->addLayout(btnRow);

    connect(tipoBtnGrp, &QButtonGroup::idToggled,
            this, [this](int, bool) { onTipoProductoChanged(); });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

void AgregarProductoDialog::setupConnections() {
    connect(m_calcBtn,    &QPushButton::clicked, this, &AgregarProductoDialog::onCalcularClicked);
    connect(m_btnAgregar, &QPushButton::clicked, this, &AgregarProductoDialog::onAgregarClicked);
}

void AgregarProductoDialog::onCalcularClicked() {
    double precioNeto = m_inputSpin->value();
    Calculadora::CalculationResult r;

    auto tipo = m_radioLibro->isChecked() ? Calculadora::TipoProducto::Libro
                                          : Calculadora::TipoProducto::Papeleria;
    if (m_radioConCFDI->isChecked())
        r = m_calculator.calcularConcesionConCFDI(precioNeto, m_comisionPct, tipo);
    else
        r = m_calculator.calcularConcesionSinCFDI(precioNeto, m_comisionPct, tipo);

    if (!r.isValid) {
        QMessageBox::warning(this, "Entrada invalida",
                             QString::fromStdString(r.errorMessage));
        return;
    }

    m_result = r;
    displayResult(r);
    m_resultsGroup->setVisible(true);
    m_btnAgregar->setEnabled(true);
    adjustSize();
}

void AgregarProductoDialog::displayResult(const Calculadora::CalculationResult& r) {
    QLocale loc;
    auto fmt = [&](double v) { return loc.toCurrencyString(v); };

    const bool esLibro = (r.tipoProducto == Calculadora::TipoProducto::Libro);

    m_lbPrecioFinal->setText(fmt(r.precioFinal));
    m_lbCosto->setText(fmt(r.costo));
    m_lbComision->setText(fmt(r.comision));

    if (esLibro) {
        m_lbIvaTrasladado->setText("$0.00  (Tasa 0% — Art. 2-A LIVA)");
        if (r.tieneCFDI)
            m_lbIvaAcreditable->setText(fmt(r.ivaAcreditable) + "  (saldo a favor)");
        else
            m_lbIvaAcreditable->setText("$0.00  (sin CFDI — no acreditable)");
        m_lbIvaNetoPagar->setText(r.tieneCFDI
            ? fmt(r.ivaNetoPagar) + "  (saldo a favor)"
            : "$0.00");
    } else {
        m_lbIvaTrasladado->setText(fmt(r.ivaTrasladado));
        m_lbIvaAcreditable->setText(r.tieneCFDI ? fmt(r.ivaAcreditable)
                                                 : "$0.00  (sin CFDI)");
        m_lbIvaNetoPagar->setText(fmt(r.ivaNetoPagar));
    }

    // Callout visual — tipo de IVA
    if (esLibro) {
        const QString detalle = r.tieneCFDI
            ? "IVA pagado al proveedor (16%) genera <b>saldo a favor</b> de la libreria."
            : "Sin CFDI: IVA pagado al proveedor se absorbe como costo (no recuperable).";
        m_lbIvaCallout->setText(
            QString("<div style='background:#E8F5E9;border-left:3px solid #4CAF50;"
                    "padding:6px 8px;'>"
                    "<b style='color:#1B5E20;'>&#10003; Tasa 0% — Libro (LIVA Art. 2-A fr. IV)</b><br>"
                    "<span style='color:#388E3C;font-size:11px;'>"
                    "No se cobra IVA al cliente. %1</span></div>").arg(detalle));
        m_lbIvaCallout->setVisible(true);
    } else {
        m_lbIvaCallout->setText(
            QString("<div style='background:#FFF8E1;border-left:3px solid #FF9800;"
                    "padding:6px 8px;'>"
                    "<b style='color:#E65100;'>IVA 16% incluido en precio al cliente: %1</b>"
                    "</div>").arg(fmt(r.ivaTrasladado)));
        m_lbIvaCallout->setVisible(true);
    }

    // Callout de pasos — solo para Papeleria Sin CFDI
    const bool sinCFDI = !r.tieneCFDI;
    if (sinCFDI && !esLibro) {
        const double costoEfectivo = r.costo + r.ivaAbsorbido;
        m_lbDesglose->setText(
            QString("<b>IVA no acreditable — se absorbe como costo:</b><br>"
                    "<tt>&nbsp;&nbsp;Paso 1: %1 &times; 1.16 = %2&nbsp;&nbsp;"
                    "(IVA absorbido: %3)</tt><br>"
                    "<tt>&nbsp;&nbsp;Paso 2: %2 / 0.54 = %4&nbsp;&nbsp;"
                    "(Comision: %5)</tt>")
                .arg(fmt(r.costo), fmt(costoEfectivo), fmt(r.ivaAbsorbido),
                     fmt(r.precioFinal), fmt(r.comision)));
    } else if (sinCFDI && esLibro && r.ivaAbsorbido > 0.0) {
        m_lbDesglose->setText(
            QString("<b>Libro sin CFDI — IVA pagado al proveedor no recuperable:</b><br>"
                    "<tt>&nbsp;&nbsp;IVA absorbido = %1 &times; 0.16 = %2</tt>")
                .arg(fmt(r.costo), fmt(r.ivaAbsorbido)));
    }
    m_desgloseFrame->setVisible(sinCFDI);
}

void AgregarProductoDialog::onTipoProductoChanged() {
    bool esLibro = m_radioLibro->isChecked();
    m_isbnLabel->setVisible(esLibro);
    m_txtIsbn->setVisible(esLibro);
    if (!esLibro) m_txtIsbn->clear();
    // Resetear resultado: la formula cambia segun tipo (tasa 0% vs 16%)
    if (m_result.has_value()) {
        m_result.reset();
        m_resultsGroup->setVisible(false);
        m_btnAgregar->setEnabled(false);
    }
    adjustSize();
}

void AgregarProductoDialog::onAgregarClicked() {
    if (!m_result.has_value()) return;
    if (m_txtNombre->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido", "El nombre del producto es obligatorio.");
        m_txtNombre->setFocus();
        return;
    }
    if (m_radioLibro->isChecked() && m_txtIsbn->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido", "El ISBN es obligatorio para libros.");
        m_txtIsbn->setFocus();
        return;
    }
    accept();
}

int64_t  AgregarProductoDialog::productoId()    const { return m_productoId; }
QString AgregarProductoDialog::nombreProducto() const { return m_txtNombre->text().trimmed(); }
Calculadora::TipoProducto AgregarProductoDialog::tipoProducto() const {
    return m_radioLibro->isChecked() ? Calculadora::TipoProducto::Libro
                                     : Calculadora::TipoProducto::Papeleria;
}
QString  AgregarProductoDialog::isbn()           const { return m_txtIsbn->text().trimmed(); }
int64_t  AgregarProductoDialog::concesionId()    const { return m_concesionId; }
int      AgregarProductoDialog::cantidad()       const { return m_spinCantidad->value(); }
Calculadora::CalculationResult AgregarProductoDialog::calculationResult() const {
    return m_result.value_or(Calculadora::CalculationResult{});
}

} // namespace App
