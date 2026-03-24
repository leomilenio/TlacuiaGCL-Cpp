# COMPAT-10: Especificar versión mínima evita que find_package encuentre la versión
# equivocada en Windows donde pueden coexistir Qt 6.4, 6.6, 6.8, etc.
# En Windows con múltiples instalaciones, pasar -DQt6_DIR al configurar:
#   cmake -B build -DQt6_DIR="C:/Qt/6.8.x/msvc2022_64/lib/cmake/Qt6"
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Sql PrintSupport Network)

qt_standard_project_setup()

message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 Core: ${Qt6Core_FOUND}")
message(STATUS "Qt6 Widgets: ${Qt6Widgets_FOUND}")
message(STATUS "Qt6 Sql: ${Qt6Sql_FOUND}")
message(STATUS "Qt6 PrintSupport: ${Qt6PrintSupport_FOUND}")
