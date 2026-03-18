#pragma once
#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;

namespace App {

// Dialogo de verificacion de actualizaciones.
// Muestra la version instalada y sus notas, y permite consultar la API de
// GitHub Releases para detectar si hay una version mas reciente disponible.
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);

private slots:
    void onVerificarClicked();
    void onReplyFinished(QNetworkReply* reply);

private:
    void setupUi();
    void setStatus(const QString& html);

    QNetworkAccessManager* m_nam          = nullptr;
    QLabel*                m_lblStatus    = nullptr;
    QPushButton*           m_btnVerificar = nullptr;
    QPushButton*           m_btnDescargas = nullptr;
    QString                m_urlDescargas;
};

} // namespace App
