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
    // El IVA pagado al proveedor es acreditable -> costo base = precio neto.
    // precioFinal = precioNetoProveedor / 0.54
    // comisionPct: porcentaje de comision (ej. 30.0 = 30%); default = PriceRatios::COMISION * 100
    [[nodiscard]]
    CalculationResult calcularConcesionConCFDI(double precioNetoProveedor,
                                               double comisionPct = 30.0) const;

    // Escenario 2b: Concesion sin CFDI.
    // El IVA no puede acreditarse, se absorbe en el costo.
    // costoEfectivo = precioNetoProveedor * 1.16
    // precioFinal = costoEfectivo / 0.54
    [[nodiscard]]
    CalculationResult calcularConcesionSinCFDI(double precioNetoProveedor,
                                               double comisionPct = 30.0) const;

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
