#include "app/NuevaConcesionDialog.h"
#include <QComboBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDate>

namespace App {

NuevaConcesionDialog::NuevaConcesionDialog(Calculadora::EmisorRepository& emisorRepo,
                                           QWidget* parent)
    : QDialog(parent)
    , m_emisorRepo(emisorRepo)
{
    setupUi();
    loadEmisores();
    setWindowTitle("Nueva Concesion");
}

NuevaConcesionDialog::NuevaConcesionDialog(const Calculadora::ConcesionRecord& record,
                                           Calculadora::EmisorRepository& emisorRepo,
                                           QWidget* parent)
    : QDialog(parent)
    , m_emisorRepo(emisorRepo)
    , m_editMode(true)
    , m_editId(record.id)
{
    setupUi();
    loadEmisores();
    populateFrom(record);
    setWindowTitle("Editar Concesion");
}

void NuevaConcesionDialog::setupUi() {
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 14, 16, 14);

    // ---- Emisor ----
    auto* emisorGroup  = new QGroupBox("Distribuidor / Emisor");
    auto* emisorLayout = new QVBoxLayout(emisorGroup);

    auto* cmbRow = new QHBoxLayout();
    cmbRow->addWidget(new QLabel("Seleccionar:"));
    m_cmbEmisor = new QComboBox();
    m_cmbEmisor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cmbRow->addWidget(m_cmbEmisor);
    emisorLayout->addLayout(cmbRow);

    m_grpNuevoEmisor = new QGroupBox("Datos del nuevo distribuidor");
    auto* nuevoForm  = new QFormLayout(m_grpNuevoEmisor);
    nuevoForm->setLabelAlignment(Qt::AlignRight);
    m_txtNombreEmisor   = new QLineEdit(); m_txtNombreEmisor->setPlaceholderText("Requerido");
    m_txtNombreVendedor = new QLineEdit(); m_txtNombreVendedor->setPlaceholderText("Representante de ventas");
    m_txtTelefono       = new QLineEdit(); m_txtTelefono->setPlaceholderText("Opcional");
    m_txtEmail          = new QLineEdit(); m_txtEmail->setPlaceholderText("Opcional");
    nuevoForm->addRow("Nombre *:",    m_txtNombreEmisor);
    nuevoForm->addRow("Vendedor:",    m_txtNombreVendedor);
    nuevoForm->addRow("Telefono:",    m_txtTelefono);
    nuevoForm->addRow("Email:",       m_txtEmail);
    m_grpNuevoEmisor->setVisible(false);
    emisorLayout->addWidget(m_grpNuevoEmisor);

    mainLayout->addWidget(emisorGroup);

    // ---- Datos de la concesion ----
    auto* concGroup  = new QGroupBox("Datos de la concesion");
    auto* concForm   = new QFormLayout(concGroup);
    concForm->setLabelAlignment(Qt::AlignRight);

    m_cmbTipo = new QComboBox();
    m_cmbTipo->addItem("Factura",         QVariant("Factura"));
    m_cmbTipo->addItem("Nota de credito", QVariant("Nota de credito"));
    concForm->addRow("Tipo:", m_cmbTipo);

    m_txtFolio = new QLineEdit(); m_txtFolio->setPlaceholderText("Numero de folio / referencia *");
    concForm->addRow("Folio *:", m_txtFolio);

    m_dateFechaRec = new QDateEdit(QDate::currentDate());
    m_dateFechaRec->setCalendarPopup(true);
    m_dateFechaRec->setDisplayFormat("yyyy-MM-dd");
    concForm->addRow("Recepcion:", m_dateFechaRec);

    mainLayout->addWidget(concGroup);

    // ---- Vencimiento ----
    auto* vencGroup  = new QGroupBox("Fecha de vencimiento");
    auto* vencLayout = new QVBoxLayout(vencGroup);

    auto* rdGroup = new QButtonGroup(this);
    m_rdDias        = new QRadioButton("En dias a partir de recepcion:");
    m_rdFechaExacta = new QRadioButton("Fecha exacta:");
    m_rdDias->setChecked(true);
    rdGroup->addButton(m_rdDias,        0);
    rdGroup->addButton(m_rdFechaExacta, 1);

    auto* diasRow = new QHBoxLayout();
    diasRow->addWidget(m_rdDias);
    m_spnDias = new QSpinBox();
    m_spnDias->setRange(1, 3650);
    m_spnDias->setValue(30);
    m_spnDias->setSuffix(" dias");
    diasRow->addWidget(m_spnDias);
    diasRow->addStretch();
    vencLayout->addLayout(diasRow);

    auto* fechaRow = new QHBoxLayout();
    fechaRow->addWidget(m_rdFechaExacta);
    m_dateFechaVenc = new QDateEdit(QDate::currentDate().addDays(30));
    m_dateFechaVenc->setCalendarPopup(true);
    m_dateFechaVenc->setDisplayFormat("yyyy-MM-dd");
    m_dateFechaVenc->setEnabled(false);
    fechaRow->addWidget(m_dateFechaVenc);
    fechaRow->addStretch();
    vencLayout->addLayout(fechaRow);

    mainLayout->addWidget(vencGroup);

    // ---- Notas ----
    auto* notasGroup  = new QGroupBox("Notas (opcional)");
    auto* notasLayout = new QVBoxLayout(notasGroup);
    m_txtNotas = new QLineEdit();
    m_txtNotas->setPlaceholderText("Observaciones adicionales...");
    notasLayout->addWidget(m_txtNotas);
    mainLayout->addWidget(notasGroup);

    // ---- Comision ----
    auto* comisionGroup  = new QGroupBox("Comision acordada");
    auto* comisionLayout = new QVBoxLayout(comisionGroup);

    auto* comisionBtnGrp = new QButtonGroup(this);
    m_rdComisionEstandar = new QRadioButton("Comision estandar (30.0%)");
    m_rdComisionCustom   = new QRadioButton("Otro porcentaje:");
    m_rdComisionEstandar->setChecked(true);
    comisionBtnGrp->addButton(m_rdComisionEstandar, 0);
    comisionBtnGrp->addButton(m_rdComisionCustom,   1);

    auto* customRow = new QHBoxLayout();
    customRow->addWidget(m_rdComisionCustom);
    m_spnComision = new QDoubleSpinBox();
    m_spnComision->setRange(1.0, 99.0);
    m_spnComision->setDecimals(1);
    m_spnComision->setSuffix(" %");
    m_spnComision->setValue(30.0);
    m_spnComision->setEnabled(false);
    customRow->addWidget(m_spnComision);
    customRow->addStretch();

    comisionLayout->addWidget(m_rdComisionEstandar);
    comisionLayout->addLayout(customRow);
    mainLayout->addWidget(comisionGroup);

    // ---- Documentos adjuntos ----
    auto* adjGroup  = new QGroupBox("Documentos adjuntos (opcional)");
    auto* adjLayout = new QVBoxLayout(adjGroup);

    m_listaAdjuntos = new QListWidget();
    m_listaAdjuntos->setMaximumHeight(80);
    m_listaAdjuntos->setToolTip("Archivos PDF o Excel a vincular a esta concesion");

    auto* adjBtnRow      = new QHBoxLayout();
    auto* btnSeleccionar = new QPushButton("+ Seleccionar archivos...");
    auto* btnQuitar      = new QPushButton("Quitar");
    adjBtnRow->addWidget(btnSeleccionar);
    adjBtnRow->addWidget(btnQuitar);
    adjBtnRow->addStretch();

    adjLayout->addWidget(m_listaAdjuntos);
    adjLayout->addLayout(adjBtnRow);
    mainLayout->addWidget(adjGroup);

    connect(comisionBtnGrp, &QButtonGroup::idToggled,
            this, [this](int, bool) { onComisionToggled(); });
    connect(btnSeleccionar, &QPushButton::clicked,
            this, &NuevaConcesionDialog::onSeleccionarAdjuntosClicked);
    connect(btnQuitar, &QPushButton::clicked,
            this, &NuevaConcesionDialog::onQuitarAdjuntoClicked);

    // ---- Botones ----
    auto* btnRow    = new QHBoxLayout();
    auto* acceptBtn = new QPushButton(m_editMode ? "Actualizar" : "Crear");
    auto* cancelBtn = new QPushButton("Cancelar");
    acceptBtn->setObjectName("primaryButton");
    acceptBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(acceptBtn);
    mainLayout->addLayout(btnRow);

    // Conexiones
    connect(m_cmbEmisor, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NuevaConcesionDialog::onEmisorSelectionChanged);
    connect(rdGroup, &QButtonGroup::idToggled,
            this, [this](int, bool) { onFechaToggled(); });
    connect(acceptBtn, &QPushButton::clicked, this, &NuevaConcesionDialog::onAcceptClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void NuevaConcesionDialog::loadEmisores() {
    m_cmbEmisor->clear();
    m_cmbEmisor->addItem("(Nuevo distribuidor...)", QVariant(-1LL));
    const auto emisores = m_emisorRepo.findAll();
    for (const auto& e : emisores) {
        QString label = e.nombreVendedor.isEmpty()
            ? e.nombreEmisor
            : QString("%1 — %2").arg(e.nombreEmisor, e.nombreVendedor);
        m_cmbEmisor->addItem(label, QVariant::fromValue(static_cast<qlonglong>(e.id)));
    }
}

void NuevaConcesionDialog::onEmisorSelectionChanged(int /*index*/) {
    bool esNuevo = (m_cmbEmisor->currentData().toLongLong() == -1LL);
    m_grpNuevoEmisor->setVisible(esNuevo);
    adjustSize();
}

void NuevaConcesionDialog::onFechaToggled() {
    bool diasMode = m_rdDias->isChecked();
    m_spnDias->setEnabled(diasMode);
    m_dateFechaVenc->setEnabled(!diasMode);
}

void NuevaConcesionDialog::onAcceptClicked() {
    // Validar folio
    if (m_txtFolio->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido", "El folio es obligatorio.");
        m_txtFolio->setFocus();
        return;
    }
    // Validar emisor
    bool esNuevo = (m_cmbEmisor->currentData().toLongLong() == -1LL);
    if (esNuevo && m_txtNombreEmisor->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Campo requerido",
            "El nombre del distribuidor es obligatorio.");
        m_txtNombreEmisor->setFocus();
        return;
    }
    accept();
}

void NuevaConcesionDialog::populateFrom(const Calculadora::ConcesionRecord& record) {
    // Seleccionar el emisor existente en el combo
    for (int i = 0; i < m_cmbEmisor->count(); ++i) {
        if (m_cmbEmisor->itemData(i).toLongLong() == record.emisorId) {
            m_cmbEmisor->setCurrentIndex(i);
            break;
        }
    }
    // Tipo de documento
    int tipoIdx = (record.tipoDocumento == Calculadora::TipoDocumentoConcesion::NotaDeCredito) ? 1 : 0;
    m_cmbTipo->setCurrentIndex(tipoIdx);
    m_txtFolio->setText(record.folio);
    if (!record.fechaRecepcion.isEmpty())
        m_dateFechaRec->setDate(QDate::fromString(record.fechaRecepcion, Qt::ISODate));
    if (!record.fechaVencimiento.isEmpty()) {
        m_rdFechaExacta->setChecked(true);
        m_dateFechaVenc->setDate(QDate::fromString(record.fechaVencimiento, Qt::ISODate));
        onFechaToggled();
    }
    m_txtNotas->setText(record.notas);

    // Comisión
    if (record.comisionPct != 30.0) {
        m_rdComisionCustom->setChecked(true);
        m_spnComision->setValue(record.comisionPct);
        m_spnComision->setEnabled(true);
    } else {
        m_rdComisionEstandar->setChecked(true);
        m_spnComision->setEnabled(false);
    }
}

Calculadora::ConcesionRecord NuevaConcesionDialog::result() const {
    Calculadora::ConcesionRecord r;
    r.id = m_editId;

    bool esNuevo = (m_cmbEmisor->currentData().toLongLong() == -1LL);
    if (esNuevo) {
        // Se creara el emisor en el llamador; aqui solo guardamos el nombre
        r.emisorId     = 0;
        r.emisorNombre = m_txtNombreEmisor->text().trimmed();
    } else {
        r.emisorId     = m_cmbEmisor->currentData().toLongLong();
        r.emisorNombre = m_cmbEmisor->currentText();
    }

    r.tipoDocumento = (m_cmbTipo->currentData().toString() == "Nota de credito")
                      ? Calculadora::TipoDocumentoConcesion::NotaDeCredito
                      : Calculadora::TipoDocumentoConcesion::Factura;
    r.folio         = m_txtFolio->text().trimmed();
    r.fechaRecepcion = m_dateFechaRec->date().toString(Qt::ISODate);

    if (m_rdDias->isChecked()) {
        QDate venc = m_dateFechaRec->date().addDays(m_spnDias->value());
        r.fechaVencimiento = venc.toString(Qt::ISODate);
    } else {
        r.fechaVencimiento = m_dateFechaVenc->date().toString(Qt::ISODate);
    }

    r.notas       = m_txtNotas->text().trimmed();
    r.activa      = true;
    r.comisionPct = m_rdComisionCustom->isChecked()
                    ? m_spnComision->value()
                    : 30.0;

    // Datos del nuevo emisor (el llamador debe usarlos para crear el registro)
    if (esNuevo) {
        r.emisorNombreVendedor = m_txtNombreVendedor->text().trimmed();
        // Telefono y email se pueden recuperar via los campos del dialog si el llamador
        // necesita crear el emisor; para simplificar los embebemos en emisorContacto:
        r.emisorContacto = m_txtTelefono->text().trimmed();
    }

    return r;
}

bool    NuevaConcesionDialog::isNuevoEmisor()       const { return m_cmbEmisor->currentData().toLongLong() == -1LL; }
QString NuevaConcesionDialog::nuevoEmisorNombre()   const { return m_txtNombreEmisor->text().trimmed(); }
QString NuevaConcesionDialog::nuevoEmisorVendedor() const { return m_txtNombreVendedor->text().trimmed(); }
QString NuevaConcesionDialog::nuevoEmisorTelefono() const { return m_txtTelefono->text().trimmed(); }
QString NuevaConcesionDialog::nuevoEmisorEmail()    const { return m_txtEmail->text().trimmed(); }

QStringList NuevaConcesionDialog::adjuntosSeleccionados() const {
    QStringList paths;
    for (int i = 0; i < m_listaAdjuntos->count(); ++i)
        paths << m_listaAdjuntos->item(i)->data(Qt::UserRole).toString();
    return paths;
}

void NuevaConcesionDialog::onComisionToggled() {
    m_spnComision->setEnabled(m_rdComisionCustom->isChecked());
}

void NuevaConcesionDialog::onSeleccionarAdjuntosClicked() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Seleccionar documentos", QString(),
        "Documentos soportados (*.pdf *.xlsx *.xls);;PDF (*.pdf);;Excel (*.xlsx *.xls)");
    for (const QString& path : files) {
        // Evitar duplicados
        bool dup = false;
        for (int i = 0; i < m_listaAdjuntos->count(); ++i) {
            if (m_listaAdjuntos->item(i)->data(Qt::UserRole).toString() == path) {
                dup = true; break;
            }
        }
        if (!dup) {
            auto* item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, path);
            m_listaAdjuntos->addItem(item);
        }
    }
    adjustSize();
}

void NuevaConcesionDialog::onQuitarAdjuntoClicked() {
    delete m_listaAdjuntos->takeItem(m_listaAdjuntos->currentRow());
    adjustSize();
}

} // namespace App
