#include <gtest/gtest.h>
#include "core/PriceCalculator.h"

using namespace Calculadora;

class PriceCalculatorTest : public ::testing::Test {
protected:
    PriceCalculator calc;
    static constexpr double EPS = 0.001;
};

// ---- Escenario 1: Producto Propio ----

TEST_F(PriceCalculatorTest, ProductoPropio_PrecioRedondo) {
    auto r = calc.calcularProductoPropio(100.0);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,   100.0, EPS);
    EXPECT_NEAR(r.costo,          54.0, EPS);
    EXPECT_NEAR(r.comision,       30.0, EPS);
    EXPECT_NEAR(r.ivaTrasladado,  16.0, EPS);
    EXPECT_NEAR(r.ivaAcreditable,  0.0, EPS);
    EXPECT_NEAR(r.ivaNetoPagar,   16.0, EPS);
    EXPECT_EQ(r.escenario, Escenario::ProductoPropio);
    EXPECT_FALSE(r.tieneCFDI);
}

TEST_F(PriceCalculatorTest, ProductoPropio_PrecioInvalido) {
    EXPECT_FALSE(calc.calcularProductoPropio(-10.0).isValid);
    EXPECT_FALSE(calc.calcularProductoPropio(0.0).isValid);
}

// ---- Escenario 2a: Concesion con CFDI ----
// Modelo markup: comision 30% sobre precio neto del proveedor.
// Proveedor neto $54:
//   precioSinIVA   = $54 * 1.30 = $70.20
//   precioFinal    = $70.20 * 1.16 = $81.432
//   comision       = $54 * 0.30 = $16.20
//   ivaTrasladado  = $70.20 * 0.16 = $11.232
//   ivaAcreditable = $54 * 0.16 = $8.64
//   ivaNetoPagar   = $11.232 - $8.64 = $2.592

TEST_F(PriceCalculatorTest, ConcesionConCFDI_BasicoNeto54) {
    auto r = calc.calcularConcesionConCFDI(54.0);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,    81.432, EPS);
    EXPECT_NEAR(r.costo,          54.0,   EPS);
    EXPECT_NEAR(r.comision,       16.20,  EPS);
    EXPECT_NEAR(r.ivaTrasladado,  11.232, EPS);
    EXPECT_NEAR(r.ivaAcreditable,  8.64,  EPS);
    EXPECT_NEAR(r.ivaNetoPagar,    2.592, EPS);
    EXPECT_NEAR(r.ivaAbsorbido,    0.0,   EPS);
    EXPECT_EQ(r.escenario, Escenario::Concesion);
    EXPECT_TRUE(r.tieneCFDI);
}

TEST_F(PriceCalculatorTest, ConcesionConCFDI_Invalido) {
    EXPECT_FALSE(calc.calcularConcesionConCFDI(0.0).isValid);
}

// ---- Escenario 2b: Concesion sin CFDI ----
// Mismo precioFinal que Con CFDI. La libreria absorbe el IVA pagado al proveedor.
// Proveedor neto $54:
//   precioFinal   = $81.432  (igual que Con CFDI)
//   costo         = $54.00   (precio neto del proveedor)
//   comision      = $16.20   (30% sobre costo, misma comision nominal)
//   ivaTrasladado = $11.232
//   ivaAbsorbido  = $54 * 0.16 = $8.64  (IVA pagado sin CFDI, no acreditable)
//   ivaNetoPagar  = $11.232  (todo se entera a SAT)

TEST_F(PriceCalculatorTest, ConcesionSinCFDI_BasicoNeto54) {
    auto r = calc.calcularConcesionSinCFDI(54.0);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,    81.432, EPS);  // igual que Con CFDI
    EXPECT_NEAR(r.costo,          54.0,   EPS);
    EXPECT_NEAR(r.comision,       16.20,  EPS);
    EXPECT_NEAR(r.ivaTrasladado,  11.232, EPS);
    EXPECT_NEAR(r.ivaAcreditable,  0.0,   EPS);
    EXPECT_NEAR(r.ivaNetoPagar,   11.232, EPS);
    EXPECT_NEAR(r.ivaAbsorbido,    8.64,  EPS);
    EXPECT_EQ(r.escenario, Escenario::Concesion);
    EXPECT_FALSE(r.tieneCFDI);
}

TEST_F(PriceCalculatorTest, ConcesionSinCFDI_Invalido) {
    EXPECT_FALSE(calc.calcularConcesionSinCFDI(-1.0).isValid);
}

// ---- Escenario 1b: Producto Propio con CFDI de compra (LIVA Art. 4-5) ----
// precioFinal = $100 (usuario lo fija)
// costo implicito = $100 * 54% = $54
// ivaAcreditable  = $54 * 16% = $8.64  (IVA pagado al proveedor con CFDI)
// ivaTrasladado   = $100 * 16% = $16.00 (IVA que cobra al cliente)
// ivaNetoPagar    = $16.00 - $8.64 = $7.36 (lo que entera a SAT)

TEST_F(PriceCalculatorTest, ProductoPropio_ConCFDI_Precio100) {
    auto r = calc.calcularProductoPropio(100.0, /*conCFDI=*/true);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,   100.0, EPS);
    EXPECT_NEAR(r.costo,          54.0, EPS);
    EXPECT_NEAR(r.comision,       30.0, EPS);
    EXPECT_NEAR(r.ivaTrasladado,  16.0, EPS);
    EXPECT_NEAR(r.ivaAcreditable,  8.64, EPS);
    EXPECT_NEAR(r.ivaNetoPagar,    7.36, EPS);
    EXPECT_EQ(r.escenario, Escenario::ProductoPropio);
    EXPECT_TRUE(r.tieneCFDI);
}

// El IVA Acreditable es identico entre Producto Propio Con CFDI y Concesion Con CFDI
// cuando el costo implicito (54% del precio final) == precio neto del proveedor ($54).
// Nota: ivaNetoPagar ya NO es simetrico porque los modelos de precio son distintos
// (margen para Propio vs markup para Concesion).
TEST_F(PriceCalculatorTest, ProductoPropio_ConCFDI_SimetriaConConcesion) {
    auto propio    = calc.calcularProductoPropio(100.0, true);
    auto concesion = calc.calcularConcesionConCFDI(54.0);
    EXPECT_NEAR(propio.ivaAcreditable, concesion.ivaAcreditable, EPS);  // $8.64 en ambos
}

// Sin CFDI el comportamiento es igual que antes (retrocompatibilidad)
TEST_F(PriceCalculatorTest, ProductoPropio_SinCFDI_EsRetrocompatible) {
    auto sinParam = calc.calcularProductoPropio(100.0);           // default false
    auto explicit_ = calc.calcularProductoPropio(100.0, false);
    EXPECT_NEAR(sinParam.ivaAcreditable, explicit_.ivaAcreditable, EPS);
    EXPECT_NEAR(sinParam.ivaNetoPagar,   explicit_.ivaNetoPagar,   EPS);
    EXPECT_FALSE(sinParam.tieneCFDI);
}

// ---- Coherencia de ratios ----
TEST_F(PriceCalculatorTest, RatiosSumanUno) {
    double suma = PriceRatios::COSTO + PriceRatios::COMISION + PriceRatios::IVA;
    EXPECT_NEAR(suma, 1.0, EPS);
}
