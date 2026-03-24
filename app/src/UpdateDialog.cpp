#include "app/UpdateDialog.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>

namespace App {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* RELEASES_API =
    "https://api.github.com/repos/leomilenio/TlacuiaGCL-Cpp/releases/latest";

// Mapea QNetworkReply::NetworkError a mensajes amigables en español.
// Evita mostrar strings del SO (en inglés en Windows, variables en macOS).
static QString networkErrorToSpanish(QNetworkReply* reply) {
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
        return "No se encontró el servidor. Verifica tu conexión a internet.";
    case QNetworkReply::ConnectionRefusedError:
        return "El servidor rechazó la conexión.";
    case QNetworkReply::RemoteHostClosedError:
        return "El servidor cerró la conexión inesperadamente.";
    case QNetworkReply::TimeoutError:
        return "La solicitud tardó demasiado. Intenta de nuevo.";
    case QNetworkReply::SslHandshakeFailedError:
        return "Error de seguridad SSL. Tu sistema puede tener certificados desactualizados.";
    case QNetworkReply::ContentNotFoundError:
        return "No se encontraron releases publicados en el repositorio.";
    case QNetworkReply::TooManyRedirectsError:
        return "Demasiadas redirecciones al conectar con GitHub. Intenta de nuevo.";
    case QNetworkReply::AuthenticationRequiredError:
        return "El repositorio requiere autenticación (puede ser privado).";
    case QNetworkReply::OperationCanceledError:
        return "La operación fue cancelada.";
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
        return "Sin conexión a internet o red no disponible.";
    default:
        // No exponer el string del SO; sólo el código numérico para diagnóstico.
        return QString("Error de red (código %1). Verifica tu conexión a internet.")
               .arg(static_cast<int>(reply->error()));
    }
}

// Compara dos strings de version "X.Y.Z" — devuelve true si remote > current.
static bool isNewer(const QString& current, const QString& remote) {
    auto nums = [](const QString& v) {
        const QStringList p = v.split('.');
        return std::make_tuple(p.value(0).toInt(),
                               p.value(1).toInt(),
                               p.value(2).toInt());
    };
    const auto [cm, cn, cp] = nums(current);
    const auto [rm, rn, rp] = nums(remote);
    if (rm != cm) return rm > cm;
    if (rn != cn) return rn > cn;
    return rp > cp;
}

// Lee el JSON embebido en recursos y devuelve el campo solicitado.
static QString fromVersionJson(const QString& key) {
    QFile f(":/app/version_info.json");
    if (!f.open(QFile::ReadOnly)) return {};
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    return obj.value(key).toString();
}

// ---------------------------------------------------------------------------
// UpdateDialog
// ---------------------------------------------------------------------------

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    setupUi();
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &UpdateDialog::onReplyFinished);
}

void UpdateDialog::setupUi() {
    setWindowTitle("Buscar actualizaciones");
    setMinimumWidth(440);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    // Version actual + notas
    const QString currentVer = QApplication::applicationVersion();
    const QString notes      = fromVersionJson("notes");

    auto* lblVersion = new QLabel(
        QString("<b>Version instalada:</b> %1").arg(currentVer));
    layout->addWidget(lblVersion);

    if (!notes.isEmpty()) {
        auto* sep = new QFrame(); sep->setFrameShape(QFrame::HLine);
        layout->addWidget(sep);
        auto* lblNotes = new QLabel(notes);
        lblNotes->setWordWrap(true);
        lblNotes->setStyleSheet("color: palette(placeholder-text); font-size: 11px;");
        layout->addWidget(lblNotes);
    }

    auto* sep2 = new QFrame(); sep2->setFrameShape(QFrame::HLine);
    layout->addWidget(sep2);

    // Area de estado (resultado de la verificacion)
    m_lblStatus = new QLabel("Haz clic en \"Verificar\" para comprobar si hay actualizaciones.");
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setTextFormat(Qt::RichText);
    m_lblStatus->setOpenExternalLinks(false);
    layout->addWidget(m_lblStatus);

    // Boton "Ver descargas" (oculto hasta que haya una version nueva)
    m_btnDescargas = new QPushButton("Ver descargas en GitHub");
    m_btnDescargas->setObjectName("primaryButton");
    m_btnDescargas->setVisible(false);
    layout->addWidget(m_btnDescargas);
    connect(m_btnDescargas, &QPushButton::clicked, this, [this]() {
        if (!m_urlDescargas.isEmpty())
            QDesktopServices::openUrl(QUrl(m_urlDescargas));
    });

    // Fila de botones
    auto* btnRow = new QHBoxLayout();
    m_btnVerificar = new QPushButton("Verificar ahora");
    m_btnVerificar->setObjectName("primaryButton");
    auto* btnCerrar = new QPushButton("Cerrar");
    btnRow->addWidget(m_btnVerificar);
    btnRow->addStretch();
    btnRow->addWidget(btnCerrar);
    layout->addLayout(btnRow);

    connect(m_btnVerificar, &QPushButton::clicked, this, &UpdateDialog::onVerificarClicked);
    connect(btnCerrar,      &QPushButton::clicked, this, &QDialog::accept);
}

void UpdateDialog::setStatus(const QString& html) {
    m_lblStatus->setText(html);
}

void UpdateDialog::onVerificarClicked() {
    m_btnVerificar->setEnabled(false);
    m_btnDescargas->setVisible(false);
    setStatus("Verificando...");

    QNetworkRequest req{QUrl{RELEASES_API}};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("TlacuiaGCL-Cpp/%1 Qt/%2")
                      .arg(QApplication::applicationVersion(), qVersion()));
    req.setRawHeader(QByteArray("Accept"), QByteArray("application/vnd.github+json"));

    // SSL: documentamos explícitamente que se requiere verificación de certificados.
    // Qt usa VerifyPeer por defecto, pero lo fijamos para que no sea sobreescrito
    // por ninguna configuración global alterada en el entorno del usuario.
    QSslConfiguration sslConf = QSslConfiguration::defaultConfiguration();
    sslConf.setPeerVerifyMode(QSslSocket::VerifyPeer);
    req.setSslConfiguration(sslConf);

    m_nam->get(req);
}

void UpdateDialog::onReplyFinished(QNetworkReply* reply) {
    m_btnVerificar->setEnabled(true);
    reply->deleteLater();

    // Colores adaptativos segun el modo del sistema
    const bool dark      = palette().color(QPalette::Window).lightness() < 128;
    const QString cRojo  = dark ? QStringLiteral("#EF9A9A") : QStringLiteral("#C62828");
    const QString cNaranja = dark ? QStringLiteral("#FF8A65") : QStringLiteral("#E65100");
    const QString cVerde = dark ? QStringLiteral("#81C784") : QStringLiteral("#2E7D32");

    if (reply->error() != QNetworkReply::NoError) {
        setStatus(QString(
            "<span style='color:%1;'>&#10007; No se pudo conectar con GitHub.<br>"
            "<small>%2</small></span>").arg(cRojo, networkErrorToSpanish(reply).toHtmlEscaped()));
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (obj.isEmpty()) {
        setStatus(QString("<span style='color:%1;'>&#10007; Respuesta inesperada del servidor.</span>").arg(cRojo));
        return;
    }

    // tag_name suele tener el formato "v0.5.0" — quitamos la "v" inicial
    QString tagName = obj.value("tag_name").toString();
    if (tagName.startsWith('v', Qt::CaseInsensitive))
        tagName = tagName.mid(1);

    const QString releaseUrl = obj.value("html_url").toString();
    const QString current    = QApplication::applicationVersion();

    if (tagName.isEmpty()) {
        setStatus(QString("<span style='color:%1;'>&#10007; No se encontraron releases publicados.</span>").arg(cRojo));
        return;
    }

    if (isNewer(current, tagName)) {
        m_urlDescargas = releaseUrl;
        m_btnDescargas->setVisible(true);
        setStatus(QString(
            "<span style='color:%1;'><b>&#9888; Nueva version disponible: %2</b></span><br>"
            "<small>Version instalada: %3</small>")
            .arg(cNaranja, tagName, current));
    } else {
        setStatus(QString(
            "<span style='color:%1;'>&#10003; <b>La aplicacion esta al dia</b> (v%2)</span>")
            .arg(cVerde, current));
    }
}

} // namespace App
