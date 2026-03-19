#pragma once
#include <QDialog>
#include <QByteArray>
#include "core/LibreriaConfigRepository.h"
#include "core/DatabaseManager.h"

class QTabWidget;
class QLineEdit;
class QLabel;

namespace App {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Calculadora::LibreriaConfigRepository& configRepo,
                            Calculadora::DatabaseManager&          dbManager,
                            QWidget* parent = nullptr);

private slots:
    void onGuardarClicked();
    void onSeleccionarLogoLibreria();
    void onQuitarLogoLibreria();
    void onSeleccionarLogoEmpresa();
    void onQuitarLogoEmpresa();
    void onCrearRespaldo();
    void onCargarRespaldo();

private:
    void setupUi();
    void buildTabDatos(QWidget* tab);
    void buildTabBaseDatos(QWidget* tab);
    void loadFromRepo();
    void updatePreview(QLabel* preview, const QByteArray& data);

    Calculadora::LibreriaConfigRepository& m_configRepo;
    Calculadora::DatabaseManager&          m_dbManager;

    // Tab Datos — campos de texto
    QLineEdit* m_txtLibreria = nullptr;
    QLineEdit* m_txtEmpresa  = nullptr;
    QLineEdit* m_txtRFC      = nullptr;
    QLineEdit* m_txtTel1     = nullptr;
    QLineEdit* m_txtTel2     = nullptr;

    // Tab Datos — logos
    QByteArray m_logoLibreria;
    QString    m_logoLibreriaMime;
    QLabel*    m_previewLibreria = nullptr;

    QByteArray m_logoEmpresa;
    QString    m_logoEmpresaMime;
    QLabel*    m_previewEmpresa  = nullptr;

    QTabWidget* m_tabs = nullptr;
};

} // namespace App
