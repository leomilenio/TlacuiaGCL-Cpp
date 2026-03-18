#!/usr/bin/env bash
# build_and_run.sh — Compila, ejecuta tests y lanza la GUI
#
# Uso:
#   bash build_and_run.sh          # Compilar + tests + GUI
#   bash build_and_run.sh --deploy # Idem + bundle macOS self-contained en dist/
#
# Plataforma: macOS ARM64 con Qt 6 via Homebrew
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
DEPLOY_FLAG=false

for arg in "$@"; do
    [[ "$arg" == "--deploy" ]] && DEPLOY_FLAG=true
done

# ---------------------------------------------------------------------------
# Detectar Qt
# ---------------------------------------------------------------------------
if command -v brew &>/dev/null && brew --prefix qt &>/dev/null 2>&1; then
    QT_PREFIX="$(brew --prefix qt)"
else
    echo "ERROR: Qt no encontrado via Homebrew."
    echo "Instala con: brew install qt"
    exit 1
fi

echo "==> Qt ${QT_PREFIX##*/opt/} encontrado en: ${QT_PREFIX}"
echo "==> Configurando CMake (Debug)..."

cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="${QT_PREFIX}" \
    -DBUILD_TESTS=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "${SCRIPT_DIR}"

echo ""
echo "==> Compilando..."
cmake --build "${BUILD_DIR}" --parallel

echo ""
echo "==> Ejecutando tests unitarios..."
"${BUILD_DIR}/bin/test_calculadora" --gtest_color=yes
TEST_RESULT=$?

if [ $TEST_RESULT -ne 0 ]; then
    echo ""
    echo "FALLO: Tests fallaron. Corrige los errores antes de continuar."
    exit $TEST_RESULT
fi

echo ""
echo "==> [OK] Todos los tests pasaron."

# ---------------------------------------------------------------------------
# Deploy macOS (--deploy)
# Usa cmake --install que internamente llama macdeployqt para crear un
# bundle .app self-contained con todas las bibliotecas Qt incluidas.
# Equivalente a windeployqt en Windows cuando se compila ahi.
# ---------------------------------------------------------------------------
if $DEPLOY_FLAG; then
    DIST_DIR="${SCRIPT_DIR}/dist/macos"
    echo ""
    echo "==> Creando bundle macOS self-contained en ${DIST_DIR}..."
    cmake --install "${BUILD_DIR}" \
        --prefix "${DIST_DIR}" \
        --config Debug
    echo "==> Bundle listo en: ${DIST_DIR}/CalculadoraPapeleria.app"
    open "${DIST_DIR}/CalculadoraPapeleria.app"
else
    echo "==> Lanzando GUI desde build (modo desarrollo)..."
    open "${BUILD_DIR}/bin/CalculadoraPapeleria.app"
fi
