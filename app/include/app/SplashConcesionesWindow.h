#pragma once
#include <QWidget>
#include <QString>
#include <memory>

namespace Calculadora {
class DatabaseManager;
class ConcesionRepository;
}

namespace App {

// Ventana de arranque que muestra el estado de todas las concesiones activas
// antes de abrir MainWindow. Replica la SplashWindow de TlacuiaGCL (Python/PyQt5).
//
// Flujo:
//   1. Se muestra antes de que exista MainWindow.
//   2. El usuario revisa el estado de sus concesiones.
//   3. Al presionar "Continuar" o "Ir a esta concesion", emite continuar(id).
//      id = 0 si no se selecciono ninguna concesion especifica.
class SplashConcesionesWindow : public QWidget {
    Q_OBJECT
public:
    // dbPath: ruta a la base de datos SQLite (misma que usara MainWindow).
    explicit SplashConcesionesWindow(const QString& dbPath, QWidget* parent = nullptr);
    ~SplashConcesionesWindow() override;

signals:
    void continuar(int64_t concesionSeleccionadaId);

private:
    void setupUi();

    QString m_dbPath;
    std::unique_ptr<Calculadora::DatabaseManager>   m_dbManager;
    std::unique_ptr<Calculadora::ConcesionRepository> m_concesionRepo;
};

} // namespace App
