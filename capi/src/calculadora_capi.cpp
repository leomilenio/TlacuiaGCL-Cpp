#include "capi/calculadora_capi.h"
#include "core/PriceCalculator.h"
#include <cstring>

using namespace Calculadora;

static PriceCalculator* g_calculator = nullptr;

static CApiResult toCApi(const CalculationResult& r) {
    CApiResult out{};
    out.precio_final    = r.precioFinal;
    out.costo           = r.costo;
    out.comision        = r.comision;
    out.iva_trasladado  = r.ivaTrasladado;
    out.iva_acreditable = r.ivaAcreditable;
    out.iva_neto_sat    = r.ivaNetoPagar;
    out.escenario       = (r.escenario == Escenario::Concesion) ? 1 : 0;
    out.tiene_cfdi      = r.tieneCFDI ? 1 : 0;
    out.is_valid        = r.isValid ? 1 : 0;
    if (!r.errorMessage.empty()) {
        std::strncpy(out.error_message, r.errorMessage.c_str(), sizeof(out.error_message) - 1);
        out.error_message[sizeof(out.error_message) - 1] = '\0';
    }
    return out;
}

extern "C" {

void calculadora_init(void) {
    if (!g_calculator) {
        g_calculator = new PriceCalculator();
    }
}

void calculadora_shutdown(void) {
    delete g_calculator;
    g_calculator = nullptr;
}

CApiResult calc_producto_propio(double precio_final) {
    if (!g_calculator) calculadora_init();
    return toCApi(g_calculator->calcularProductoPropio(precio_final, false));
}

CApiResult calc_producto_propio_con_cfdi(double precio_final) {
    if (!g_calculator) calculadora_init();
    return toCApi(g_calculator->calcularProductoPropio(precio_final, true));
}

CApiResult calc_concesion_con_cfdi(double precio_neto_proveedor) {
    if (!g_calculator) calculadora_init();
    return toCApi(g_calculator->calcularConcesionConCFDI(precio_neto_proveedor));
}

CApiResult calc_concesion_sin_cfdi(double precio_neto_proveedor) {
    if (!g_calculator) calculadora_init();
    return toCApi(g_calculator->calcularConcesionSinCFDI(precio_neto_proveedor));
}

} // extern "C"
