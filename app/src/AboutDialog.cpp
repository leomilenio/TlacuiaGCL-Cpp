#include "app/AboutDialog.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QPixmap>
#include <QIcon>

namespace App {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Acerca de TlacuiaGCL");
    setFixedWidth(420);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Deteccion de modo oscuro: determina colores adaptativos y logo ARLE
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString arleLogoPath = darkMode ? ":/icons/arleLogo_White.png"
                                          : ":/icons/arleLogo_Black.png";
    const QString cSecundario = "color: palette(placeholder-text);";
    const QString cContacto   = darkMode ? "color: #EF9A9A; font-size: 12px;"
                                         : "color: #B71C1C; font-size: 12px;";

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(28, 24, 28, 24);
    main->setSpacing(0);

    // ---- Logos: [TlacuiaLogo] [ARLE Logo] ----
    auto* logosRow = new QHBoxLayout();
    logosRow->setSpacing(20);
    logosRow->setAlignment(Qt::AlignHCenter);

    auto* tlacuiaLogoLbl = new QLabel();
    QPixmap tlacuiaLogo(":/icons/TlacuiaLogo.png");
    if (!tlacuiaLogo.isNull())
        tlacuiaLogoLbl->setPixmap(tlacuiaLogo.scaledToHeight(110, Qt::SmoothTransformation));
    tlacuiaLogoLbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    auto* arleLogoLbl = new QLabel();
    QPixmap arleLogo(arleLogoPath);
    if (!arleLogo.isNull())
        arleLogoLbl->setPixmap(arleLogo.scaledToHeight(110, Qt::SmoothTransformation));
    arleLogoLbl->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    logosRow->addStretch();
    logosRow->addWidget(tlacuiaLogoLbl);
    logosRow->addWidget(arleLogoLbl);
    logosRow->addStretch();
    main->addLayout(logosRow);
    main->addSpacing(14);

    // ---- Título ----
    auto* titleLbl = new QLabel("Gestor de Concesiones<br><b>Tlacuia</b>");
    titleLbl->setAlignment(Qt::AlignHCenter);
    QFont titleFont = titleLbl->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleLbl->setFont(titleFont);
    main->addWidget(titleLbl);
    main->addSpacing(4);

    // ---- Versión ----
    auto* verLbl = new QLabel(QString("Versión %1").arg(QApplication::applicationVersion()));
    verLbl->setAlignment(Qt::AlignHCenter);
    verLbl->setStyleSheet(cSecundario + " font-size: 12px;");
    main->addWidget(verLbl);
    main->addSpacing(16);

    // ---- Separador ----
    auto addSep = [&]() {
        auto* s = new QFrame();
        s->setFrameShape(QFrame::HLine);
        main->addWidget(s);
    };
    addSep();
    main->addSpacing(12);

    // ---- Descripción ----
    auto* descLbl = new QLabel(
        "Sistema de gestión de concesiones de libros y papelería\n"
        "con cálculo de precios y tratamiento correcto de\n"
        "IVA mexicano conforme a LIVA Art. 4 y 5.");
    descLbl->setAlignment(Qt::AlignHCenter);
    descLbl->setWordWrap(true);
    descLbl->setStyleSheet("font-size: 12px;");
    main->addWidget(descLbl);
    main->addSpacing(16);

    addSep();
    main->addSpacing(12);

    // ---- Desarrollador ----
    auto* devHeaderLbl = new QLabel("Desarrollado por");
    devHeaderLbl->setAlignment(Qt::AlignHCenter);
    devHeaderLbl->setStyleSheet(cSecundario + " font-size: 11px;");
    main->addWidget(devHeaderLbl);
    main->addSpacing(3);

    auto* devLbl = new QLabel(
        "<b>Servicios de Ingeniería y Desarrollo\nde Software ARLE</b>");
    devLbl->setAlignment(Qt::AlignHCenter);
    devLbl->setStyleSheet("font-size: 13px;");
    main->addWidget(devLbl);
    main->addSpacing(6);

    auto* contactLbl = new QLabel("arlesoftware.com.mx");
    contactLbl->setAlignment(Qt::AlignHCenter);
    contactLbl->setStyleSheet(cContacto);
    main->addWidget(contactLbl);
    main->addSpacing(20);

    // ---- Licencia ----
    auto* licLbl = new QLabel("Desarrollado bajo licencia MIT");
    licLbl->setAlignment(Qt::AlignHCenter);
    licLbl->setStyleSheet(cSecundario + " font-size: 11px;");
    main->addWidget(licLbl);
    main->addSpacing(20);

    // ---- Botón cerrar ----
    auto* btnRow = new QHBoxLayout();
    auto* btnClose = new QPushButton("Cerrar");
    btnClose->setObjectName("primaryButton");
    btnClose->setFixedWidth(120);
    btnRow->addStretch();
    btnRow->addWidget(btnClose);
    btnRow->addStretch();
    main->addLayout(btnRow);

    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace App
