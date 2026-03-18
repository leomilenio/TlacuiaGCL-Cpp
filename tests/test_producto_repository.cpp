#include <gtest/gtest.h>
#include "core/DatabaseManager.h"
#include "core/ProductoRepository.h"
#include "core/PriceCalculator.h"
#include <QCoreApplication>

using namespace Calculadora;

// QSqlDatabase requiere un QCoreApplication activo.
// Se crea estaticamente una vez y nunca se destruye durante los tests.
namespace {
int    g_argc = 0;
char** g_argv = nullptr;
QCoreApplication* g_app = nullptr;

struct InitQtApp {
    InitQtApp() {
        if (!QCoreApplication::instance())
            g_app = new QCoreApplication(g_argc, g_argv);
    }
} g_initQtApp;
}

// Fixture: base de datos SQLite en memoria para cada test
class ProductoRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dbManager = std::make_unique<DatabaseManager>(":memory:");
        ASSERT_TRUE(m_dbManager->initialize());
        m_repo = std::make_unique<ProductoRepository>(*m_dbManager);
    }

    // Crea un ProductoRecord minimo ligado a una concesion ficticia
    ProductoRecord makeProducto(int64_t concesionId,
                                double costo,
                                int cantidadRecibida,
                                int cantidadVendida = 0) {
        ProductoRecord p;
        p.nombreProducto  = "Test";
        p.tipoProducto    = TipoProducto::Papeleria;
        p.precioFinal     = costo * 1.30 * 1.16;
        p.costo           = costo;
        p.comision        = costo * 0.30;
        p.ivaTrasladado   = costo * 1.30 * 0.16;
        p.ivaAcreditable  = costo * 0.16;
        p.ivaNetoPagar    = p.ivaTrasladado - p.ivaAcreditable;
        p.escenario       = Escenario::Concesion;
        p.tieneCFDI       = true;
        p.concesionId     = concesionId;
        p.cantidadRecibida = cantidadRecibida;
        p.cantidadVendida  = cantidadVendida;
        return p;
    }

    // Inserta una concesion de prueba con el ID dado para satisfacer la FK
    void insertConcesionFake(int64_t id) {
        QSqlQuery q(m_dbManager->database());
        q.prepare(R"(
            INSERT INTO concesiones (id, emisor_nombre, activa)
            VALUES (:id, 'Test', 1)
        )");
        q.bindValue(":id", static_cast<qlonglong>(id));
        q.exec();
    }

    std::unique_ptr<DatabaseManager>    m_dbManager;
    std::unique_ptr<ProductoRepository> m_repo;

    static constexpr double EPS = 0.001;
};

TEST_F(ProductoRepositoryTest, CalcularCorte_SinProductos_Invalido) {
    CorteResult r = m_repo->calcularCorte(999);
    EXPECT_FALSE(r.isValid);
}

TEST_F(ProductoRepositoryTest, CalcularCorte_Cantidades_Basico) {
    insertConcesionFake(1);
    // Producto 1: recibidos=5, vendidos=0 (default al guardar), costo=$100
    auto p1 = makeProducto(1, 100.0, 5);
    int64_t id1 = m_repo->save(p1);
    ASSERT_GT(id1, 0);

    // Producto 2: recibidos=10, costo=$80
    auto p2 = makeProducto(1, 80.0, 10);
    int64_t id2 = m_repo->save(p2);
    ASSERT_GT(id2, 0);

    CorteResult r = m_repo->calcularCorte(1);
    ASSERT_TRUE(r.isValid);
    EXPECT_EQ(r.cantidadRegistros,       2);
    EXPECT_EQ(r.totalUnidadesRecibidas,  15);  // 5 + 10
    EXPECT_EQ(r.totalUnidadesVendidas,   0);   // nada vendido aun
    EXPECT_EQ(r.totalUnidadesDevueltas,  15);  // todo devuelto
    EXPECT_NEAR(r.totalPagoAlDistribuidor, 0.0, EPS);  // 0 vendido
    EXPECT_NEAR(r.totalDevolucion, 100.0*5 + 80.0*10, EPS);  // $1300
}

TEST_F(ProductoRepositoryTest, UpdateCantidadVendida_ActualizaCorte) {
    insertConcesionFake(2);
    auto p = makeProducto(2, 54.0, 5);
    int64_t id = m_repo->save(p);
    ASSERT_GT(id, 0);

    // Declarar 3 vendidos
    EXPECT_TRUE(m_repo->updateCantidadVendida(id, 3));

    CorteResult r = m_repo->calcularCorte(2);
    ASSERT_TRUE(r.isValid);
    EXPECT_EQ(r.totalUnidadesRecibidas,  5);
    EXPECT_EQ(r.totalUnidadesVendidas,   3);
    EXPECT_EQ(r.totalUnidadesDevueltas,  2);
    EXPECT_NEAR(r.totalPagoAlDistribuidor, 54.0 * 3, EPS);  // $162
    EXPECT_NEAR(r.totalDevolucion,         54.0 * 2, EPS);  // $108
}

TEST_F(ProductoRepositoryTest, FindByConcesion_DevuelveCorrectamente) {
    insertConcesionFake(3);
    insertConcesionFake(99);
    auto p1 = makeProducto(3, 100.0, 2);
    auto p2 = makeProducto(3, 200.0, 4);
    auto pOtra = makeProducto(99, 50.0, 1);  // otra concesion
    m_repo->save(p1);
    m_repo->save(p2);
    m_repo->save(pOtra);

    auto lista = m_repo->findByConcesion(3);
    EXPECT_EQ(lista.size(), 2);
    for (const auto& p : lista) {
        EXPECT_EQ(p.concesionId.value(), 3);
    }
}

TEST_F(ProductoRepositoryTest, CalcularCorte_MultiplesProductos_SumaCorrecta) {
    insertConcesionFake(4);
    // p1: recibidos=5, vendidos=3 -> devuelta=2, pago=300*3=900
    auto p1 = makeProducto(4, 100.0, 5);
    int64_t id1 = m_repo->save(p1);
    m_repo->updateCantidadVendida(id1, 3);

    // p2: recibidos=10, vendidos=8 -> devuelta=2, pago=80*8=640
    auto p2 = makeProducto(4, 80.0, 10);
    int64_t id2 = m_repo->save(p2);
    m_repo->updateCantidadVendida(id2, 8);

    CorteResult r = m_repo->calcularCorte(4);
    ASSERT_TRUE(r.isValid);
    EXPECT_EQ(r.totalUnidadesRecibidas,  15);  // 5+10
    EXPECT_EQ(r.totalUnidadesVendidas,   11);  // 3+8
    EXPECT_EQ(r.totalUnidadesDevueltas,  4);   // 2+2
    EXPECT_NEAR(r.totalPagoAlDistribuidor, 100.0*3 + 80.0*8, EPS);  // 300+640=940
    EXPECT_NEAR(r.totalDevolucion,         100.0*2 + 80.0*2, EPS);  // 200+160=360
}
