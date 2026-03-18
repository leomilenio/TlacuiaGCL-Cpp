#pragma once
#include <QString>

namespace Calculadora {

struct AppConfig {
    QString dbPath;

    // Resuelve la ruta segun la plataforma usando QStandardPaths:
    //   macOS:   ~/Library/Application Support/SomosVoces/CalculadoraPapeleria/data.db
    //   Windows: %APPDATA%\SomosVoces\CalculadoraPapeleria\data.db
    static AppConfig loadDefault();
};

} // namespace Calculadora
