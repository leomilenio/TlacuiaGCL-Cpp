#pragma once
#include <string>

namespace Calculadora {

enum class Escenario {
    ProductoPropio,
    Concesion
};

// Clasificacion del articulo.
// Permite filtrar y reportar por categoria en sprint futuro.
// Libro requiere ISBN obligatorio (para integracion con TlacuiaGCL Productos.isbn).
enum class TipoProducto {
    Libro,
    Papeleria
};

// CalculationResult: tipos POD sin dependencias de Qt.
// Seguro para cruzar el limite del C API.
struct CalculationResult {
    double precioFinal     = 0.0;
    double costo           = 0.0;
    double comision        = 0.0;
    double ivaTrasladado   = 0.0;  // IVA que cobro al cliente (16% papeleria / 0% libros)
    double ivaAcreditable  = 0.0;  // IVA pagado al proveedor con CFDI (recuperable)
    double ivaNetoPagar    = 0.0;  // ivaTrasladado - ivaAcreditable = lo que pago a SAT
                                   // Negativo = saldo a favor (libros con CFDI)
    double ivaAbsorbido    = 0.0;  // IVA pagado al proveedor sin CFDI (no recuperable)
                                   // Solo nonzero en Concesion Sin CFDI
    Escenario    escenario    = Escenario::ProductoPropio;
    TipoProducto tipoProducto = TipoProducto::Papeleria;
    bool  tieneCFDI        = false;
    bool  isValid          = false;
    std::string errorMessage;
};

} // namespace Calculadora
