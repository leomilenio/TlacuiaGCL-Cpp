#pragma once
// Configuracion e identidad visual de la libreria.
// Se almacena en la tabla app_config (fila unica, id=1).
// Usada para personalizar los PDF generados por la aplicacion.
#include <QString>
#include <QByteArray>
#include <QSqlQuery>

namespace Calculadora {

struct LibreriaConfig {
    QString    libreriaNombre;       // Nombre de la libreria (aparece en encabezados)
    QString    empresaNombre;        // Razon social / nombre fiscal de la empresa
    QString    rfc;                  // RFC (13 caracteres persona fisica, 12 moral)
    QString    tel1;
    QString    tel2;
    QByteArray logoLibreria;         // Bytes crudos de la imagen (PNG/JPEG)
    QString    logoLibreriaMime;     // "image/png" | "image/jpeg"
    QByteArray logoEmpresa;
    QString    logoEmpresaMime;
};

class DatabaseManager;

class LibreriaConfigRepository {
public:
    explicit LibreriaConfigRepository(DatabaseManager& dbManager);

    // Carga la configuracion almacenada. Devuelve un struct con valores vacios
    // si aun no se ha guardado ninguna configuracion.
    [[nodiscard]] LibreriaConfig load() const;

    // Persiste la configuracion. Usa INSERT OR REPLACE con id=1.
    bool save(const LibreriaConfig& cfg);

private:
    DatabaseManager& m_db;
};

} // namespace Calculadora
