include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_calculadora
    test_price_calculator.cpp
    test_producto_repository.cpp
)

target_link_libraries(test_calculadora
    PRIVATE core_lib GTest::gtest_main Qt6::Sql
)

include(GoogleTest)
gtest_discover_tests(test_calculadora)
