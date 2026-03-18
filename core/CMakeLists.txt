add_library(core_lib STATIC
    src/PriceCalculator.cpp
    src/AppConfig.cpp
    src/DatabaseManager.cpp
    src/ProductoRepository.cpp
    src/ConcesionRepository.cpp
    src/EmisorRepository.cpp
    src/DocumentoRepository.cpp
)

target_include_directories(core_lib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(core_lib
    PUBLIC Qt6::Core Qt6::Sql
)

set_target_properties(core_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
