#pragma once
#include <QDialog>
#include <QByteArray>
#include <QList>
#include "core/LibreriaConfigRepository.h"
#include "core/DatabaseManager.h"

class QTabWidget;
class QLineEdit;
class QLabel;
class QComboBox;
class QVBoxLayout;
class QPushButton;

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
    void buildTabDocumentos(QWidget* tab);
    void buildTabBaseDatos(QWidget* tab);
    void loadFromRepo();
    void updatePreview(QLabel* preview, const QByteArray& data);
    // Agrega una fila de telefono dinamica al contenedor de telefonos.
    // tipo/numero: valores a precargar (vacios = nueva fila en blanco).
    void addTelRow(const QString& tipo = {}, const QString& numero = {});

    Calculadora::LibreriaConfigRepository& m_configRepo;
    Calculadora::DatabaseManager&          m_dbManager;

    // Tab Datos — informacion general
    QLineEdit* m_txtLibreria  = nullptr;
    QLineEdit* m_txtEmpresa   = nullptr;
    QLineEdit* m_txtRFC       = nullptr;
    QLineEdit* m_txtEmail     = nullptr;
    QLineEdit* m_txtContacto  = nullptr;
    QComboBox* m_cmbRegimen   = nullptr;

    // Tab Datos — dirección
    QLineEdit* m_txtDirCalle     = nullptr;
    QLineEdit* m_txtDirNumExt    = nullptr;
    QLineEdit* m_txtDirNumInt    = nullptr;
    QLineEdit* m_txtDirCP        = nullptr;
    QLineEdit* m_txtDirColonia   = nullptr;
    QLineEdit* m_txtDirMunicipio = nullptr;
    QComboBox* m_cmbDirEstado    = nullptr;

    // Tab Datos — telefonos dinamicos (max 4)
    static constexpr int MAX_TELEFONOS = 4;
    struct TelRow {
        QWidget*   widget;   // contenedor de la fila (para deleteLater)
        QComboBox* tipo;
        QLineEdit* numero;
    };
    QList<TelRow>  m_telRows;
    QWidget*       m_telContainer = nullptr;
    QVBoxLayout*   m_telLayout    = nullptr;
    QPushButton*   m_btnAddTel    = nullptr;

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
