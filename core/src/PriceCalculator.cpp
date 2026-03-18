#include "core/PriceCalculator.h"
#include <cmath>
#include <limits>

namespace Calculadora {

bool PriceCalculator::isValidPrice(double price) noexcept {
    return std::isfinite(price) && price > 0.0;
}

CalculationResult PriceCalculator::buildFromPrecioFinal(double precioFinal,
                                                         double ivaAcreditable,
                                                         Escenario escenario,
                                                         bool tieneCFDI) const {
    CalculationResult r;
    r.precioFinal    = precioFinal;
    r.costo          = precioFinal * PriceRatios::COSTO;
    r.comision       = precioFinal * PriceRatios::COMISION;
    r.ivaTrasladado  = precioFinal * PriceRatios::IVA;
    r.ivaAcreditable = ivaAcreditable;
    r.ivaNetoPagar   = r.ivaTrasladado - r.ivaAcreditable;
    r.escenario      = escenario;
    r.tieneCFDI      = tieneCFDI;
    r.isValid        = true;
    return r;
}

// Escenario 1: El usuario conoce el precio final que quiere cobrar.
//
// El IVA Trasladado (LIVA Art. 1) es siempre el 16% del precio final: es el IVA
// que el comerciante cobra al comprador final y debe enterar a SAT.
//
// conCFDI=true: el comerciante adquirio el producto con CFDI valido del proveedor.
//   El IVA pagado en esa compra es Acreditable (LIVA Art. 4 y 5).
//   Se estima como: ivaAcreditable = costo (54% del precio) * 16%
//   Requisitos legales del IVA Acreditable (LIVA Art. 5):
//     - Que corresponda a actividades gravadas
//     - Que el gasto sea deducible para ISR
//     - Que conste en CFDI con los requisitos del CFF Art. 29-A
//     - Que pagos >$2,000 MXN sean via sistema financiero
//
// conCFDI=false: sin CFDI o producto de elaboracion propia.
//   ivaAcreditable = 0; todo el IVA Trasladado se entera a SAT.
CalculationResult PriceCalculator::calcularProductoPropio(double precioFinal, bool conCFDI) const {
    if (!isValidPrice(precioFinal)) {
        CalculationResult r;
        r.isValid = false;
        r.errorMessage = "El precio debe ser un valor positivo.";
        return r;
    }
    double ivaAcreditable = 0.0;
    if (conCFDI) {
        // El costo (54% del precio) representa el precio neto de compra al proveedor.
        // El IVA que se pago sobre ese costo es recuperable via acreditamiento.
        double costoImplicito = precioFinal * PriceRatios::COSTO;
        ivaAcreditable = costoImplicito * PriceRatios::IVA;
    }
    return buildFromPrecioFinal(precioFinal, ivaAcreditable, Escenario::ProductoPropio, conCFDI);
}

// Escenario 2a: Concesion con CFDI del proveedor.
//
// Modelo de markup: la comision (30%) se aplica SOBRE el precio neto del proveedor.
// Al hacer el corte, el proveedor recupera integramente su costo y la libreria
// retiene el 30% acordado.
//
//   precioSinIVA   = precioNeto * 1.30    (proveedor + comision libreria)
//   precioFinal    = precioSinIVA * 1.16  (agrega IVA cobrado al cliente)
//   ivaAcreditable = precioNeto * 0.16    (IVA pagado al proveedor, recuperable con CFDI)
//   ivaNetoPagar   = ivaTrasladado - ivaAcreditable
CalculationResult PriceCalculator::calcularConcesionConCFDI(double precioNetoProveedor,
                                                             double comisionPct) const {
    if (!isValidPrice(precioNetoProveedor)) {
        CalculationResult r;
        r.isValid = false;
        r.errorMessage = "El precio neto del proveedor debe ser un valor positivo.";
        return r;
    }
    const double comisionRatio = comisionPct / 100.0;
    CalculationResult r;
    const double precioSinIVA = precioNetoProveedor * (1.0 + comisionRatio);
    r.precioFinal    = precioSinIVA * (1.0 + PriceRatios::IVA);
    r.costo          = precioNetoProveedor;
    r.comision       = precioNetoProveedor * comisionRatio;
    r.ivaTrasladado  = precioSinIVA * PriceRatios::IVA;
    r.ivaAcreditable = precioNetoProveedor * PriceRatios::IVA;
    r.ivaNetoPagar   = r.ivaTrasladado - r.ivaAcreditable;
    r.ivaAbsorbido   = 0.0;
    r.escenario      = Escenario::Concesion;
    r.tieneCFDI      = true;
    r.isValid        = true;
    return r;
}

// Escenario 2b: Concesion sin CFDI.
//
// Mismo precio final que Con CFDI (mismo markup 30% + IVA al cliente).
// La diferencia: la libreria pago IVA al proveedor pero no puede acreditarlo.
// Ese IVA (ivaAbsorbido) es un costo adicional que reduce el margen efectivo.
//
//   precioFinal    = precioNeto * 1.30 * 1.16   (igual que Con CFDI)
//   ivaAbsorbido   = precioNeto * 0.16           (IVA pagado, no recuperable)
//   ivaAcreditable = 0
//   ivaNetoPagar   = ivaTrasladado (todo se entera a SAT)
CalculationResult PriceCalculator::calcularConcesionSinCFDI(double precioNetoProveedor,
                                                             double comisionPct) const {
    if (!isValidPrice(precioNetoProveedor)) {
        CalculationResult r;
        r.isValid = false;
        r.errorMessage = "El precio neto del proveedor debe ser un valor positivo.";
        return r;
    }
    const double comisionRatio = comisionPct / 100.0;
    CalculationResult r;
    const double precioSinIVA = precioNetoProveedor * (1.0 + comisionRatio);
    r.precioFinal    = precioSinIVA * (1.0 + PriceRatios::IVA);
    r.costo          = precioNetoProveedor;
    r.comision       = precioNetoProveedor * comisionRatio;
    r.ivaTrasladado  = precioSinIVA * PriceRatios::IVA;
    r.ivaAcreditable = 0.0;
    r.ivaNetoPagar   = r.ivaTrasladado;
    r.ivaAbsorbido   = precioNetoProveedor * PriceRatios::IVA;
    r.escenario      = Escenario::Concesion;
    r.tieneCFDI      = false;
    r.isValid        = true;
    return r;
}

} // namespace Calculadora
