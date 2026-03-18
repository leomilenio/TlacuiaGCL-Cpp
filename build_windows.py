#!/usr/bin/env python3
from __future__ import annotations  # Permite "Path | None" en Python 3.9+
"""
build_windows.py — Compila TlacuiaGCL para Windows x86_64

Detecta la plataforma host y elige la estrategia:

  Linux   → Cross-compilacion via MXE (mxe.cc)
              El .exe resultante es estatico (auto-contenido, sin DLLs externas).

  Windows → Compilacion nativa con MSVC o MinGW + Qt6 instalado localmente.
              windeployqt copia las DLLs de Qt junto al .exe.

Uso:
  python3 build_windows.py
  python3 build_windows.py --debug
  python3 build_windows.py --mxe-root /opt/mxe
  python3 build_windows.py --qt-prefix C:/Qt/6.7.2/msvc2022_64
  python3 build_windows.py --output-dir ./dist/windows

─────────────────────────────────────────────────────────────────────────────
Prerequisitos en Linux (MXE via PPA oficial):

  sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9
  sudo add-apt-repository 'deb [arch=amd64] https://pkg.mxe.cc/repos/apt focal main'
  sudo apt-get update
  sudo apt-get install \\
      mxe-x86-64-w64-mingw32.static-cmake \\
      mxe-x86-64-w64-mingw32.static-qt6-qtbase \\
      mxe-x86-64-w64-mingw32.static-qt6-qttools \\
      mxe-x86-64-w64-mingw32.static-qt6-qtserialport

  Instalacion alternativa (compilar MXE desde fuente):
    git clone https://github.com/mxe/mxe.git ~/mxe
    cd ~/mxe
    make MXE_TARGETS=x86_64-w64-mingw32.static qt6-qtbase qt6-qttools cmake

Prerequisitos en Windows:
  - Qt 6.x instalado desde https://www.qt.io/download
  - Visual Studio 2022 (recomendado) o MinGW-w64 con gcc 12+
  - CMake 3.24+ en el PATH
─────────────────────────────────────────────────────────────────────────────
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuracion del proyecto
# ---------------------------------------------------------------------------

PROJECT_NAME   = "CalculadoraPapeleria"
EXE_NAME       = f"{PROJECT_NAME}.exe"
MXE_TARGET     = "x86_64-w64-mingw32.static"   # target estatico 64-bit
BUILD_DIR_NAME = "build-windows"

# Rutas donde buscar MXE en Linux (en orden de preferencia)
MXE_SEARCH_PATHS = [
    Path("/usr/lib/mxe"),           # PPA oficial de MXE en Ubuntu/Debian
    Path.home() / "mxe",            # build desde fuente en $HOME
    Path("/opt/mxe"),               # instalacion manual comun
    Path("/usr/local/mxe"),
]

# Rutas base donde buscar Qt6 en Windows
QT_WIN_SEARCH_PATHS = [
    Path("C:/Qt"),
    Path(os.environ.get("USERPROFILE", "C:/Users/User")) / "Qt",
    Path("C:/Program Files/Qt"),
]

# Variantes de kit de Qt6 en Windows, en orden de preferencia
QT_WIN_VARIANTS = [
    "msvc2022_64",
    "msvc2019_64",
    "mingw_64",
    "mingw64_64",
]

# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------

BOLD  = "\033[1m"
GREEN = "\033[32m"
RED   = "\033[31m"
CYAN  = "\033[36m"
RESET = "\033[0m"


def _supports_color() -> bool:
    return sys.stdout.isatty() and platform.system() != "Windows"


def header(msg: str) -> None:
    line = "─" * 70
    if _supports_color():
        print(f"\n{CYAN}{line}{RESET}")
        print(f"{BOLD}  {msg}{RESET}")
        print(f"{CYAN}{line}{RESET}")
    else:
        print(f"\n{'=' * 70}")
        print(f"  {msg}")
        print(f"{'=' * 70}")


def ok(msg: str) -> None:
    prefix = f"{GREEN}[OK]{RESET}" if _supports_color() else "[OK]"
    print(f"{prefix} {msg}")


def error(msg: str) -> None:
    prefix = f"{RED}[ERROR]{RESET}" if _supports_color() else "[ERROR]"
    print(f"{prefix} {msg}", file=sys.stderr)


def warn(msg: str) -> None:
    print(f"[AVISO] {msg}")


def run(cmd: list, cwd: Path = None, extra_env: dict = None) -> None:
    """Ejecuta un comando mostrando la invocacion. Termina el proceso si falla."""
    display = " ".join(str(c) for c in cmd)
    print(f"\n$ {display}")

    env = os.environ.copy()
    if extra_env:
        env.update(extra_env)

    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        error(f"Comando fallido (codigo {result.returncode}).")
        sys.exit(result.returncode)


def find_exe_in_build(build_dir: Path, build_type: str) -> Path | None:
    """Busca el .exe en las ubicaciones tipicas de CMake."""
    candidates = [
        build_dir / "bin" / EXE_NAME,
        build_dir / "bin" / build_type / EXE_NAME,
        build_dir / build_type / "bin" / EXE_NAME,
        build_dir / build_type / EXE_NAME,
    ]
    for c in candidates:
        if c.exists():
            return c
    # Busqueda recursiva como ultimo recurso
    matches = list(build_dir.rglob(EXE_NAME))
    return matches[0] if matches else None


# ---------------------------------------------------------------------------
# Deteccion de MXE (Linux)
# ---------------------------------------------------------------------------

def find_mxe(override: Path | None) -> Path:
    """Retorna la raiz de MXE o termina con error si no se encuentra."""
    candidates = [override] if override else MXE_SEARCH_PATHS

    for path in candidates:
        if not path or not path.is_dir():
            continue
        # Verificar que el cmake wrapper exista — es la señal de que MXE
        # esta correctamente instalado con el target correcto.
        cmake_wrapper = path / "usr" / "bin" / f"{MXE_TARGET}-cmake"
        if cmake_wrapper.exists():
            return path

    error("No se encontro MXE con el target x86_64-w64-mingw32.static.")
    print()
    print("  Rutas buscadas:")
    for p in candidates:
        print(f"    {p}")
    print()
    print("  Instalacion rapida (Ubuntu/Debian via PPA):")
    print("    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9")
    print("    sudo add-apt-repository 'deb [arch=amd64] https://pkg.mxe.cc/repos/apt focal main'")
    print("    sudo apt-get update")
    print("    sudo apt-get install \\")
    print("        mxe-x86-64-w64-mingw32.static-cmake \\")
    print("        mxe-x86-64-w64-mingw32.static-qt6-qtbase \\")
    print("        mxe-x86-64-w64-mingw32.static-qt6-qttools")
    print()
    print("  Compilar MXE desde fuente:")
    print("    git clone https://github.com/mxe/mxe.git ~/mxe && cd ~/mxe")
    print("    make MXE_TARGETS=x86_64-w64-mingw32.static qt6-qtbase qt6-qttools cmake")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Build en Linux → cross-compile via MXE
# ---------------------------------------------------------------------------

def build_linux(args: argparse.Namespace, project_root: Path) -> None:
    header("Cross-compilacion Windows x86_64 via MXE (host: Linux)")

    mxe_root     = find_mxe(Path(args.mxe_root) if args.mxe_root else None)
    cmake_wrapper = mxe_root / "usr" / "bin" / f"{MXE_TARGET}-cmake"
    mxe_prefix   = mxe_root / "usr" / MXE_TARGET
    build_dir    = project_root / BUILD_DIR_NAME
    build_type   = "Debug" if args.debug else "Release"
    output_dir   = Path(args.output_dir) if args.output_dir else project_root / "dist" / "windows"

    print(f"  MXE root:    {mxe_root}")
    print(f"  cmake:       {cmake_wrapper}")
    print(f"  Qt prefix:   {mxe_prefix}")
    print(f"  Build type:  {build_type}")
    print(f"  Salida:      {output_dir}")

    # El cmake wrapper de MXE ya configura internamente el toolchain, sysroot
    # y CMAKE_PREFIX_PATH para el target MXE_TARGET. Solo pasamos las opciones
    # propias del proyecto.
    header("Configurando CMake...")
    run([
        str(cmake_wrapper),
        "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DBUILD_TESTS=OFF",          # No se pueden ejecutar tests de Windows en Linux
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF",
        str(project_root),
    ])

    header("Compilando...")
    run([str(cmake_wrapper), "--build", str(build_dir), "--parallel"])

    # Copiar .exe al directorio de salida
    exe_src = find_exe_in_build(build_dir, build_type)
    if not exe_src:
        error(f"No se encontro {EXE_NAME} en {build_dir}")
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)
    exe_dest = output_dir / EXE_NAME
    shutil.copy2(exe_src, exe_dest)

    size_mb = exe_dest.stat().st_size / (1024 * 1024)
    header("Build completado")
    ok(f"{exe_dest}  ({size_mb:.1f} MB)")
    print()
    print("  El .exe es estatico (MXE .static) — no requiere DLLs de Qt")
    print("  para ejecutarse en Windows. Puede distribuirse directamente.")


# ---------------------------------------------------------------------------
# Deteccion de Qt6 en Windows
# ---------------------------------------------------------------------------

def find_qt_windows(override: Path | None) -> Path:
    """Busca Qt6 instalado en Windows. Retorna el prefix path (contiene bin/, lib/)."""
    if override:
        if override.is_dir() and (override / "lib" / "cmake" / "Qt6").is_dir():
            return override
        error(f"La ruta de Qt especificada no es valida: {override}")
        print("  Debe contener lib/cmake/Qt6/")
        sys.exit(1)

    for base in QT_WIN_SEARCH_PATHS:
        if not base.is_dir():
            continue
        # Qt instala versiones como C:/Qt/6.7.2/<variant>
        try:
            version_dirs = sorted(
                [d for d in base.iterdir() if d.is_dir() and d.name.startswith("6.")],
                reverse=True,   # version mas reciente primero
            )
        except PermissionError:
            continue

        for ver_dir in version_dirs:
            for variant in QT_WIN_VARIANTS:
                candidate = ver_dir / variant
                cmake_dir = candidate / "lib" / "cmake" / "Qt6"
                if cmake_dir.is_dir():
                    return candidate

    error("No se encontro Qt6 instalado en Windows.")
    print("  Instala Qt6 desde https://www.qt.io/download")
    print("  o especifica la ruta con:  --qt-prefix C:/Qt/6.x.x/msvc2022_64")
    sys.exit(1)


def detect_compiler_windows() -> tuple[str, list[str]]:
    """
    Detecta el compilador disponible.
    Retorna (generator_name, cmake_extra_flags).
    Preferencia: MSVC > Ninja+MinGW > MinGW Makefiles.
    """
    # MSVC: cl.exe en el PATH (requiere haber activado el Developer Command Prompt)
    if shutil.which("cl"):
        return "Visual Studio 17 2022", ["-A", "x64"]

    # Ninja + MinGW (rapido)
    if shutil.which("ninja") and shutil.which("gcc"):
        return "Ninja", [
            "-DCMAKE_C_COMPILER=gcc",
            "-DCMAKE_CXX_COMPILER=g++",
        ]

    # MinGW Makefiles (fallback)
    if shutil.which("mingw32-make") or shutil.which("make"):
        return "MinGW Makefiles", []

    error("No se detecto ningun compilador C++ compatible.")
    print()
    print("  Opciones:")
    print("    1. Instala Visual Studio 2022 con el componente 'C++ build tools'")
    print("       y ejecuta este script desde el 'Developer Command Prompt'.")
    print("    2. Instala MinGW-w64 (https://www.mingw-w64.org/) y agrega")
    print("       su carpeta bin/ al PATH.")
    sys.exit(1)


def find_cmake_windows() -> str:
    cmake = shutil.which("cmake")
    if cmake:
        return cmake
    for candidate in [
        Path("C:/Program Files/CMake/bin/cmake.exe"),
        Path("C:/Program Files (x86)/CMake/bin/cmake.exe"),
    ]:
        if candidate.exists():
            return str(candidate)
    error("CMake no encontrado.")
    print("  Instala desde https://cmake.org/download/ y agrega al PATH.")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Build en Windows → compilacion nativa
# ---------------------------------------------------------------------------

def build_windows(args: argparse.Namespace, project_root: Path) -> None:
    header("Compilacion nativa Windows x86_64")

    qt_prefix  = find_qt_windows(Path(args.qt_prefix) if args.qt_prefix else None)
    cmake_bin  = find_cmake_windows()
    build_dir  = project_root / BUILD_DIR_NAME
    build_type = "Debug" if args.debug else "Release"
    output_dir = Path(args.output_dir) if args.output_dir else project_root / "dist" / "windows"
    generator, extra_flags = detect_compiler_windows()

    print(f"  Compilador:  {generator}")
    print(f"  Qt6 prefix:  {qt_prefix}")
    print(f"  Build type:  {build_type}")
    print(f"  Salida:      {output_dir}")

    header("Configurando CMake...")
    run([
        cmake_bin,
        "-B", str(build_dir),
        f"-G{generator}",
        *extra_flags,
        f"-DCMAKE_PREFIX_PATH={qt_prefix}",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DBUILD_TESTS=OFF",
        str(project_root),
    ])

    header("Compilando...")
    run([
        cmake_bin, "--build", str(build_dir),
        "--config", build_type,
        "--parallel",
    ])

    # Localizar el .exe
    exe_src = find_exe_in_build(build_dir, build_type)
    if not exe_src:
        error(f"No se encontro {EXE_NAME} en {build_dir}")
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)
    exe_dest = output_dir / EXE_NAME
    shutil.copy2(exe_src, exe_dest)

    # windeployqt — copia las DLLs de Qt necesarias junto al .exe
    header("Copiando dependencias Qt (windeployqt)...")
    windeployqt = qt_prefix / "bin" / "windeployqt6.exe"
    if not windeployqt.exists():
        windeployqt = qt_prefix / "bin" / "windeployqt.exe"

    if windeployqt.exists():
        run([
            str(windeployqt),
            "--no-translations",
            "--no-system-d3d-compiler",
            "--no-opengl-sw",
            f"--{build_type.lower()}",
            str(exe_dest),
        ])
    else:
        warn("windeployqt no encontrado.")
        warn("El .exe puede no ejecutarse en maquinas sin Qt instalado.")
        warn(f"Busca windeployqt6.exe en {qt_prefix / 'bin'} y ejecutalo manualmente.")

    size_mb = exe_dest.stat().st_size / (1024 * 1024)
    header("Build completado")
    ok(f"{exe_dest}  ({size_mb:.1f} MB)")
    print()
    print(f"  Distribuye el contenido completo de:  {output_dir}")
    print("  (el .exe mas las DLLs copiadas por windeployqt)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compila TlacuiaGCL para Windows x86_64 desde Linux (MXE) o Windows (nativo).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="Compilar en modo Debug (por defecto: Release)",
    )
    parser.add_argument(
        "--mxe-root", metavar="PATH",
        help="[Linux] Ruta raiz de MXE. Ej: /opt/mxe  (se autodetecta si no se indica)",
    )
    parser.add_argument(
        "--qt-prefix", metavar="PATH",
        help="[Windows] Ruta prefix de Qt6. Ej: C:/Qt/6.7.2/msvc2022_64  (se autodetecta)",
    )
    parser.add_argument(
        "--output-dir", metavar="PATH",
        help="Directorio de salida del binario (por defecto: dist/windows/)",
    )
    args = parser.parse_args()

    system       = platform.system()
    machine      = platform.machine()
    project_root = Path(__file__).resolve().parent

    print(f"TlacuiaGCL — Build para Windows x86_64")
    print(f"Host detectado:    {system} {machine}")
    print(f"Raiz del proyecto: {project_root}")

    if system == "Linux":
        build_linux(args, project_root)
    elif system == "Windows":
        build_windows(args, project_root)
    else:
        error(f"Sistema operativo no soportado: {system}")
        print("  Este script funciona en Linux (cross-compile via MXE) o Windows (nativo).")
        sys.exit(1)


if __name__ == "__main__":
    main()
