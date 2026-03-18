#include "app/SaveDialog.h"
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QLocale>
#include <QMessageBox>

namespace App {

SaveDialog::SaveDialog(double precioFinal,
                       Calculadora::ConcesionRepository& concesionRepo,
                       QWidget* parent)
    : QDialog(parent)
{
    setupUi(precioFinal);

    // Cargar concesiones activas en el combo
    m_cmbConcesion->addItem("(Ninguna)", QVariant(-1LL));
    const auto activas = concesionRepo.findActivas();
    for (const auto& c : activas) {
        QString emisor = c.emisorNombre.isEmpty() ? "(Sin distribuidor)" : c.emisorNombre;
        QString folio  = c.folio.isEmpty()        ? "(Sin folio)"        : c.folio;
        QString statusStr;
        if (c.status() == Calculadora::ConcesionStatus::VencePronto)
            statusStr = " [Vence pronto]";
        m_cmbConcesion->addItem(
            QString("%1 — %2%3").arg(emisor, folio, statusStr),
            QVariant::fromValue(static_cast<qlonglong>(c.id)));
    }
}

void SaveDialog::setupUi(double precioFinal) {
    setWindowTitle("Guardar calculo");
    setMinimumWidth(400);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // -- Encabezado --
    QLocale loc;
    auto* headerLabel = new QLabel(
        QString("Precio final de venta: <b>%1</b>").arg(loc.toCurrencyString(precioFinal)));
    headerLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(headerLabel);

    auto addSep = [&]() {
        auto* s = new QFrame(); s->setFrameShape(QFrame::HLine);
        mainLayout->addWidget(s);
    };
    addSep();

    // -- Tipo de producto --
    auto* tipoLabel = new QLabel("<b>Tipo de articulo</b>");
    mainLayout->addWidget(tipoLabel);

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
    mainLayout->addLayout(tipoRow);

    addSep();

    // -- Datos del producto --
    auto* productoTitle = new QLabel("<b>Producto</b>");
    mainLayout->addWidget(productoTitle);

    auto* productoForm = new QFormLayout();
    productoForm->setLabelAlignment(Qt::AlignRight);

    m_productoEdit = new QLineEdit();
    m_productoEdit->setPlaceholderText("Nombre del producto *");
    productoForm->addRow("Nombre *:", m_productoEdit);

    // Fila ISBN — visible solo para Libro
    m_isbnLabel    = new QLabel("ISBN *:");
    m_isbnEdit     = new QLineEdit();
    m_isbnEdit->setPlaceholderText("Ej. 978-607-XXX-XXX-X");
    m_isbnEdit->setMaxLength(17);
    m_isbnReqLabel = new QLabel("Requerido para libros");
    m_isbnReqLabel->setStyleSheet("color: gray; font-size: 10px;");

    productoForm->addRow(m_isbnLabel,    m_isbnEdit);
    productoForm->addRow(new QLabel(""), m_isbnReqLabel);
    mainLayout->addLayout(productoForm);

    addSep();

    // -- Datos opcionales --
    auto* opTitle = new QLabel("<b>Datos de la operacion</b> (opcionales)");
    mainLayout->addWidget(opTitle);

    auto* opForm = new QFormLayout();
    opForm->setLabelAlignment(Qt::AlignRight);
    m_proveedorEdit = new QLineEdit();
    m_proveedorEdit->setPlaceholderText("Nombre del proveedor / emisor");
    m_vendedorEdit  = new QLineEdit();
    m_vendedorEdit->setPlaceholderText("Nombre del vendedor");
    opForm->addRow("Proveedor:", m_proveedorEdit);
    opForm->addRow("Vendedor:",  m_vendedorEdit);
    mainLayout->addLayout(opForm);

    addSep();

    // -- Concesion opcional --
    auto* concTitle = new QLabel("<b>Vincular a concesion</b> (opcional)");
    mainLayout->addWidget(concTitle);

    auto* concForm = new QFormLayout();
    concForm->setLabelAlignment(Qt::AlignRight);
    m_cmbConcesion = new QComboBox();
    m_cmbConcesion->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    concForm->addRow("Concesion:", m_cmbConcesion);
    mainLayout->addLayout(concForm);

    addSep();

    // -- Botones --
    auto* btnRow    = new QHBoxLayout();
    m_acceptBtn     = new QPushButton("Guardar");
    auto* cancelBtn = new QPushButton("Cancelar");
    m_acceptBtn->setObjectName("primaryButton");
    m_acceptBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(m_acceptBtn);
    mainLayout->addLayout(btnRow);

    connect(tipoBtnGrp, &QButtonGroup::idToggled,
            this, [this](int, bool) { onTipoProductoChanged(); });
    connect(m_acceptBtn, &QPushButton::clicked, this, &SaveDialog::onAcceptClicked);
    connect(cancelBtn,   &QPushButton::clicked, this, &QDialog::reject);

    // Estado inicial: papeleria → ISBN oculto
    onTipoProductoChanged();
    m_productoEdit->setFocus();
}

void SaveDialog::onTipoProductoChanged() {
    bool esLibro = m_radioLibro->isChecked();
    m_isbnLabel->setVisible(esLibro);
    m_isbnEdit->setVisible(esLibro);
    m_isbnReqLabel->setVisible(esLibro);
    if (!esLibro) {
        m_isbnEdit->clear();
    }
    adjustSize();
}

void SaveDialog::onAcceptClicked() {
    if (m_productoEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido",
            "El nombre del producto es obligatorio.");
        m_productoEdit->setFocus();
        return;
    }
    if (m_radioLibro->isChecked() && m_isbnEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido",
            "El ISBN es obligatorio para libros.");
        m_isbnEdit->setFocus();
        return;
    }
    accept();
}

QString SaveDialog::nombreProducto() const {
    return m_productoEdit->text().trimmed();
}

Calculadora::TipoProducto SaveDialog::tipoProducto() const {
    return m_radioLibro->isChecked()
        ? Calculadora::TipoProducto::Libro
        : Calculadora::TipoProducto::Papeleria;
}

QString SaveDialog::isbn()            const { return m_isbnEdit->text().trimmed(); }
QString SaveDialog::nombreProveedor() const { return m_proveedorEdit->text().trimmed(); }
QString SaveDialog::nombreVendedor()  const { return m_vendedorEdit->text().trimmed(); }

std::optional<int64_t> SaveDialog::concesionId() const {
    qlonglong id = m_cmbConcesion->currentData().toLongLong();
    if (id <= 0) return std::nullopt;
    return static_cast<int64_t>(id);
}

} // namespace App
