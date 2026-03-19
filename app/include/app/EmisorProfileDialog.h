#pragma once
#include <QDialog>
#include "core/EmisorRepository.h"
#include "core/ConcesionRepository.h"
#include "core/ProductoRepository.h"

class QTabWidget;

namespace App {

class EmisorProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit EmisorProfileDialog(
        const Calculadora::EmisorRecord&   emisor,
        Calculadora::ConcesionRepository&  concesionRepo,
        Calculadora::ProductoRepository&   productoRepo,
        QWidget* parent = nullptr);

signals:
    void navegarAConcesion(int64_t concesionId);

private:
    void setupUi();
    void buildResumenHeader(QLayout* parent);
    void buildTabActivas();
    void buildTabHistorial();
    void buildTabReporte();
    void onExportarPdfClicked();

    Calculadora::EmisorRecord              m_emisor;
    Calculadora::ConcesionRepository&      m_concesionRepo;
    Calculadora::ProductoRepository&       m_productoRepo;

    QList<Calculadora::ConcesionRecord>    m_activas;
    QList<Calculadora::ConcesionRecord>    m_finalizadas;
    QList<Calculadora::CorteResult>        m_activasCortes;
    QList<Calculadora::CorteResult>        m_cortes;        // parallel to m_finalizadas

    QTabWidget* m_tabs = nullptr;
};

} // namespace App
