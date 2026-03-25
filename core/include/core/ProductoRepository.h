#pragma once
// ProductoRecord mapea a las tablas de TlacuiaGCL (para futura integracion):
//   precio_final  <-> Productos.pvp_unitario  (precio de venta al publico)
//   costo         <-> Productos.precio_neto   (precio neto del proveedor)
//   isbn          <-> Productos.isbn
//   nombre_producto <-> Productos.descripcion
//   concesion_id  <-> Productos.concesion_id
#include "CalculationScenario.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <optional>
#include <cstdint>

namespace Calculadora {

struct ProductoRecord {
    int64_t     id               = 0;
    QString     fecha;
    QString     nombreProducto;          // Requerido al guardar
    TipoProducto tipoProducto   = TipoProducto::Papeleria;
    QString     isbn;                    // Requerido si tipoProducto == Libro
    double      precioFinal     = 0.0;  // pvp_unitario en TlacuiaGCL
    double    costo            = 0.0;  // precio_neto en TlacuiaGCL
    double    comision         = 0.0;
    double    ivaTrasladado    = 0.0;
    double    ivaAcreditable   = 0.0;
    double    ivaNetoPagar     = 0.0;
    Escenario escenario        = Escenario::ProductoPropio;
    bool      tieneCFDI        = false;
    QString   nombreProveedor;
    QString   nombreVendedor;
    std::optional<int64_t> concesionId;  // FK a concesiones (TlacuiaGCL compatible)
    int       cantidadRecibida = 1;      // piezas recibidas en la concesion
    int       cantidadVendida  = 0;      // piezas declaradas vendidas al hacer el corte
    // cantidadDevuelta es calculado: cantidadRecibida - cantidadVendida
};

struct CorteResult {
    int64_t concesionId              = 0;
    int     cantidadRegistros        = 0;
    double  totalPrecioFinal         = 0.0;
    double  totalCosto               = 0.0;   // Lo que se paga al proveedor
    double  totalComision            = 0.0;   // Lo que se queda la libreria
    double  totalIvaTrasladado       = 0.0;
    double  totalIvaAcreditable      = 0.0;
    double  totalIvaNetoPagar        = 0.0;
    // Campos de inventario (Sprint 2)
    int     totalUnidadesRecibidas   = 0;
    int     totalUnidadesVendidas    = 0;
    int     totalUnidadesDevueltas   = 0;
    double  totalPagoAlDistribuidor  = 0.0;  // SUM(costo * cantidad_vendida)
    double  totalDevolucion          = 0.0;  // SUM(costo * cantidad_devuelta)
    double  gananciaEstimada         = 0.0;  // SUM(comision * cantidad_vendida)
    bool    isValid                  = false;
};

// Resumen consolidado de todos los cortes finalizados de un emisor.
struct EmisorCorteResumen {
    int    totalConcesiones       = 0;
    int    totalUnidadesRecibidas = 0;
    int    totalUnidadesVendidas  = 0;
    int    totalUnidadesDevueltas = 0;
    double totalIngresado         = 0.0; // SUM(precio_final * cantidad_vendida)
    double totalAlDistribuidor    = 0.0; // SUM(costo * cantidad_vendida)
    double totalComisiones        = 0.0; // totalIngresado - totalAlDistribuidor
    double totalDevolucion        = 0.0; // SUM(costo * cantidad_devuelta)
    double rotacionPromedio       = 0.0; // vendidas / recibidas (%)
    double tasaDevolucionPromedio = 0.0; // devueltas / recibidas (%)
};

class DatabaseManager;

class ProductoRepository {
public:
    explicit ProductoRepository(DatabaseManager& dbManager);

    // Guarda un registro y retorna el id generado, o -1 si falla.
    [[nodiscard]] int64_t save(const ProductoRecord& record);

    // Actualiza un producto existente (todos los campos financieros + datos).
    bool update(const ProductoRecord& record);

    // Todos los registros ordenados por fecha DESC.
    [[nodiscard]] QList<ProductoRecord> findAll() const;

    // Paginacion simple.
    [[nodiscard]] QList<ProductoRecord> findPage(int limit, int offset) const;

    bool remove(int64_t id);

    // Todos los productos de una concesion, ordenados por id.
    [[nodiscard]] QList<ProductoRecord> findByConcesion(int64_t concesionId) const;

    // Calcula el corte de venta de una concesion (suma de sus productos guardados).
    [[nodiscard]] CorteResult calcularCorte(int64_t concesionId) const;

    // Actualiza la cantidad vendida de un producto (para CorteDialog).
    bool updateCantidadVendida(int64_t productoId, int cantidadVendida);

    // Resumen financiero consolidado de todos los cortes finalizados de un emisor.
    [[nodiscard]] EmisorCorteResumen calcularResumenEmisor(int64_t emisorId) const;

private:
    [[nodiscard]] ProductoRecord mapRow(const QSqlQuery& query) const;

    DatabaseManager& m_db;
};

} // namespace Calculadora
