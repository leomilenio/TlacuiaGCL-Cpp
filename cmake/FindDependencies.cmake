find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql PrintSupport Network)

qt_standard_project_setup()

message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 Core: ${Qt6Core_FOUND}")
message(STATUS "Qt6 Widgets: ${Qt6Widgets_FOUND}")
message(STATUS "Qt6 Sql: ${Qt6Sql_FOUND}")
message(STATUS "Qt6 PrintSupport: ${Qt6PrintSupport_FOUND}")
