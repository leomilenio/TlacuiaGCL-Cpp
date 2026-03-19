#pragma once
#include <QWidget>
#include "core/ConcesionRepository.h"
#include "core/EmisorRepository.h"
#include "core/ProductoRepository.h"
#include "core/DocumentoRepository.h"
#include "core/PriceCalculator.h"

class QListWidget;
class QPushButton;
class QLabel;
class QGroupBox;
class QTableWidget;
class QSplitter;
class QLineEdit;
class QComboBox;

namespace App {

class ConcesionesWidget : public QWidget {
    Q_OBJECT
public:
    explicit ConcesionesWidget(Calculadora::ConcesionRepository& concesionRepo,
                               Calculadora::EmisorRepository&    emisorRepo,
                               Calculadora::ProductoRepository&  productoRepo,
                               Calculadora::DocumentoRepository& documentoRepo,
                               Calculadora::PriceCalculator&     calculator,
                               QWidget* parent = nullptr);
    void refresh();
    void selectById(int64_t concesionId);

signals:
    void emisorCreado();
    void productoAgregado();
    void concesionesModificadas();

private slots:
    void onSelectionChanged();
    void onNuevaClicked();
    void onEditarClicked();
    void onFinalizarClicked();
    void onEliminarClicked();
    void onAgregarProductoClicked();
    void onVerCorteClicked();
    void onEditarProducto(int64_t productoId);
    void onEliminarProducto(int64_t productoId);
    void onVerAlertasClicked();
    void onAdjuntarDocClicked();
    void onAbrirDocClicked();

private:
    void setupUi();
    void setupConnections();
    void updateDetailPanel(const Calculadora::ConcesionRecord& record);
    void clearDetailPanel();

    Calculadora::ConcesionRepository& m_concesionRepo;
    Calculadora::EmisorRepository&    m_emisorRepo;
    Calculadora::ProductoRepository&  m_productoRepo;
    Calculadora::DocumentoRepository& m_documentoRepo;
    Calculadora::PriceCalculator&     m_calculator;

    // Header
    QLabel*       m_lblFecha       = nullptr;
    QPushButton*  m_btnVerAlertas  = nullptr;

    // Panel izquierdo — lista
    QListWidget*  m_listWidget     = nullptr;
    QPushButton*  m_btnNueva       = nullptr;

    // Panel derecho — detalle
    QGroupBox*  m_detailGroup      = nullptr;
    QLabel*     m_lblEmisor        = nullptr;
    QLabel*     m_lblVendedor      = nullptr;
    QLabel*     m_lblFacturacion   = nullptr;
    QLabel*     m_lblFolio         = nullptr;
    QLabel*     m_lblTipo          = nullptr;
    QLabel*     m_lblRecepcion     = nullptr;
    QLabel*     m_lblVencimiento   = nullptr;
    QLabel*     m_lblStatus        = nullptr;
    QLabel*     m_lblDias          = nullptr;
    QLabel*     m_lblComision      = nullptr;
    QLabel*     m_lblNotas         = nullptr;

    // Panel derecho — productos vinculados
    QTableWidget*       m_productosTable      = nullptr;
    QPushButton*        m_btnAgregarProducto  = nullptr;
    QPushButton*        m_btnVerCorte         = nullptr;

    // Panel documentos adjuntos
    QListWidget*  m_documentosView    = nullptr;
    QPushButton*  m_btnAdjuntarDoc    = nullptr;
    QPushButton*  m_btnAbrirDoc       = nullptr;

    // Buscador
    QLineEdit*  m_txtBuscar       = nullptr;
    QComboBox*  m_cmbFiltroStatus = nullptr;

    // Acciones de concesión
    QPushButton*  m_btnEditar    = nullptr;
    QPushButton*  m_btnEliminar  = nullptr;
    QPushButton*  m_btnFinalizar = nullptr;
};

} // namespace App
