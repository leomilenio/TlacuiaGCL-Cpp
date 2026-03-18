add_library(project_warnings INTERFACE)

if(MSVC)
    target_compile_options(project_warnings INTERFACE
        /W4
        /permissive-
        /wd4251  # DLL-interface warning (Qt internals)
    )
else()
    target_compile_options(project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()
