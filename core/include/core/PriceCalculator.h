#pragma once
#include "CalculationScenario.h"

namespace Calculadora {

struct PriceRatios {
    static constexpr double COSTO    = 0.54;
    static constexpr double COMISION = 0.30;
    static constexpr double IVA      = 0.16;
};

class PriceCalculator {
public:
    PriceCalculator() = default;

    // Escenario 1: Producto propio.
    // Entrada: el precio final que el usuario quiere cobrar.
    // conCFDI=true  -> el usuario tiene CFDI de compra al proveedor:
    //   ivaAcreditable = costo (54%) * 16%  [LIVA Art. 4 y 5]
    //   ivaNetoPagar   = ivaTrasladado - ivaAcreditable
    // conCFDI=false -> sin CFDI o producto elaborado por el propio usuario:
    //   ivaAcreditable = 0
    //   ivaNetoPagar   = ivaTrasladado completo
    [[nodiscard]]
    CalculationResult calcularProductoPropio(double precioFinal, bool conCFDI = false) const;

    // Escenario 2a: Concesion con CFDI.
    // Papeleria (modelo margen, costo = 54% del precio final):
    //   precioFinal    = precioNeto / 0.54
    //   comision       = precioFinal * 30%
    //   ivaTrasladado  = precioFinal * 16%
    //   ivaAcreditable = precioNeto * 16%   [recuperable con CFDI]
    //   ivaNetoPagar   = ivaTrasladado - ivaAcreditable
    // Libro (tasa 0%, LIVA Art. 2-A fr. IV):
    //   precioFinal    = precioNeto * (1 + comision%)  [sin IVA al cliente]
    //   ivaAcreditable = precioNeto * 16%  → genera saldo a favor (ivaNetoPagar < 0)
    [[nodiscard]]
    CalculationResult calcularConcesionConCFDI(double precioNetoProveedor,
                                               double comisionPct = 30.0,
                                               TipoProducto tipo  = TipoProducto::Papeleria) const;

    // Escenario 2b: Concesion sin CFDI.
    // Papeleria (modelo margen, IVA pagado al proveedor se absorbe como costo):
    //   costoEfectivo  = precioNeto * 1.16
    //   precioFinal    = costoEfectivo / 0.54
    //   ivaTrasladado  = precioFinal * 16%
    //   ivaAbsorbido   = precioNeto * 16%   [pagado al proveedor, no recuperable]
    // Libro (tasa 0%): precioFinal = precioNeto * (1 + comision%)  [sin IVA al cliente]
    //   ivaAbsorbido   = precioNeto * 16%   [pagado al proveedor, no recuperable]
    [[nodiscard]]
    CalculationResult calcularConcesionSinCFDI(double precioNetoProveedor,
                                               double comisionPct = 30.0,
                                               TipoProducto tipo  = TipoProducto::Papeleria) const;

    [[nodiscard]]
    static bool isValidPrice(double price) noexcept;

private:
    [[nodiscard]]
    CalculationResult buildFromPrecioFinal(double precioFinal,
                                           double ivaAcreditable,
                                           Escenario escenario,
                                           bool tieneCFDI) const;
};

} // namespace Calculadora
