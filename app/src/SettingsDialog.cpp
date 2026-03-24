#include "app/SettingsDialog.h"
#include "core/DatabaseManager.h"
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
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

// Calcula SHA-256 de un archivo en bloques (eficiente para archivos grandes).
static QByteArray computeSha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hasher.addData(f.read(64 * 1024));
    }
    return hasher.result();
}

// Guarda un archivo .sha256 en formato estandar compatible con sha256sum(1):
//   <64-hex>  <nombre_archivo>\n
static bool guardarHashFile(const QString& hashFilePath,
                            const QByteArray& hash,
                            const QString& dbFileName) {
    QFile hf(hashFilePath);
    if (!hf.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&hf);
    // Encoding UTF-8 explícito: garantiza que nombres con caracteres no-ASCII
    // (ej: tildes en rutas de Windows) se escriban correctamente en todas las plataformas.
    ts.setEncoding(QStringConverter::Utf8);
    ts << QString::fromLatin1(hash.toHex()) << "  " << dbFileName << "\n";
    return true;
}

// Lee el hash desde un archivo .sha256 (primer token de la primera linea).
static QString leerHashDesdeArchivo(const QString& sha256Path) {
    QFile hf(sha256Path);
    if (!hf.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream ts(&hf);
    ts.setEncoding(QStringConverter::Utf8);
    const QString linea = ts.readLine().trimmed();
    // Formato: "<hash>  <filename>" — tomamos solo el primer campo
    return linea.split(QRegularExpression("\\s+")).value(0).toLower();
}

// Abre una conexion SQLite temporal (solo lectura) para leer PRAGMA user_version.
// Devuelve -1 si no se puede abrir.
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
    QSqlDatabase::removeDatabase(connName); // safe: el objeto tempDb ya fue destruido
    return version;
}

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
    setMinimumSize(580, 500);
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
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 8);
    layout->setSpacing(16);

    auto* grpInfo = new QGroupBox("Información General");
    auto* form = new QFormLayout(grpInfo);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_txtLibreria = new QLineEdit();
    m_txtLibreria->setPlaceholderText("Nombre que aparece en encabezados de PDF");
    m_txtEmpresa  = new QLineEdit();
    m_txtEmpresa->setPlaceholderText("Razón social (si difiere del nombre de librería)");

    m_txtRFC = new QLineEdit();
    m_txtRFC->setPlaceholderText("Ej: XAXX010101000  (12 o 13 caracteres)");
    m_txtRFC->setMaxLength(13);
    m_txtRFC->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[A-Z&Ñ]{3,4}[0-9]{6}[A-Z0-9]{3}"), this));

    m_txtTel1 = new QLineEdit();
    m_txtTel1->setPlaceholderText("Ej: 55 1234 5678");
    m_txtTel2 = new QLineEdit();
    m_txtTel2->setPlaceholderText("Teléfono alternativo (opcional)");

    form->addRow("Nombre de la librería *:", m_txtLibreria);
    form->addRow("Razón social / empresa:",  m_txtEmpresa);
    form->addRow("RFC:",                     m_txtRFC);
    form->addRow("Teléfono 1:",              m_txtTel1);
    form->addRow("Teléfono 2:",              m_txtTel2);
    layout->addWidget(grpInfo);

    auto* grpLogos = new QGroupBox("Identidad Visual");
    auto* logosLayout = new QHBoxLayout(grpLogos);
    logosLayout->setSpacing(24);

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
        hint->setStyleSheet("color: palette(mid); font-size:9pt;");
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

    logosLayout->addWidget(blkLib);
    auto* vsep = new QFrame();
    vsep->setFrameShape(QFrame::VLine);
    vsep->setFrameShadow(QFrame::Sunken);
    logosLayout->addWidget(vsep);
    logosLayout->addWidget(blkEmp);
    logosLayout->addStretch();
    layout->addWidget(grpLogos);
    layout->addStretch();
}

// ===========================================================================
// Tab 2 — Base de Datos
// ===========================================================================

void SettingsDialog::buildTabBaseDatos(QWidget* tab) {
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    // Naranja adaptivo: más brillante en modo oscuro para mantener contraste legible
    const bool darkMode = QApplication::palette().color(QPalette::Window).lightness() < 128;
    const QString warnColor = darkMode ? QStringLiteral("#FF8A65") : QStringLiteral("#E65100");

    // ---- Ubicación actual ----
    auto* grpUbic = new QGroupBox("Ubicación de la Base de Datos");
    auto* vlUbic = new QVBoxLayout(grpUbic);
    auto* lblRuta = new QLabel(m_dbManager.dbPath());
    lblRuta->setWordWrap(true);
    lblRuta->setStyleSheet("font-family: monospace; font-size: 10pt;");
    lblRuta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vlUbic->addWidget(lblRuta);
    layout->addWidget(grpUbic);

    // ---- Respaldo manual ----
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

    // ---- Cargar respaldo ----
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
    m_txtTel1->setText(cfg.tel1);
    m_txtTel2->setText(cfg.tel2);

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
// Slot — Guardar datos de libreria
// ===========================================================================

void SettingsDialog::onGuardarClicked() {
    Calculadora::LibreriaConfig cfg;
    cfg.libreriaNombre   = m_txtLibreria->text().trimmed();
    cfg.empresaNombre    = m_txtEmpresa->text().trimmed();
    cfg.rfc              = m_txtRFC->text().trimmed().toUpper();
    cfg.tel1             = m_txtTel1->text().trimmed();
    cfg.tel2             = m_txtTel2->text().trimmed();
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
//   Genera: <nombre>.db  +  <nombre>.sha256 (ambos en la misma carpeta)
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

    // Asegurar que el WAL este integrado antes de copiar
    QSqlQuery checkpoint(m_dbManager.database());
    checkpoint.exec("PRAGMA wal_checkpoint(FULL)");

    if (!QFile::copy(m_dbManager.dbPath(), dest)) {
        QMessageBox::critical(this, "Error al crear respaldo",
            "No se pudo copiar la base de datos.\n"
            "Verifica que la ruta de destino sea accesible.");
        return;
    }

    // Generar firma SHA-256 del archivo copiado
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
//   Flujo:
//     1. Seleccionar .db del respaldo
//     2. Seleccionar .sha256 del respaldo
//     3. Verificar integridad (SHA-256)
//     4. Verificar compatibilidad de schema (PRAGMA user_version)
//     5. Forzar respaldo de seguridad de la DB actual (TlacuiaGCL-DB-ddMMyyyy)
//     6. Colocar .db en <dbActual>.restore_candidate
//     7. Cerrar la aplicacion — DatabaseManager aplicara el swap al reiniciar
// ===========================================================================

void SettingsDialog::onCargarRespaldo() {
    // -- Paso 1: seleccionar el .db del respaldo --
    const QString backupDb = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo de base de datos del respaldo", {},
        "Base de datos SQLite (*.db *.sqlite)");
    if (backupDb.isEmpty()) return;

    // Validación de extensión post-selección: en Windows el file dialog puede
    // mostrar archivos .DB (mayúsculas) que no coincidan con el filtro.
    {
        const QString ext = QFileInfo(backupDb).suffix().toLower();
        if (ext != "db" && ext != "sqlite") {
            QMessageBox::warning(this, "Formato no válido",
                "El archivo seleccionado no tiene extensión .db o .sqlite.\n"
                "Selecciona un archivo de base de datos SQLite válido.");
            return;
        }
    }

    // Evitar que el usuario seleccione la DB activa como respaldo
    if (QFileInfo(backupDb).canonicalFilePath() ==
        QFileInfo(m_dbManager.dbPath()).canonicalFilePath()) {
        QMessageBox::warning(this, "Archivo no válido",
            "El archivo seleccionado es la base de datos activa.\n"
            "Selecciona un respaldo generado previamente.");
        return;
    }

    // -- Paso 2: seleccionar el .sha256 del mismo respaldo --
    // Sugiere la carpeta del .db y el nombre esperado del hash
    const QString sha256Sugerido = QFileInfo(backupDb).dir().filePath(
        QFileInfo(backupDb).completeBaseName() + ".sha256");
    const QString backupHash = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo de verificación del respaldo",
        QFile::exists(sha256Sugerido) ? sha256Sugerido
                                      : QFileInfo(backupDb).absolutePath(),
        "Archivo de verificación SHA-256 (*.sha256);;Todos los archivos (*)");
    if (backupHash.isEmpty()) return;

    // Validación de extensión del archivo de verificación
    if (QFileInfo(backupHash).suffix().toLower() != "sha256") {
        QMessageBox::warning(this, "Formato no válido",
            "El archivo de verificación no tiene extensión .sha256.\n"
            "Selecciona el archivo de firma generado junto con el respaldo.");
        return;
    }

    // -- Paso 3: verificar integridad del respaldo --
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

    // -- Paso 4: verificar compatibilidad de schema --
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

    // -- Paso 5: forzar respaldo de seguridad de la DB actual --
    QMessageBox::information(this, "Respaldo de seguridad requerido",
        "Antes de continuar, debes crear un respaldo de seguridad de la\n"
        "base de datos actual. Se generarán dos archivos:\n\n"
        "  • TlacuiaGCL-DB-<fecha>.db\n"
        "  • TlacuiaGCL-DB-<fecha>.sha256\n\n"
        "Elige la carpeta donde guardarlos.");

    const QString fechaSeguridad = QDate::currentDate().toString("ddMMyyyy");
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

    // Checkpoint WAL y copiar
    QSqlQuery checkpoint(m_dbManager.database());
    checkpoint.exec("PRAGMA wal_checkpoint(FULL)");

    if (!QFile::copy(m_dbManager.dbPath(), destSeguridad)) {
        QMessageBox::critical(this, "Error al crear respaldo de seguridad",
            "No se pudo copiar la base de datos actual.\n"
            "La restauración fue cancelada.");
        return;
    }

    // Generar .sha256 del respaldo de seguridad
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

    // -- Paso 6: colocar el respaldo como candidato de restauracion --
    const QString candidato = m_dbManager.dbPath() + ".restore_candidate";
    if (QFile::exists(candidato)) QFile::remove(candidato);
    if (!QFile::copy(backupDb, candidato)) {
        QMessageBox::critical(this, "Error al preparar restauración",
            "No se pudo copiar el respaldo a la ubicación de trabajo.\n"
            "El respaldo de seguridad ya fue creado correctamente.");
        return;
    }

    // -- Paso 7: informar y cerrar la aplicacion --
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
