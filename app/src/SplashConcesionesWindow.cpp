#include "app/SplashConcesionesWindow.h"
#include "core/AppConfig.h"
#include "core/DatabaseManager.h"
#include "core/ConcesionRepository.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDate>

namespace App {

// ---------------------------------------------------------------------------
// Helpers de estilo y texto
// ---------------------------------------------------------------------------

static QString sectionStyle(Calculadora::ConcesionStatus status) {
    switch (status) {
    case Calculadora::ConcesionStatus::Vencida:
        return "QGroupBox { border: 2px solid #F44336; border-radius: 4px; margin-top: 6px; }"
               "QGroupBox::title { color: #F44336; font-weight: bold; }";
    case Calculadora::ConcesionStatus::VencePronto:
        return "QGroupBox { border: 2px solid #FFC107; border-radius: 4px; margin-top: 6px; }"
               "QGroupBox::title { color: #E65100; font-weight: bold; }";
    default:
        return "QGroupBox { border: 2px solid #4CAF50; border-radius: 4px; margin-top: 6px; }"
               "QGroupBox::title { color: #2E7D32; font-weight: bold; }";
    }
}

static QString diasLabel(const Calculadora::ConcesionRecord& c) {
    int dias = c.diasRestantes();
    if (dias < 0)
        return QString("Vencio hace %1 dia%2").arg(-dias).arg(-dias == 1 ? "" : "s");
    if (dias == 0)
        return "Vence hoy";
    return QString("Vence en %1 dia%2").arg(dias).arg(dias == 1 ? "" : "s");
}

// ---------------------------------------------------------------------------
// SplashConcesionesWindow
// ---------------------------------------------------------------------------

SplashConcesionesWindow::SplashConcesionesWindow(const QString& dbPath, QWidget* parent)
    : QWidget(parent)
    , m_dbPath(dbPath)
{
    m_dbManager     = std::make_unique<Calculadora::DatabaseManager>(m_dbPath);
    m_dbManager->initialize();
    m_concesionRepo = std::make_unique<Calculadora::ConcesionRepository>(*m_dbManager);

    setupUi();
    setWindowTitle("Gestor de Concesiones Tlacuia — Estado de concesiones");
    setMinimumSize(560, 400);
    resize(620, 520);
}

SplashConcesionesWindow::~SplashConcesionesWindow() = default;

void SplashConcesionesWindow::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 14, 16, 14);
    outerLayout->setSpacing(10);

    // Encabezado
    auto* header = new QLabel("<b style='font-size:15pt;'>Gestor de Concesiones Tlacuia</b>");
    header->setAlignment(Qt::AlignCenter);
    outerLayout->addWidget(header);

    auto* subHeader = new QLabel(
        QString("Estado de concesiones al %1")
            .arg(QDate::currentDate().toString("dd/MM/yyyy")));
    subHeader->setAlignment(Qt::AlignCenter);
    subHeader->setStyleSheet("color: gray;");
    outerLayout->addWidget(subHeader);

    auto* sep = new QFrame(); sep->setFrameShape(QFrame::HLine);
    outerLayout->addWidget(sep);

    // Scroll area para las tarjetas
    auto* scrollArea   = new QScrollArea();
    auto* scrollWidget = new QWidget();
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(4, 4, 4, 4);
    scrollLayout->setSpacing(8);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    const auto activas = m_concesionRepo->findActivas();

    if (activas.isEmpty()) {
        auto* sinCons = new QLabel("No hay concesiones activas en este momento.");
        sinCons->setAlignment(Qt::AlignCenter);
        sinCons->setStyleSheet("color: gray; font-style: italic; padding: 20px;");
        scrollLayout->addWidget(sinCons);
    } else {
        // Agrupar por status
        QList<Calculadora::ConcesionRecord> vencidas, prontas, vigentes;
        for (const auto& c : activas) {
            switch (c.status()) {
            case Calculadora::ConcesionStatus::Vencida:     vencidas.append(c);  break;
            case Calculadora::ConcesionStatus::VencePronto: prontas.append(c);   break;
            default:                                         vigentes.append(c);  break;
            }
        }

        // Crea un grupo con sus tarjetas
        auto buildSection = [&](const QString& titulo,
                                const QList<Calculadora::ConcesionRecord>& lista,
                                Calculadora::ConcesionStatus status) {
            if (lista.isEmpty()) return;

            auto* grp    = new QGroupBox(QString("%1  (%2)").arg(titulo).arg(lista.size()));
            grp->setStyleSheet(sectionStyle(status));
            auto* grpLay = new QVBoxLayout(grp);
            grpLay->setSpacing(4);

            for (const auto& c : lista) {
                auto* row    = new QWidget();
                auto* rowLay = new QHBoxLayout(row);
                rowLay->setContentsMargins(4, 2, 4, 2);

                QString emisor = c.emisorNombre.isEmpty() ? "(Sin distribuidor)" : c.emisorNombre;
                QString folio  = c.folio.isEmpty()        ? "(Sin folio)"        : c.folio;

                auto* lblInfo = new QLabel(
                    QString("<b>%1</b> — %2  <span style='color:gray;'>|  %3</span>")
                        .arg(emisor.toHtmlEscaped(), folio.toHtmlEscaped(), diasLabel(c)));
                lblInfo->setTextFormat(Qt::RichText);
                rowLay->addWidget(lblInfo, 1);

                auto* btnIr = new QPushButton("→");
                btnIr->setFixedWidth(36);
                btnIr->setToolTip("Ir a esta concesion");
                int64_t cid = c.id;
                connect(btnIr, &QPushButton::clicked, this, [this, cid]() {
                    emit continuar(cid);
                });
                rowLay->addWidget(btnIr);

                grpLay->addWidget(row);
            }

            scrollLayout->addWidget(grp);
        };

        buildSection("VENCIDAS",     vencidas, Calculadora::ConcesionStatus::Vencida);
        buildSection("VENCE PRONTO", prontas,  Calculadora::ConcesionStatus::VencePronto);
        buildSection("VIGENTES",     vigentes, Calculadora::ConcesionStatus::Valido);
    }

    scrollLayout->addStretch();
    outerLayout->addWidget(scrollArea, 1);

    // Separador + botón Continuar
    auto* sep2 = new QFrame(); sep2->setFrameShape(QFrame::HLine);
    outerLayout->addWidget(sep2);

    auto* btnRow   = new QHBoxLayout();
    btnRow->addStretch();
    auto* btnCont  = new QPushButton("Continuar al programa  →");
    btnCont->setObjectName("primaryButton");
    btnCont->setDefault(true);
    btnCont->setMinimumWidth(200);
    connect(btnCont, &QPushButton::clicked, this, [this]() {
        emit continuar(0);
    });
    btnRow->addWidget(btnCont);
    outerLayout->addLayout(btnRow);
}

} // namespace App
