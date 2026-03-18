#pragma once
#include <QDialog>
#include <QStringList>
#include "core/ConcesionRepository.h"
#include "core/EmisorRepository.h"

class QComboBox;
class QLineEdit;
class QDateEdit;
class QRadioButton;
class QSpinBox;
class QDoubleSpinBox;
class QGroupBox;
class QListWidget;

namespace App {

class NuevaConcesionDialog : public QDialog {
    Q_OBJECT
public:
    // Para crear nueva concesion
    explicit NuevaConcesionDialog(Calculadora::EmisorRepository& emisorRepo,
                                  QWidget* parent = nullptr);
    // Para editar concesion existente
    explicit NuevaConcesionDialog(const Calculadora::ConcesionRecord& record,
                                  Calculadora::EmisorRepository& emisorRepo,
                                  QWidget* parent = nullptr);

    [[nodiscard]] Calculadora::ConcesionRecord result() const;

    // Para crear un emisor nuevo cuando isNuevoEmisor() es true
    [[nodiscard]] bool    isNuevoEmisor()      const;
    [[nodiscard]] QString nuevoEmisorNombre()  const;
    [[nodiscard]] QString nuevoEmisorVendedor() const;
    [[nodiscard]] QString nuevoEmisorTelefono() const;
    [[nodiscard]] QString nuevoEmisorEmail()   const;

    // Archivos seleccionados para adjuntar (rutas absolutas)
    [[nodiscard]] QStringList adjuntosSeleccionados() const;

private slots:
    void onEmisorSelectionChanged(int index);
    void onFechaToggled();
    void onComisionToggled();
    void onSeleccionarAdjuntosClicked();
    void onQuitarAdjuntoClicked();
    void onAcceptClicked();

private:
    void setupUi();
    void loadEmisores();
    void populateFrom(const Calculadora::ConcesionRecord& record);

    Calculadora::EmisorRepository& m_emisorRepo;
    bool    m_editMode = false;
    int64_t m_editId   = 0;

    // Emisor
    QComboBox*  m_cmbEmisor         = nullptr;
    QGroupBox*  m_grpNuevoEmisor    = nullptr;
    QLineEdit*  m_txtNombreEmisor   = nullptr;
    QLineEdit*  m_txtNombreVendedor = nullptr;
    QLineEdit*  m_txtTelefono       = nullptr;
    QLineEdit*  m_txtEmail          = nullptr;

    // Concesion
    QComboBox*    m_cmbTipo       = nullptr;
    QLineEdit*    m_txtFolio      = nullptr;
    QDateEdit*    m_dateFechaRec  = nullptr;
    QRadioButton* m_rdDias        = nullptr;
    QRadioButton* m_rdFechaExacta = nullptr;
    QSpinBox*     m_spnDias       = nullptr;
    QDateEdit*    m_dateFechaVenc = nullptr;
    QLineEdit*    m_txtNotas      = nullptr;

    // Comisión
    QRadioButton*    m_rdComisionEstandar  = nullptr;
    QRadioButton*    m_rdComisionCustom    = nullptr;
    QDoubleSpinBox*  m_spnComision         = nullptr;

    // Documentos adjuntos
    QListWidget*  m_listaAdjuntos  = nullptr;
};

} // namespace App
