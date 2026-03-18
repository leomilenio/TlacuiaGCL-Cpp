add_library(calculadora_shared SHARED
    src/calculadora_capi.cpp
)

target_include_directories(calculadora_shared
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(calculadora_shared
    PRIVATE core_lib
)

if(WIN32)
    target_compile_definitions(calculadora_shared PRIVATE CAPI_EXPORTS)
endif()

if(UNIX AND NOT APPLE)
    target_link_options(calculadora_shared PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/capi.map)
endif()
