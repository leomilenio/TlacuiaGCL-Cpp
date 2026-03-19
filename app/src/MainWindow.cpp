#include "app/MainWindow.h"
#include "app/AboutDialog.h"
#include "app/UpdateDialog.h"
#include "app/CalculatorWidget.h"
#include "app/HistoryWidget.h"
#include "app/ConcesionesWidget.h"
#include "app/EmisoresWidget.h"
#include "core/AppConfig.h"
#include "core/DatabaseManager.h"
#include "core/ProductoRepository.h"
#include "core/ConcesionRepository.h"
#include "core/EmisorRepository.h"
#include "core/DocumentoRepository.h"
#include "core/PriceCalculator.h"
#include <QTabWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>

namespace App {

MainWindow::MainWindow(int64_t concesionInicial, QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Gestor de Concesiones Tlacuia");
    setMinimumSize(700, 560);
    resize(860, 680);

    initDatabase();
    setupMenuBar();
    setupCentralWidget(concesionInicial);
    refreshAlertaStatus();
}

MainWindow::~MainWindow() = default;

void MainWindow::initDatabase() {
    auto config = Calculadora::AppConfig::loadDefault();
    m_dbManager      = std::make_unique<Calculadora::DatabaseManager>(config.dbPath);
    m_productoRepo   = std::make_unique<Calculadora::ProductoRepository>(*m_dbManager);
    m_concesionRepo  = std::make_unique<Calculadora::ConcesionRepository>(*m_dbManager);
    m_emisorRepo     = std::make_unique<Calculadora::EmisorRepository>(*m_dbManager);
    m_documentoRepo  = std::make_unique<Calculadora::DocumentoRepository>(*m_dbManager);
    m_calculator     = std::make_unique<Calculadora::PriceCalculator>();

    if (!m_dbManager->initialize()) {
        QMessageBox::critical(this, "Error de base de datos",
            "No se pudo inicializar la base de datos.\n"
            "La aplicacion puede no guardar datos correctamente.");
    }
}

void MainWindow::setupMenuBar() {
    QMenu* toolsMenu = menuBar()->addMenu("&Herramientas");
    toolsMenu->addAction("&Calculadora de Precios", this, &MainWindow::onCalculadoraRequested);

    QMenu* helpMenu = menuBar()->addMenu("&Ayuda");
    helpMenu->addAction("&Buscar actualizaciones...", this, &MainWindow::onBuscarActualizacionesRequested);
    helpMenu->addSeparator();
    helpMenu->addAction("&Acerca de", this, &MainWindow::onAboutRequested);
}

void MainWindow::setupCentralWidget(int64_t concesionInicial) {
    m_tabs = new QTabWidget(this);

    m_concesionesTab = new ConcesionesWidget(*m_concesionRepo, *m_emisorRepo,
                                             *m_productoRepo, *m_documentoRepo,
                                             *m_calculator, m_tabs);
    m_emisoresTab    = new EmisoresWidget(*m_emisorRepo, *m_concesionRepo, m_tabs);
    m_historyTab     = new HistoryWidget(*m_productoRepo, m_tabs);

    m_tabs->addTab(m_concesionesTab, "Concesiones");
    m_tabs->addTab(m_emisoresTab,    "Distribuidores");
    m_tabs->addTab(m_historyTab,     "Historial");

    connect(m_concesionesTab, &ConcesionesWidget::emisorCreado,
            m_emisoresTab,    &EmisoresWidget::refresh);
    connect(m_concesionesTab, &ConcesionesWidget::productoAgregado,
            m_historyTab,     &HistoryWidget::refresh);

    // Refrescar indicador de alertas y conteo de concesiones en distribuidores
    connect(m_concesionesTab, &ConcesionesWidget::concesionesModificadas,
            this,             &MainWindow::refreshAlertaStatus);
    connect(m_concesionesTab, &ConcesionesWidget::concesionesModificadas,
            m_emisoresTab,    &EmisoresWidget::refresh);

    setCentralWidget(m_tabs);

    // Indicador de alertas en la statusbar (clickeable)
    m_alertasLabel = new QLabel();
    m_alertasLabel->setCursor(Qt::PointingHandCursor);
    m_alertasLabel->setToolTip("Haz clic para ir a Concesiones");
    connect(m_alertasLabel, &QLabel::linkActivated, this, [this](const QString&) {
        m_tabs->setCurrentWidget(m_concesionesTab);
    });
    statusBar()->addPermanentWidget(m_alertasLabel);

    // Navegar a concesion inicial si se indicó una
    if (concesionInicial > 0) {
        navigateToConcesion(concesionInicial);
    }
}

void MainWindow::navigateToConcesion(int64_t concesionId) {
    m_tabs->setCurrentWidget(m_concesionesTab);
    m_concesionesTab->selectById(concesionId);
}

void MainWindow::refreshAlertaStatus() {
    if (!m_concesionRepo) return;

    int vencidas   = 0;
    int porVencer  = 0;

    const auto activas = m_concesionRepo->findActivas();
    for (const auto& c : activas) {
        switch (c.status()) {
        case Calculadora::ConcesionStatus::Vencida:     ++vencidas;  break;
        case Calculadora::ConcesionStatus::VencePronto: ++porVencer; break;
        default: break;
        }
    }

    if (vencidas > 0 || porVencer > 0) {
        QStringList partes;
        if (vencidas > 0)
            partes << QString("%1 vencida%2").arg(vencidas).arg(vencidas > 1 ? "s" : "");
        if (porVencer > 0)
            partes << QString("%1 proxima%2 a vencer").arg(porVencer).arg(porVencer > 1 ? "s" : "");
        m_alertasLabel->setText(
            QString("<a href='#' style='color:#E65100; text-decoration:none;'>"
                    "&#9888; %1</a>").arg(partes.join(" · ")));
    } else {
        m_alertasLabel->setText(
            "<a href='#' style='color:#2E7D32; text-decoration:none;'>"
            "&#10003; Todas las concesiones al dia</a>");
    }
}

void MainWindow::onCalculadoraRequested() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Calculadora de Precios");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setMinimumSize(560, 500);

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* calc = new CalculatorWidget(*m_calculator, *m_productoRepo,
                                      *m_concesionRepo, dlg);
    calc->disableSaving();
    layout->addWidget(calc);
    dlg->show();
}

void MainWindow::onBuscarActualizacionesRequested() {
    auto* dlg = new UpdateDialog(this);
    dlg->exec();
}

void MainWindow::onAboutRequested() {
    AboutDialog dlg(this);
    dlg.exec();
}

} // namespace App
