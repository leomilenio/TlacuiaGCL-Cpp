add_library(project_warnings INTERFACE)

if(MSVC)
    target_compile_options(project_warnings INTERFACE
        /W4
        /permissive-    # Conformancia estricta con el estándar C++
        /wd4251         # Suprimir warning de DLL-interface (internos de Qt)

        # Sin /Zc:__cplusplus, MSVC siempre reporta __cplusplus = 199711L
        # aunque compile en modo C++20. Esto rompe macros de detección de
        # features en Qt y en headers de C++20 que comprueban __cplusplus.
        /Zc:__cplusplus

        # Fuerza que el compilador interprete los archivos .cpp/.h como UTF-8
        # y que los string literals se emitan en UTF-8 en el binario.
        # Sin este flag, MSVC usa el locale del sistema (ej. cp1252 en Windows
        # en español), lo que corrompe string literals con tildes y 'ñ'.
        /utf-8
    )
else()
    target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()
