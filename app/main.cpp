#include "app/MainWindow.h"
#include "app/SplashConcesionesWindow.h"
#include "core/AppConfig.h"
#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("ARLE");
    app.setOrganizationDomain("arle.com.mx");
    app.setApplicationName("CalculadoraPapeleria");
    app.setApplicationVersion("0.4.0");

#ifdef Q_OS_WIN
    app.setWindowIcon(QIcon(":/icons/icon.ico"));
#else
    app.setWindowIcon(QIcon(":/icons/icon.icns"));
#endif

    // Cargar hoja de estilos Tlacuia
    QFile styleFile(":/styles/tlacuia.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QTextStream(&styleFile).readAll());
    }

    auto config = Calculadora::AppConfig::loadDefault();

    // Ventana de arranque — muestra el estado de concesiones antes de MainWindow.
    // Cuando el usuario presiona "Continuar" o "Ir a esta concesion", se abre
    // MainWindow y la splash se cierra.
    auto* splash = new App::SplashConcesionesWindow(config.dbPath);
    splash->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(splash, &App::SplashConcesionesWindow::continuar,
                     [splash](int64_t concesionId) {
        auto* mainWin = new App::MainWindow(concesionId);
        mainWin->setAttribute(Qt::WA_DeleteOnClose);
        mainWin->show();
        splash->close();
    });

    splash->show();
    return app.exec();
}
