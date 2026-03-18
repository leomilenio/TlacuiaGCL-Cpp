#pragma once
#include <QString>

namespace Calculadora {

struct AppConfig {
    QString dbPath;

    // Resuelve la ruta segun la plataforma usando QStandardPaths:
    //   macOS:   ~/Library/Application Support/ARLE/TlacuiaGCL/data.db
    //   Windows: %APPDATA%\ARLE\TlacuiaGCL\data.db
    // ADVERTENCIA: La ruta depende de OrganizationName y ApplicationName en main.cpp.
    //              Cambiar esos valores cambia la ruta y la app pierde acceso a los datos.
    static AppConfig loadDefault();
};

} // namespace Calculadora
