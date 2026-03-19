set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# macOS: include .icns so CMake copies it into the bundle's Resources/
set(MACOS_ICON "")
if(APPLE)
    set(MACOS_ICON resources/icons/icon.icns)
    set_source_files_properties(${MACOS_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

# Los headers con Q_OBJECT deben estar en la lista de fuentes para que AUTOMOC los procese
add_executable(CalculadoraPapeleria WIN32 MACOSX_BUNDLE
    main.cpp
    resources/icons.qrc
    ${MACOS_ICON}
    include/app/MainWindow.h
    include/app/CalculatorWidget.h
    include/app/HistoryWidget.h
    include/app/HistoryTableModel.h
    include/app/SaveDialog.h
    include/app/AlertDialog.h
    include/app/NuevaConcesionDialog.h
    include/app/EmisoresWidget.h
    include/app/ConcesionesWidget.h
    include/app/AgregarProductoDialog.h
    include/app/CorteDialog.h
    include/app/CortePdfExporter.h
    include/app/EmisorProfileDialog.h
    include/app/EmisorPdfExporter.h
    include/app/SettingsDialog.h
    include/app/SplashConcesionesWindow.h
    include/app/AboutDialog.h
    include/app/UpdateDialog.h
    src/MainWindow.cpp
    src/CalculatorWidget.cpp
    src/HistoryWidget.cpp
    src/HistoryTableModel.cpp
    src/SaveDialog.cpp
    src/AlertDialog.cpp
    src/NuevaConcesionDialog.cpp
    src/EmisoresWidget.cpp
    src/ConcesionesWidget.cpp
    src/AgregarProductoDialog.cpp
    src/CorteDialog.cpp
    src/CortePdfExporter.cpp
    src/EmisorProfileDialog.cpp
    src/EmisorPdfExporter.cpp
    src/SettingsDialog.cpp
    src/SplashConcesionesWindow.cpp
    src/AboutDialog.cpp
    src/UpdateDialog.cpp
)

target_include_directories(CalculadoraPapeleria
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(CalculadoraPapeleria
    PRIVATE core_lib Qt6::Widgets Qt6::PrintSupport Qt6::Network
)

set_target_properties(CalculadoraPapeleria PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER       "com.mx.arle.tlacuia"
    MACOSX_BUNDLE_BUNDLE_NAME          "Gestor de Concesiones Tlacuia"
    MACOSX_BUNDLE_BUNDLE_VERSION       "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT            "ARLE"
    MACOSX_BUNDLE_ICON_FILE            "icon"        # references icon.icns in Resources/
)

# Windows: embed icon into the .exe via RC_ICONS
if(WIN32)
    set_target_properties(CalculadoraPapeleria PROPERTIES
        RC_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico"
    )
endif()

# ---------------------------------------------------------------------------
# Deployment — qt_generate_deploy_app_script (Qt 6.3+)
#
# macOS: cmake --install build --prefix dist/macos
#        genera un .app self-contained usando macdeployqt internamente.
#
# Windows: cmake --install build --prefix dist\windows
#          copia windeployqt + DLLs Qt al directorio de instalacion.
#
# En entorno de desarrollo macOS NO es necesario hacer install; la app
# corre directamente desde build/bin/ porque DYLD_LIBRARY_PATH incluye
# los frameworks de Qt de Homebrew via rpath implicito.
# ---------------------------------------------------------------------------
qt_generate_deploy_app_script(
    TARGET CalculadoraPapeleria
    OUTPUT_SCRIPT CALCULADORA_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(TARGETS CalculadoraPapeleria
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
)
install(SCRIPT ${CALCULADORA_DEPLOY_SCRIPT})
