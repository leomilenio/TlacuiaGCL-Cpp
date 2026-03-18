#pragma once
#include <QMainWindow>
#include <memory>
#include <cstdint>

class QTabWidget;
class QLabel;

namespace Calculadora {
class DatabaseManager;
class ProductoRepository;
class ConcesionRepository;
class EmisorRepository;
class DocumentoRepository;
class PriceCalculator;
}

namespace App {
class CalculatorWidget;
class HistoryWidget;
class ConcesionesWidget;
class EmisoresWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // concesionInicial: si > 0, la pestaña Concesiones selecciona esa concesion al abrir.
    explicit MainWindow(int64_t concesionInicial = 0, QWidget* parent = nullptr);
    ~MainWindow() override;

    // Navega a la pestaña Concesiones y selecciona la concesion indicada.
    void navigateToConcesion(int64_t concesionId);

public slots:
    // Actualiza el indicador de alertas en la statusbar.
    void refreshAlertaStatus();

private slots:
    void onCalculadoraRequested();
    void onAboutRequested();

private:
    void setupMenuBar();
    void setupCentralWidget(int64_t concesionInicial);
    void initDatabase();

    QTabWidget*          m_tabs             = nullptr;
    HistoryWidget*       m_historyTab       = nullptr;
    ConcesionesWidget*   m_concesionesTab   = nullptr;
    EmisoresWidget*      m_emisoresTab      = nullptr;
    QLabel*              m_alertasLabel     = nullptr;

    std::unique_ptr<Calculadora::DatabaseManager>      m_dbManager;
    std::unique_ptr<Calculadora::ProductoRepository>   m_productoRepo;
    std::unique_ptr<Calculadora::ConcesionRepository>  m_concesionRepo;
    std::unique_ptr<Calculadora::EmisorRepository>     m_emisorRepo;
    std::unique_ptr<Calculadora::DocumentoRepository>  m_documentoRepo;
    std::unique_ptr<Calculadora::PriceCalculator>      m_calculator;
};

} // namespace App
