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
// Modelo margen: costo = 54% del precio final (igual que ProductoPropio).
// Proveedor neto $54:
//   precioFinal    = $54 / 0.54 = $100.00
//   comision       = $100 * 0.30 = $30.00
//   ivaTrasladado  = $100 * 0.16 = $16.00
//   ivaAcreditable = $54 * 0.16  = $8.64  (IVA pagado al proveedor, recuperable)
//   ivaNetoPagar   = $16.00 - $8.64 = $7.36

TEST_F(PriceCalculatorTest, ConcesionConCFDI_BasicoNeto54) {
    auto r = calc.calcularConcesionConCFDI(54.0);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,   100.0,  EPS);
    EXPECT_NEAR(r.costo,          54.0,  EPS);
    EXPECT_NEAR(r.comision,       30.0,  EPS);
    EXPECT_NEAR(r.ivaTrasladado,  16.0,  EPS);
    EXPECT_NEAR(r.ivaAcreditable,  8.64, EPS);
    EXPECT_NEAR(r.ivaNetoPagar,    7.36, EPS);
    EXPECT_NEAR(r.ivaAbsorbido,    0.0,  EPS);
    EXPECT_EQ(r.escenario, Escenario::Concesion);
    EXPECT_TRUE(r.tieneCFDI);
}

TEST_F(PriceCalculatorTest, ConcesionConCFDI_Invalido) {
    EXPECT_FALSE(calc.calcularConcesionConCFDI(0.0).isValid);
}

// ---- Escenario 2b: Concesion sin CFDI ----
// Modelo margen: el IVA pagado al proveedor se absorbe como costo antes de calcular.
// Proveedor neto $54:
//   costoEfectivo = $54 * 1.16 = $62.64  (IVA absorbido: $8.64)
//   precioFinal   = $62.64 / 0.54 = $116.00
//   comision      = $116 * 0.30  = $34.80
//   ivaTrasladado = $116 * 0.16  = $18.56  (todo se entera a SAT)
//   ivaAcreditable = $0
//   ivaNetoPagar  = $18.56

TEST_F(PriceCalculatorTest, ConcesionSinCFDI_BasicoNeto54) {
    auto r = calc.calcularConcesionSinCFDI(54.0);
    EXPECT_TRUE(r.isValid);
    EXPECT_NEAR(r.precioFinal,   116.0,  EPS);
    EXPECT_NEAR(r.costo,          54.0,  EPS);
    EXPECT_NEAR(r.comision,       34.80, EPS);
    EXPECT_NEAR(r.ivaTrasladado,  18.56, EPS);
    EXPECT_NEAR(r.ivaAcreditable,  0.0,  EPS);
    EXPECT_NEAR(r.ivaNetoPagar,   18.56, EPS);
    EXPECT_NEAR(r.ivaAbsorbido,    8.64, EPS);
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

// Ambos escenarios usan el mismo modelo de margen (costo = 54% del precioFinal).
// Con precioFinal=$100 (Propio) y precioNeto=$54 (Concesion), todos los campos son identicos.
TEST_F(PriceCalculatorTest, ProductoPropio_ConCFDI_SimetriaConConcesion) {
    auto propio    = calc.calcularProductoPropio(100.0, true);
    auto concesion = calc.calcularConcesionConCFDI(54.0);
    EXPECT_NEAR(propio.precioFinal,    concesion.precioFinal,    EPS);  // $100
    EXPECT_NEAR(propio.comision,       concesion.comision,       EPS);  // $30
    EXPECT_NEAR(propio.ivaTrasladado,  concesion.ivaTrasladado,  EPS);  // $16
    EXPECT_NEAR(propio.ivaAcreditable, concesion.ivaAcreditable, EPS);  // $8.64
    EXPECT_NEAR(propio.ivaNetoPagar,   concesion.ivaNetoPagar,   EPS);  // $7.36
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
