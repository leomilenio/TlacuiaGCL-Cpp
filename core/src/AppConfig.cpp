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
    config.dbPath = dataDir + "/data.db";
    return config;
}

} // namespace Calculadora
