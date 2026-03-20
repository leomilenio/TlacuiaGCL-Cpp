#include "app/MainWindow.h"
#include "app/SplashConcesionesWindow.h"
#include "core/AppConfig.h"
#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    // IMPORTANTE: OrganizationName y ApplicationName son identificadores internos
    // que determinan la ruta de la base de datos en disco. NO deben cambiar nunca,
    // aunque cambie el nombre comercial o el dominio del desarrollador.
    // macOS: ~/Library/Application Support/ARLE/TlacuiaGCL/data.db
    // Windows: %APPDATA%\ARLE\TlacuiaGCL\data.db
    app.setOrganizationName("ARLE");
    app.setOrganizationDomain("arlesoftware.com.mx");
    app.setApplicationName("TlacuiaGCL");
    app.setApplicationVersion("0.5.0");

#ifdef Q_OS_WIN
    // En Windows el icono no se hereda del ejecutable automaticamente
    app.setWindowIcon(QIcon(":/icons/icon.ico"));
#endif
    // En macOS el icono lo toma Qt directamente de NSApplication, que a su vez
    // lo lee del bundle (CFBundleIconFile = "icon" → Resources/icon.icns).
    // Llamar setWindowIcon con un QIcon cargado desde recursos puede producir
    // un icono nulo (los .icns modernos usan compresion que Qt no siempre decodifica)
    // y ese icono nulo sobreescribe el del bundle, causando el placeholder generico.

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
