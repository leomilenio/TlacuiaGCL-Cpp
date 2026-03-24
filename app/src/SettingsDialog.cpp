#include "app/SettingsDialog.h"
#include "core/DatabaseManager.h"
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QMessageBox>
#include <QDate>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QFrame>
#include <QApplication>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QTextStream>
#include <QDebug>

namespace App {

// ===========================================================================
// Utilidades de hash y verificacion de schema (funciones libres de modulo)
// ===========================================================================

static QByteArray computeSha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hasher.addData(f.read(64 * 1024));
    }
    return hasher.result();
}

static bool guardarHashFile(const QString& hashFilePath,
                            const QByteArray& hash,
                            const QString& dbFileName) {
    QFile hf(hashFilePath);
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&hf);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QString::fromLatin1(hash.toHex()) << "  " << dbFileName << "\n";
    return true;
}

static QString leerHashDesdeArchivo(const QString& sha256Path) {
    QFile hf(sha256Path);
    if (!hf.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream ts(&hf);
    ts.setEncoding(QStringConverter::Utf8);
    const QString linea = ts.readLine().trimmed();
    return linea.split(QRegularExpression("\\s+")).value(0).toLower();
}

static int leerSchemaVersion(const QString& dbPath) {
    int version = -1;
    const QString connName = "verify_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase tempDb = QSqlDatabase::addDatabase("QSQLITE", connName);
        tempDb.setDatabaseName(dbPath);
        if (tempDb.open()) {
            QSqlQuery q(tempDb);
            if (q.exec("PRAGMA user_version") && q.next())
                version = q.value(0).toInt();
            tempDb.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return version;
}

// Catalogo completo c_RegimenFiscal del SAT (CFDI 4.0)
// Formato: "codigo – descripcion"
static const QStringList kRegimenesFiscales = {
    "— Sin especificar —",
    "601 – General de Ley Personas Morales",
    "603 – Personas Morales con Fines no Lucrativos",
    "605 – Sueldos y Salarios e Ingresos Asimilados a Salarios",
    "606 – Arrendamiento",
    "607 – Enajenación de Bienes",
    "608 – Demás ingresos",
    "609 – Consolidación",
    "610 – Residentes en el Extranjero sin Establecimiento Permanente en México",
    "611 – Ingresos por Dividendos (socios y accionistas)",
    "612 – Personas Físicas con Actividades Empresariales y Profesionales",
    "614 – Ingresos por intereses",
    "615 – Régimen de los ingresos por obtención de premios",
    "616 – Sin obligaciones fiscales",
    "620 – Sociedades Cooperativas de Producción que optan por diferir sus ingresos",
    "621 – Incorporación Fiscal",
    "622 – Actividades Agrícolas, Ganaderas, Silvícolas y Pesqueras",
    "623 – Opcional para Grupos de Sociedades",
    "624 – Coordinados",
    "625 – Régimen de las Actividades Empresariales con ingresos a través de Plataformas Tecnológicas",
    "626 – Régimen Simplificado de Confianza (RESICO)",
    "628 – Hidrocarburos",
    "629 – De los Regímenes Fiscales Preferentes y de las Empresas Multinacionales",
    "630 – Enajenación de acciones en bolsa de valores",
};

static const QStringList kTiposTelefono = {
    "WhatsApp", "Local", "Celular", "Oficina", "Fax", "Otro"
};

// ===========================================================================
// Constructor
// ===========================================================================

SettingsDialog::SettingsDialog(Calculadora::LibreriaConfigRepository& configRepo,
                               Calculadora::DatabaseManager&          dbManager,
                               QWidget* parent)
    : QDialog(parent)
    , m_configRepo(configRepo)
    , m_dbManager(dbManager)
{
    setWindowTitle("Preferencias");
    setMinimumSize(620, 560);
    setupUi();
    loadFromRepo();
}

// ===========================================================================
// setupUi
// ===========================================================================

void SettingsDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 12);
    root->setSpacing(0);

    m_tabs = new QTabWidget();

    auto* tabDatos = new QWidget();
    buildTabDatos(tabDatos);
    m_tabs->addTab(tabDatos, "Datos de la Librería");

    auto* tabDocs = new QWidget();
    buildTabDocumentos(tabDocs);
    m_tabs->addTab(tabDocs, "Documentos");

    auto* tabDB = new QWidget();
    buildTabBaseDatos(tabDB);
    m_tabs->addTab(tabDB, "Base de Datos");

    root->addWidget(m_tabs, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(12, 0, 12, 0);
    btnRow->addStretch();
    auto* btnCancelar = new QPushButton("Cancelar");
    auto* btnGuardar  = new QPushButton("Guardar");
    btnGuardar->setDefault(true);
    btnGuardar->setObjectName("primaryButton");
    connect(btnCancelar, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnGuardar,  &QPushButton::clicked, this, &SettingsDialog::onGuardarClicked);
    btnRow->addWidget(btnCancelar);
    btnRow->addWidget(btnGuardar);
    root->addLayout(btnRow);
}

// ===========================================================================
// Tab 1 — Datos de la Librería
// ===========================================================================

void SettingsDialog::buildTabDatos(QWidget* tab) {
    // Usamos QScrollArea para que el contenido no quede comprimido si el
    // dialogo es pequeño (ej: pantallas con escalado alto).
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    auto* inner  = new QWidget();
    auto* layout = new QVBoxLayout(inner);
    layout->setContentsMargins(16, 16, 16, 8);
    layout->setSpacing(16);
    scroll->setWidget(inner);

    // ---- Informacion General ----
    auto* grpInfo = new QGroupBox("Información General");
    auto* form    = new QFormLayout(grpInfo);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    m_txtLibreria = new QLineEdit();
    m_txtLibreria->setPlaceholderText("Nombre que aparece en encabezados de PDF");

    m_txtEmpresa = new QLineEdit();
    m_txtEmpresa->setPlaceholderText("Razón social (si difiere del nombre de librería)");

    m_txtRFC = new QLineEdit();
    m_txtRFC->setPlaceholderText("Ej: XAXX010101000  (12 o 13 caracteres)");
    m_txtRFC->setMaxLength(13);
    m_txtRFC->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[A-Z&Ñ]{3,4}[0-9]{6}[A-Z0-9]{3}"), this));

    m_cmbRegimen = new QComboBox();
    m_cmbRegimen->addItems(kRegimenesFiscales);
    m_cmbRegimen->setToolTip("Catálogo c_RegimenFiscal del SAT (CFDI 4.0)");
    // Sin este ajuste, el combo calcula su minimumSizeHint para mostrar el item
    // más largo ("625 – Régimen de las Actividades Empresariales..."), lo que
    // hace que el QFormLayout exija más ancho que el dialogo. Con AdjustToMinimum
    // el combo puede achicarse y se estira via ExpandingFieldsGrow hasta llenar
    // el ancho disponible, mostrando el texto truncado con "…" cuando es necesario.
    m_cmbRegimen->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_cmbRegimen->setMinimumContentsLength(10);

    m_txtEmail = new QLineEdit();
    m_txtEmail->setPlaceholderText("Ej: contacto@libreria.com.mx");

    m_txtContacto = new QLineEdit();
    m_txtContacto->setPlaceholderText("Ej: Nombre del contacto de la librería");

    form->addRow("Nombre de la librería *:", m_txtLibreria);
    form->addRow("Razón social / empresa:",  m_txtEmpresa);
    form->addRow("RFC:",                     m_txtRFC);
    form->addRow("Régimen fiscal (SAT):",    m_cmbRegimen);
    form->addRow("Correo electrónico:",      m_txtEmail);
    form->addRow("Nombre del contacto:",     m_txtContacto);
    layout->addWidget(grpInfo);

    // ---- Dirección de la Librería ----
    auto* grpDir  = new QGroupBox("Dirección de la Librería");
    auto* formDir = new QFormLayout(grpDir);
    formDir->setLabelAlignment(Qt::AlignRight);
    formDir->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formDir->setSpacing(8);

    m_txtDirCalle = new QLineEdit();
    m_txtDirCalle->setPlaceholderText("Ej: Av. Insurgentes Sur");

    // Número exterior e interior en una misma fila
    m_txtDirNumExt = new QLineEdit();
    m_txtDirNumExt->setPlaceholderText("Ej: 1234");
    m_txtDirNumInt = new QLineEdit();
    m_txtDirNumInt->setPlaceholderText("Opcional (Ej: A, 2B)");
    auto* numRow    = new QWidget();
    auto* numRowLay = new QHBoxLayout(numRow);
    numRowLay->setContentsMargins(0, 0, 0, 0);
    numRowLay->setSpacing(8);
    numRowLay->addWidget(m_txtDirNumExt, 1);
    auto* lblInt = new QLabel("Int.:");
    lblInt->setStyleSheet("color: palette(placeholderText); font-size: 9pt;");
    numRowLay->addWidget(lblInt);
    numRowLay->addWidget(m_txtDirNumInt, 1);

    m_txtDirCP = new QLineEdit();
    m_txtDirCP->setPlaceholderText("Ej: 06600");
    m_txtDirCP->setMaxLength(5);
    m_txtDirCP->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[0-9]{0,5}"), this));

    m_txtDirColonia = new QLineEdit();
    m_txtDirColonia->setPlaceholderText("Ej: Nápoles");

    m_txtDirMunicipio = new QLineEdit();
    m_txtDirMunicipio->setPlaceholderText("Ej: Benito Juárez");

    static const QStringList kEstados = {
        "— Sin especificar —",
        "Aguascalientes", "Baja California", "Baja California Sur",
        "Campeche", "Chiapas", "Chihuahua", "Ciudad de México",
        "Coahuila de Zaragoza", "Colima", "Durango", "Estado de México",
        "Guanajuato", "Guerrero", "Hidalgo", "Jalisco", "Michoacán de Ocampo",
        "Morelos", "Nayarit", "Nuevo León", "Oaxaca", "Puebla",
        "Querétaro", "Quintana Roo", "San Luis Potosí", "Sinaloa",
        "Sonora", "Tabasco", "Tamaulipas", "Tlaxcala",
        "Veracruz de Ignacio de la Llave", "Yucatán", "Zacatecas",
    };
    m_cmbDirEstado = new QComboBox();
    m_cmbDirEstado->addItems(kEstados);
    m_cmbDirEstado->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_cmbDirEstado->setMinimumContentsLength(10);

    formDir->addRow("Calle:",                m_txtDirCalle);
    formDir->addRow("Número exterior:",      numRow);
    formDir->addRow("Código postal:",        m_txtDirCP);
    formDir->addRow("Colonia:",              m_txtDirColonia);
    formDir->addRow("Municipio / Alcaldía:", m_txtDirMunicipio);
    formDir->addRow("Estado:",               m_cmbDirEstado);
    layout->addWidget(grpDir);

    // ---- Telefonos de contacto ----
    auto* grpTel   = new QGroupBox("Teléfonos de Contacto");
    auto* telOuter = new QVBoxLayout(grpTel);
    telOuter->setSpacing(6);

    auto* telHint = new QLabel("Agrega hasta 4 números. Cada uno puede tener un tipo distinto.");
    telHint->setStyleSheet("color: palette(placeholderText); font-size: 9pt;");
    telOuter->addWidget(telHint);

    m_telContainer = new QWidget();
    m_telLayout    = new QVBoxLayout(m_telContainer);
    m_telLayout->setContentsMargins(0, 0, 0, 0);
    m_telLayout->setSpacing(4);
    telOuter->addWidget(m_telContainer);

    m_btnAddTel = new QPushButton("+ Agregar teléfono");
    m_btnAddTel->setFixedWidth(160);
    connect(m_btnAddTel, &QPushButton::clicked, this, [this]{ addTelRow(); });
    telOuter->addWidget(m_btnAddTel, 0, Qt::AlignLeft);
    layout->addWidget(grpTel);

    // ---- Identidad Visual ----
    auto* grpLogos  = new QGroupBox("Identidad Visual");
    auto* logosLay  = new QHBoxLayout(grpLogos);
    logosLay->setSpacing(24);

    auto makeLogoBlock = [&](const QString& label,
                              QLabel*& preview,
                              auto onSelect, auto onQuitar) -> QWidget* {
        auto* box = new QWidget();
        auto* vl  = new QVBoxLayout(box);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(6);

        auto* lbl = new QLabel(QString("<b>%1</b>").arg(label));
        vl->addWidget(lbl);

        preview = new QLabel();
        preview->setFixedSize(130, 80);
        preview->setAlignment(Qt::AlignCenter);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setStyleSheet("background: palette(alternate-base); color: palette(mid); font-size:9pt;");
        preview->setText("Sin imagen");
        vl->addWidget(preview);

        auto* btnRow2 = new QHBoxLayout();
        auto* btnSel  = new QPushButton("Seleccionar…");
        auto* btnQuit = new QPushButton("Quitar");
        btnSel->setFixedHeight(24);
        btnQuit->setFixedHeight(24);
        btnRow2->addWidget(btnSel);
        btnRow2->addWidget(btnQuit);
        vl->addLayout(btnRow2);

        auto* hint = new QLabel("PNG o JPEG, máx. 2 MB");
        hint->setStyleSheet("color: palette(placeholderText); font-size:9pt;");
        vl->addWidget(hint);
        vl->addStretch();

        connect(btnSel,  &QPushButton::clicked, this, onSelect);
        connect(btnQuit, &QPushButton::clicked, this, onQuitar);
        return box;
    };

    auto* blkLib = makeLogoBlock("Logo de la librería", m_previewLibreria,
        [this]{ onSeleccionarLogoLibreria(); },
        [this]{ onQuitarLogoLibreria(); });
    auto* blkEmp = makeLogoBlock("Logo de la empresa",  m_previewEmpresa,
        [this]{ onSeleccionarLogoEmpresa(); },
        [this]{ onQuitarLogoEmpresa(); });

    logosLay->addWidget(blkLib);
    auto* vsep = new QFrame();
    vsep->setFrameShape(QFrame::VLine);
    vsep->setFrameShadow(QFrame::Sunken);
    logosLay->addWidget(vsep);
    logosLay->addWidget(blkEmp);
    logosLay->addStretch();
    layout->addWidget(grpLogos);
    layout->addStretch();
}

// ===========================================================================
// Helpers — telefonos dinamicos
// ===========================================================================

void SettingsDialog::addTelRow(const QString& tipo, const QString& numero) {
    if (m_telRows.size() >= MAX_TELEFONOS) return;

    auto* rowWidget = new QWidget();
    auto* hl = new QHBoxLayout(rowWidget);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);

    auto* cmbTipo = new QComboBox();
    cmbTipo->addItems(kTiposTelefono);
    cmbTipo->setFixedWidth(110);
    if (!tipo.isEmpty()) cmbTipo->setCurrentText(tipo);
    else                 cmbTipo->setCurrentText("Local");

    auto* txtNum = new QLineEdit();
    txtNum->setPlaceholderText("Ej: 55 1234 5678");
    if (!numero.isEmpty()) txtNum->setText(numero);

    auto* btnRemove = new QPushButton("✕");
    btnRemove->setFixedSize(24, 24);
    btnRemove->setToolTip("Quitar este teléfono");

    hl->addWidget(cmbTipo);
    hl->addWidget(txtNum, 1);
    hl->addWidget(btnRemove);

    m_telRows.append({ rowWidget, cmbTipo, txtNum });
    m_telLayout->addWidget(rowWidget);
    m_btnAddTel->setEnabled(m_telRows.size() < MAX_TELEFONOS);

    connect(btnRemove, &QPushButton::clicked, this, [this, rowWidget, cmbTipo]() {
        for (int i = 0; i < m_telRows.size(); ++i) {
            if (m_telRows[i].tipo == cmbTipo) {
                m_telRows.removeAt(i);
                break;
            }
        }
        rowWidget->deleteLater();
        m_btnAddTel->setEnabled(m_telRows.size() < MAX_TELEFONOS);
    });
}

// ===========================================================================
// Tab 2 — Documentos (propuesta / placeholder)
// ===========================================================================

void SettingsDialog::buildTabDocumentos(QWidget* tab) {
    // QScrollArea para consistencia con buildTabDatos y pantallas con escala alta
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    auto* inner  = new QWidget();
    auto* layout = new QVBoxLayout(inner);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
    scroll->setWidget(inner);

    // ---- Descripcion general ----
    auto* lblDesc = new QLabel(
        "Todos los documentos generados por TlacuiaGCL incluyen automáticamente "
        "la información de la librería configurada en la pestaña <b>Datos de la Librería</b>. "
        "En esta sección podrás personalizar su presentación.");
    lblDesc->setWordWrap(true);
    layout->addWidget(lblDesc);

    // ---- Datos incluidos automaticamente ----
    // QGridLayout de 2 columnas para alineacion real independiente de la fuente
    auto* grpDatos = new QGroupBox("Datos incluidos en todos los documentos");
    auto* datosLay = new QVBoxLayout(grpDatos);
    datosLay->setContentsMargins(12, 8, 12, 12);
    datosLay->setSpacing(0);

    struct ItemPair { QString izq; QString der; };
    const QVector<ItemPair> pares = {
        {"✓  Logo de la librería",     "✓  Logo de la empresa"},
        {"✓  Nombre de la librería",   "✓  Razón social"},
        {"✓  RFC",                     "✓  Régimen fiscal (SAT)"},
        {"✓  Teléfonos de contacto",   "✓  Correo electrónico"},
        {"✓  Número de folio",         "✓  Fecha de generación"},
        {"✓  Nombre del distribuidor", "✓  Firmas (opcional al generar)"},
    };
    auto* itemGrid = new QGridLayout();
    itemGrid->setVerticalSpacing(4);
    itemGrid->setHorizontalSpacing(24);
    itemGrid->setColumnStretch(0, 1);
    itemGrid->setColumnStretch(1, 1);
    for (int i = 0; i < pares.size(); ++i) {
        auto* lIzq = new QLabel(pares[i].izq);
        auto* lDer = new QLabel(pares[i].der);
        // palette(text) se adapta automaticamente a modo oscuro y claro
        lIzq->setStyleSheet("font-size: 9.5pt; color: palette(text);");
        lDer->setStyleSheet("font-size: 9.5pt; color: palette(text);");
        itemGrid->addWidget(lIzq, i, 0);
        itemGrid->addWidget(lDer, i, 1);
    }
    datosLay->addLayout(itemGrid);
    layout->addWidget(grpDatos);

    // ---- Personalizacion (placeholder — proximo sprint) ----
    auto* grpPersonal = new QGroupBox("Personalización de documentos");
    auto* personalLay = new QVBoxLayout(grpPersonal);
    personalLay->setSpacing(10);

    auto* lblProx = new QLabel(
        "La personalización de documentos estará disponible en una próxima versión. "
        "Las opciones a continuación son una vista previa de las funciones planeadas.");
    lblProx->setWordWrap(true);
    lblProx->setStyleSheet("color: palette(mid); font-size: 9pt; font-style: italic;");
    personalLay->addWidget(lblProx);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setSpacing(8);

    // Tamanio de papel
    auto* cmbPapel = new QComboBox();
    cmbPapel->addItems({"Carta (Letter, 8.5\" × 11\")", "Oficio (Legal, 8.5\" × 14\")", "A4 (210 × 297 mm)"});
    cmbPapel->setEnabled(false);
    form->addRow("Tamaño de papel:", cmbPapel);

    // Orientacion
    auto* cmbOrientacion = new QComboBox();
    cmbOrientacion->addItems({"Vertical (Portrait)", "Horizontal (Landscape)"});
    cmbOrientacion->setEnabled(false);
    form->addRow("Orientación:", cmbOrientacion);

    // Color primario — QLabel habilitado (no setEnabled(false)) para que el
    // background #1a3a5c no quede oscurecido por el estado disabled en dark mode.
    // El cursor de prohibicion comunica visualmente que no es interactivo.
    auto* lblColor = new QWidget();
    lblColor->setFixedHeight(26);
    lblColor->setCursor(Qt::ForbiddenCursor);
    auto* colorHl = new QHBoxLayout(lblColor);
    colorHl->setContentsMargins(0, 0, 0, 0);
    colorHl->setSpacing(8);
    auto* swatchLabel = new QLabel();
    swatchLabel->setFixedSize(18, 18);
    swatchLabel->setStyleSheet(
        "background: #1a3a5c; border-radius: 3px; border: 1px solid rgba(255,255,255,0.15);");
    auto* colorText = new QLabel("#1a3a5c — Azul corporativo");
    colorText->setStyleSheet("font-size: 9pt; color: palette(mid);");
    colorHl->addWidget(swatchLabel);
    colorHl->addWidget(colorText);
    colorHl->addStretch();
    form->addRow("Color primario:", lblColor);

    // Fuente del cuerpo
    auto* cmbFuente = new QComboBox();
    cmbFuente->addItems({"Arial / Helvetica Neue", "Times New Roman", "Georgia"});
    cmbFuente->setEnabled(false);
    form->addRow("Fuente del cuerpo:", cmbFuente);

    personalLay->addLayout(form);

    // Separador
    auto* hsep = new QFrame();
    hsep->setFrameShape(QFrame::HLine);
    hsep->setFrameShadow(QFrame::Sunken);
    personalLay->addWidget(hsep);

    // Pie de pagina — QLabel con borde imitando un campo de texto; mas legible
    // que un QLineEdit deshabilitado en dark mode (que pierde contraste).
    auto* lblPieDesc = new QLabel("El pie de página muestra el nombre del sistema, la versión y la numeración de páginas:");
    lblPieDesc->setWordWrap(true);
    lblPieDesc->setStyleSheet("font-size: 9pt;");
    personalLay->addWidget(lblPieDesc);

    auto* lblPieValor = new QLabel(
        "TlacuiaGCL - Gestor de Concesiones para Librerías  |  Versión X.X.X  |  Pág. 1 de N");
    lblPieValor->setStyleSheet(
        "font-size: 9pt; font-style: italic; color: palette(mid); "
        "background: palette(base); "
        "border: 1px solid palette(mid); "
        "border-radius: 3px; "
        "padding: 4px 8px;");
    personalLay->addWidget(lblPieValor);

    layout->addWidget(grpPersonal);
    layout->addStretch();
}

// ===========================================================================
// Tab 3 — Base de Datos
// ===========================================================================

void SettingsDialog::buildTabBaseDatos(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    const bool darkMode = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const QString warnColor = darkMode ? QStringLiteral("#FF8A65") : QStringLiteral("#E65100");

    auto* grpUbic = new QGroupBox("Ubicación de la Base de Datos");
    auto* vlUbic = new QVBoxLayout(grpUbic);
    auto* lblRuta = new QLabel(m_dbManager.dbPath());
    lblRuta->setWordWrap(true);
    lblRuta->setStyleSheet("font-family: monospace; font-size: 10pt;");
    lblRuta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlUbic->addWidget(lblRuta);
    layout->addWidget(grpUbic);

    auto* grpBackup = new QGroupBox("Crear Respaldo");
    auto* vlBackup = new QVBoxLayout(grpBackup);
    auto* lblBackupInfo = new QLabel(
        "Genera un respaldo de la base de datos actual. Se crean dos archivos:\n"
        "el archivo de base de datos (.db) y su firma de integridad (.sha256).\n"
        "Ambos archivos son necesarios para restaurar un respaldo en el futuro.");
    lblBackupInfo->setWordWrap(true);
    vlBackup->addWidget(lblBackupInfo);
    auto* lblBackupAviso = new QLabel(
        "Guarda los respaldos en una ubicación distinta al equipo de trabajo.");
    lblBackupAviso->setWordWrap(true);
    lblBackupAviso->setStyleSheet(QString("color: %1; font-size: 9pt;").arg(warnColor));
    vlBackup->addWidget(lblBackupAviso);
    auto* btnBackup = new QPushButton("Crear respaldo…");
    btnBackup->setFixedWidth(160);
    connect(btnBackup, &QPushButton::clicked, this, &SettingsDialog::onCrearRespaldo);
    vlBackup->addWidget(btnBackup, 0, Qt::AlignLeft);
    layout->addWidget(grpBackup);

    auto* grpRestore = new QGroupBox("Cargar Respaldo");
    auto* vlRestore = new QVBoxLayout(grpRestore);
    auto* lblRestoreInfo = new QLabel(
        "Restaura la base de datos desde un respaldo previamente generado.\n"
        "Necesitarás seleccionar ambos archivos del bundle de respaldo:\n"
        "el archivo de base de datos (.db) y su firma (.sha256).");
    lblRestoreInfo->setWordWrap(true);
    vlRestore->addWidget(lblRestoreInfo);
    auto* lblRestoreAviso = new QLabel(
        "Solo se aceptan respaldos compatibles con la versión actual del sistema.\n"
        "La aplicación se cerrará al completar el proceso de restauración.");
    lblRestoreAviso->setWordWrap(true);
    lblRestoreAviso->setStyleSheet(QString("color: %1; font-size: 9pt;").arg(warnColor));
    vlRestore->addWidget(lblRestoreAviso);
    auto* btnRestore = new QPushButton("Cargar respaldo…");
    btnRestore->setFixedWidth(160);
    connect(btnRestore, &QPushButton::clicked, this, &SettingsDialog::onCargarRespaldo);
    vlRestore->addWidget(btnRestore, 0, Qt::AlignLeft);
    layout->addWidget(grpRestore);

    layout->addStretch();
}

// ===========================================================================
// Cargar / guardar configuracion
// ===========================================================================

void SettingsDialog::loadFromRepo() {
    const auto cfg = m_configRepo.load();
    m_txtLibreria->setText(cfg.libreriaNombre);
    m_txtEmpresa->setText(cfg.empresaNombre);
    m_txtRFC->setText(cfg.rfc.toUpper());
    m_txtEmail->setText(cfg.email);

    // Si aun no se ha guardado un nombre de contacto, precargamos el valor
    // por defecto: "Libreria <nombre de la libreria>".
    if (cfg.contactoNombre.isEmpty() && !cfg.libreriaNombre.isEmpty())
        m_txtContacto->setText("Librería " + cfg.libreriaNombre);
    else
        m_txtContacto->setText(cfg.contactoNombre);

    // Dirección
    m_txtDirCalle->setText(cfg.dirCalle);
    m_txtDirNumExt->setText(cfg.dirNumExterior);
    m_txtDirNumInt->setText(cfg.dirNumInterior);
    m_txtDirCP->setText(cfg.dirCodigoPostal);
    m_txtDirColonia->setText(cfg.dirColonia);
    m_txtDirMunicipio->setText(cfg.dirMunicipio);
    {
        const int idx = m_cmbDirEstado->findText(cfg.dirEstado);
        m_cmbDirEstado->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    // Regimen fiscal: buscar el item correspondiente en el combobox
    if (!cfg.regimenFiscalCode.isEmpty()) {
        const QString buscar = cfg.regimenFiscalCode + " – " + cfg.regimenFiscalDesc;
        const int idx = m_cmbRegimen->findText(buscar);
        if (idx >= 0) m_cmbRegimen->setCurrentIndex(idx);
        else          m_cmbRegimen->setCurrentIndex(0); // sin especificar
    } else {
        m_cmbRegimen->setCurrentIndex(0);
    }

    // Borrar filas previas y recargar telefonos
    for (auto& row : m_telRows) {
        if (row.widget) row.widget->deleteLater();
    }
    m_telRows.clear();
    for (const auto& tel : cfg.telefonos) {
        addTelRow(tel.tipo, tel.numero);
    }
    if (m_telRows.isEmpty()) {
        addTelRow("Local", "");  // al menos una fila visible
    }

    m_logoLibreria     = cfg.logoLibreria;
    m_logoLibreriaMime = cfg.logoLibreriaMime;
    updatePreview(m_previewLibreria, m_logoLibreria);

    m_logoEmpresa     = cfg.logoEmpresa;
    m_logoEmpresaMime = cfg.logoEmpresaMime;
    updatePreview(m_previewEmpresa, m_logoEmpresa);
}

void SettingsDialog::updatePreview(QLabel* preview, const QByteArray& data) {
    if (data.isEmpty()) {
        preview->setPixmap(QPixmap());
        preview->setText("Sin imagen");
        return;
    }
    QPixmap px;
    px.loadFromData(data);
    if (px.isNull()) { preview->setText("Error al cargar"); return; }
    preview->setPixmap(px.scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    preview->setText("");
}

static QByteArray cargarImagenDesdeArchivo(const QString& path, QString& mimeOut) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray data = f.readAll();
    if (data.size() > 2 * 1024 * 1024) return {};
    mimeOut = QFileInfo(path).suffix().toLower() == "png" ? "image/png" : "image/jpeg";
    return data;
}

// ===========================================================================
// Slots — logos
// ===========================================================================

void SettingsDialog::onSeleccionarLogoLibreria() {
    QString path = QFileDialog::getOpenFileName(
        this, "Seleccionar logo de la librería", {},
        "Imágenes (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;
    QString mime;
    QByteArray data = cargarImagenDesdeArchivo(path, mime);
    if (data.isEmpty()) {
        QMessageBox::warning(this, "Imagen no válida",
            "El archivo no se pudo leer o supera el límite de 2 MB.");
        return;
    }
    m_logoLibreria = data; m_logoLibreriaMime = mime;
    updatePreview(m_previewLibreria, data);
}

void SettingsDialog::onQuitarLogoLibreria() {
    m_logoLibreria.clear(); m_logoLibreriaMime.clear();
    updatePreview(m_previewLibreria, {});
}

void SettingsDialog::onSeleccionarLogoEmpresa() {
    QString path = QFileDialog::getOpenFileName(
        this, "Seleccionar logo de la empresa", {},
        "Imágenes (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;
    QString mime;
    QByteArray data = cargarImagenDesdeArchivo(path, mime);
    if (data.isEmpty()) {
        QMessageBox::warning(this, "Imagen no válida",
            "El archivo no se pudo leer o supera el límite de 2 MB.");
        return;
    }
    m_logoEmpresa = data; m_logoEmpresaMime = mime;
    updatePreview(m_previewEmpresa, data);
}

void SettingsDialog::onQuitarLogoEmpresa() {
    m_logoEmpresa.clear(); m_logoEmpresaMime.clear();
    updatePreview(m_previewEmpresa, {});
}

// ===========================================================================
// Slot — Guardar
// ===========================================================================

void SettingsDialog::onGuardarClicked() {
    Calculadora::LibreriaConfig cfg;
    cfg.libreriaNombre = m_txtLibreria->text().trimmed();
    cfg.empresaNombre  = m_txtEmpresa->text().trimmed();
    cfg.rfc            = m_txtRFC->text().trimmed().toUpper();
    cfg.email          = m_txtEmail->text().trimmed();
    cfg.contactoNombre   = m_txtContacto->text().trimmed();
    cfg.dirCalle         = m_txtDirCalle->text().trimmed();
    cfg.dirNumExterior   = m_txtDirNumExt->text().trimmed();
    cfg.dirNumInterior   = m_txtDirNumInt->text().trimmed();
    cfg.dirCodigoPostal  = m_txtDirCP->text().trimmed();
    cfg.dirColonia       = m_txtDirColonia->text().trimmed();
    cfg.dirMunicipio     = m_txtDirMunicipio->text().trimmed();
    cfg.dirEstado        = m_cmbDirEstado->currentIndex() > 0
                               ? m_cmbDirEstado->currentText() : QString{};

    // Regimen fiscal: extraer codigo y descripcion del item seleccionado
    const QString regimenText = m_cmbRegimen->currentText();
    if (m_cmbRegimen->currentIndex() > 0) {
        // Formato: "601 – Descripcion del regimen"
        const int sep = regimenText.indexOf(" – ");
        if (sep > 0) {
            cfg.regimenFiscalCode = regimenText.left(sep).trimmed();
            cfg.regimenFiscalDesc = regimenText.mid(sep + 3).trimmed();
        }
    }

    // Telefonos: recolectar filas no vacias
    for (const auto& row : m_telRows) {
        const QString num = row.numero->text().trimmed();
        if (!num.isEmpty()) {
            cfg.telefonos.append({ row.tipo->currentText(), num });
        }
    }

    cfg.logoLibreria     = m_logoLibreria;
    cfg.logoLibreriaMime = m_logoLibreriaMime;
    cfg.logoEmpresa      = m_logoEmpresa;
    cfg.logoEmpresaMime  = m_logoEmpresaMime;

    if (!m_configRepo.save(cfg)) {
        QMessageBox::critical(this, "Error",
            "No se pudieron guardar los datos. Revisa la consola para más detalles.");
        return;
    }
    accept();
}

// ===========================================================================
// Slot — Crear respaldo
// ===========================================================================

void SettingsDialog::onCrearRespaldo() {
    const QString fecha = QDate::currentDate().toString("yyyy-MM-dd");
    const QString defaultName = QString("respaldo_tlacuia_%1.db").arg(fecha);

    QString dest = QFileDialog::getSaveFileName(
        this, "Guardar respaldo de base de datos", defaultName,
        "Base de datos SQLite (*.db *.sqlite)");
    if (dest.isEmpty()) return;

    if (QFile::exists(dest) && !QFile::remove(dest)) {
        QMessageBox::critical(this, "Error",
            "No se pudo reemplazar el archivo existente.");
        return;
    }

    QSqlQuery checkpoint(m_dbManager.database());
    checkpoint.exec("PRAGMA wal_checkpoint(FULL)");

    if (!QFile::copy(m_dbManager.dbPath(), dest)) {
        QMessageBox::critical(this, "Error al crear respaldo",
            "No se pudo copiar la base de datos.\n"
            "Verifica que la ruta de destino sea accesible.");
        return;
    }

    const QByteArray hash = computeSha256(dest);
    if (hash.isEmpty()) {
        QMessageBox::warning(this, "Advertencia",
            "El respaldo se creó pero no se pudo generar su archivo de verificación.\n"
            "El respaldo puede no ser restaurable de forma segura.");
        return;
    }

    const QString hashPath = QFileInfo(dest).dir().filePath(
        QFileInfo(dest).completeBaseName() + ".sha256");
    if (!guardarHashFile(hashPath, hash, QFileInfo(dest).fileName())) {
        QMessageBox::warning(this, "Advertencia",
            "El respaldo se creó pero no se pudo guardar el archivo .sha256.\n"
            "Ruta intentada: " + hashPath);
        return;
    }

    QMessageBox::information(this, "Respaldo creado",
        QString("Respaldo guardado correctamente.\n\n"
                "Archivos generados:\n"
                "  • %1\n"
                "  • %2\n\n"
                "Tamaño: %3 KB\n"
                "Guarda ambos archivos en una ubicación segura.")
            .arg(QFileInfo(dest).fileName())
            .arg(QFileInfo(hashPath).fileName())
            .arg(QFileInfo(dest).size() / 1024));
}

// ===========================================================================
// Slot — Cargar respaldo
// ===========================================================================

void SettingsDialog::onCargarRespaldo() {
    const QString backupDb = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo de base de datos del respaldo", {},
        "Base de datos SQLite (*.db *.sqlite)");
    if (backupDb.isEmpty()) return;

    {
        const QString ext = QFileInfo(backupDb).suffix().toLower();
        if (ext != "db" && ext != "sqlite") {
            QMessageBox::warning(this, "Formato no válido",
                "El archivo seleccionado no tiene extensión .db o .sqlite.\n"
                "Selecciona un archivo de base de datos SQLite válido.");
            return;
        }
    }

    if (QFileInfo(backupDb).canonicalFilePath() ==
        QFileInfo(m_dbManager.dbPath()).canonicalFilePath()) {
        QMessageBox::warning(this, "Archivo no válido",
            "El archivo seleccionado es la base de datos activa.\n"
            "Selecciona un respaldo generado previamente.");
        return;
    }

    const QString sha256Sugerido = QFileInfo(backupDb).dir().filePath(
        QFileInfo(backupDb).completeBaseName() + ".sha256");
    const QString backupHash = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo de verificación del respaldo",
        QFile::exists(sha256Sugerido) ? sha256Sugerido
                                      : QFileInfo(backupDb).absolutePath(),
        "Archivo de verificación SHA-256 (*.sha256);;Todos los archivos (*)");
    if (backupHash.isEmpty()) return;

    if (QFileInfo(backupHash).suffix().toLower() != "sha256") {
        QMessageBox::warning(this, "Formato no válido",
            "El archivo de verificación no tiene extensión .sha256.\n"
            "Selecciona el archivo de firma generado junto con el respaldo.");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QByteArray hashCalculado = computeSha256(backupDb);
    QApplication::restoreOverrideCursor();

    if (hashCalculado.isEmpty()) {
        QMessageBox::critical(this, "Error de lectura",
            "No se pudo leer el archivo de respaldo para calcular su firma.");
        return;
    }

    const QString hashEsperado = leerHashDesdeArchivo(backupHash);
    if (hashEsperado.isEmpty()) {
        QMessageBox::critical(this, "Archivo de verificación no válido",
            "No se pudo leer el archivo .sha256 o su formato no es reconocido.");
        return;
    }

    if (QString::fromLatin1(hashCalculado.toHex()) != hashEsperado) {
        QMessageBox::critical(this, "Fallo de integridad",
            "La firma SHA-256 del archivo no coincide con la almacenada.\n\n"
            "El archivo de respaldo puede estar corrupto o haber sido modificado.\n"
            "No es seguro restaurar con este archivo.");
        return;
    }

    const int schemaRespaldo = leerSchemaVersion(backupDb);
    if (schemaRespaldo < 0) {
        QMessageBox::critical(this, "No se pudo verificar el respaldo",
            "No fue posible abrir el archivo de respaldo para verificar\n"
            "su versión de schema. El archivo puede estar dañado.");
        return;
    }
    if (schemaRespaldo != Calculadora::SCHEMA_VERSION_CURRENT) {
        QMessageBox::critical(this, "Respaldo incompatible",
            QString("El respaldo tiene una versión de base de datos diferente a la actual.\n\n"
                    "  Versión del respaldo:  %1\n"
                    "  Versión actual:        %2\n\n"
                    "Solo se pueden restaurar respaldos de la misma versión.\n"
                    "No se realizarán migraciones automáticas.")
                .arg(schemaRespaldo)
                .arg(Calculadora::SCHEMA_VERSION_CURRENT));
        return;
    }

    QMessageBox::information(this, "Respaldo de seguridad requerido",
        "Antes de continuar, debes crear un respaldo de seguridad de la\n"
        "base de datos actual. Se generarán dos archivos:\n\n"
        "  • TlacuiaGCL-DB-<fecha>.db\n"
        "  • TlacuiaGCL-DB-<fecha>.sha256\n\n"
        "Elige la carpeta donde guardarlos.");

    const QString fechaSeguridad  = QDate::currentDate().toString("ddMMyyyy");
    const QString nombreSeguridad = QString("TlacuiaGCL-DB-%1.db").arg(fechaSeguridad);

    const QString destSeguridad = QFileDialog::getSaveFileName(
        this, "Guardar respaldo de seguridad", nombreSeguridad,
        "Base de datos SQLite (*.db)");
    if (destSeguridad.isEmpty()) {
        QMessageBox::warning(this, "Restauración cancelada",
            "Debes crear el respaldo de seguridad para continuar.\n"
            "La restauración fue cancelada.");
        return;
    }

    if (QFile::exists(destSeguridad) && !QFile::remove(destSeguridad)) {
        QMessageBox::critical(this, "Error",
            "No se pudo reemplazar el archivo de respaldo existente.");
        return;
    }

    QSqlQuery checkpoint(m_dbManager.database());
    checkpoint.exec("PRAGMA wal_checkpoint(FULL)");

    if (!QFile::copy(m_dbManager.dbPath(), destSeguridad)) {
        QMessageBox::critical(this, "Error al crear respaldo de seguridad",
            "No se pudo copiar la base de datos actual.\n"
            "La restauración fue cancelada.");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QByteArray hashSeguridad = computeSha256(destSeguridad);
    QApplication::restoreOverrideCursor();

    const QString hashSeguridadPath = QFileInfo(destSeguridad).dir().filePath(
        QFileInfo(destSeguridad).completeBaseName() + ".sha256");

    if (hashSeguridad.isEmpty() ||
        !guardarHashFile(hashSeguridadPath, hashSeguridad,
                         QFileInfo(destSeguridad).fileName())) {
        QMessageBox::warning(this, "Advertencia",
            "El respaldo de seguridad se creó pero no se pudo generar su .sha256.\n"
            "Continúa bajo tu propio riesgo o repite el proceso.");
    }

    const QString candidato = m_dbManager.dbPath() + ".restore_candidate";
    if (QFile::exists(candidato)) QFile::remove(candidato);
    if (!QFile::copy(backupDb, candidato)) {
        QMessageBox::critical(this, "Error al preparar restauración",
            "No se pudo copiar el respaldo a la ubicación de trabajo.\n"
            "El respaldo de seguridad ya fue creado correctamente.");
        return;
    }

    QMessageBox msgCierre(this);
    msgCierre.setWindowTitle("Restauración lista — la app se cerrará");
    msgCierre.setIcon(QMessageBox::Information);
    msgCierre.setText(
        "<b>¡Todo listo! La restauración fue preparada correctamente.</b>");
    msgCierre.setInformativeText(
        QString("Se creó un respaldo de seguridad de tu base de datos actual:\n"
                "  • %1\n"
                "  • %2\n\n"
                "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
                "⚠️  La aplicación se va a cerrar ahora.\n"
                "Esto es NORMAL y necesario para aplicar la restauración.\n\n"
                "👉 Vuelve a abrir la aplicación manualmente\n"
                "    y la nueva base de datos se cargará sola.")
            .arg(QFileInfo(destSeguridad).fileName())
            .arg(QFileInfo(hashSeguridadPath).fileName()));
    msgCierre.setStandardButtons(QMessageBox::Ok);
    msgCierre.setDefaultButton(QMessageBox::Ok);
    msgCierre.exec();

    QApplication::quit();
}

} // namespace App
