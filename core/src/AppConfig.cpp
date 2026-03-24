#include "core/AppConfig.h"
#include <QStandardPaths>
#include <QDir>

namespace Calculadora {

AppConfig AppConfig::loadDefault() {
    AppConfig config;
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(dataDir);
    }
    // QDir::filePath() une la ruta con el separador nativo del SO,
    // evitando asumir que Qt normalizará "/" en Windows.
    config.dbPath = QDir(dataDir).filePath("data.db");
    return config;
}

} // namespace Calculadora
