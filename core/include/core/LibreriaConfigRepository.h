#pragma once
// Configuracion e identidad visual de la libreria.
// Se almacena en la tabla app_config (fila unica, id=1).
// Usada para personalizar los PDF generados por la aplicacion.
#include <QString>
#include <QByteArray>
#include <QList>
#include <QSqlQuery>

namespace Calculadora {

// Un numero de telefono con su tipo de contacto (WhatsApp, Local, Celular, etc.)
struct TelefonoConfig {
    QString tipo;    // Etiqueta del canal: "WhatsApp", "Local", "Celular", "Oficina", "Fax", "Otro"
    QString numero;  // Numero de telefono (ej: "55 1234 5678")
};

struct LibreriaConfig {
    QString    libreriaNombre;       // Nombre de la libreria (aparece en encabezados)
    QString    empresaNombre;        // Razon social / nombre fiscal de la empresa
    QString    rfc;                  // RFC (13 caracteres persona fisica, 12 moral)
    QString    email;                // Correo electronico de contacto
    QString    regimenFiscalCode;    // Codigo SAT (ej: "601")
    QString    regimenFiscalDesc;    // Descripcion SAT (ej: "General de Ley Personas Morales")
    QString    contactoNombre;       // Nombre del contacto (ej: "Coordinacion de Libreria Somos Voces")
    // Dirección física de la librería
    QString    dirCalle;
    QString    dirNumExterior;
    QString    dirNumInterior;       // opcional
    QString    dirCodigoPostal;      // 5 dígitos
    QString    dirColonia;
    QString    dirMunicipio;         // municipio o alcaldía
    QString    dirEstado;
    QList<TelefonoConfig> telefonos; // Hasta 4 telefonos con tipo de contacto
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
