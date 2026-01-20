@echo off
REM =============================================================================
REM CMake Configuration Script for PerceptionEngine
REM =============================================================================
REM This script:
REM   1. Detects vcpkg installation
REM   2. Runs CMake to generate Visual Studio 2022 solution
REM   3. Outputs build files to "buildnew" folder
REM =============================================================================

setlocal enabledelayedexpansion

echo ============================================
echo CMake Configuration for PerceptionEngine
echo ============================================
echo.

REM Get script directory
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Set build output directory
set "BUILD_DIR=%SCRIPT_DIR%buildnew"

REM ============================================================================
REM Detect vcpkg installation
REM ============================================================================

set "VCPKG_ROOT="
set "VCPKG_EXE="

REM Check common vcpkg installation paths
if exist "C:\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=C:\vcpkg"
    set "VCPKG_EXE=C:\vcpkg\vcpkg.exe"
    goto :vcpkg_found
)

if exist "C:\tools\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=C:\tools\vcpkg"
    set "VCPKG_EXE=C:\tools\vcpkg\vcpkg.exe"
    goto :vcpkg_found
)

if exist "%USERPROFILE%\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    set "VCPKG_EXE=%USERPROFILE%\vcpkg\vcpkg.exe"
    goto :vcpkg_found
)

if exist "D:\vcpkg\vcpkg.exe" (
    set "VCPKG_ROOT=D:\vcpkg"
    set "VCPKG_EXE=D:\vcpkg\vcpkg.exe"
    goto :vcpkg_found
)

REM Try to find vcpkg.exe in PATH
where vcpkg.exe >nul 2>&1
if %errorlevel% equ 0 (
    for /f "tokens=*" %%i in ('where vcpkg.exe') do (
        set "VCPKG_EXE=%%i"
        for %%j in ("%%i\..") do set "VCPKG_ROOT=%%~fj"
        goto :vcpkg_found
    )
)

REM vcpkg not found - install locally
echo [WARNING] vcpkg not found in common locations
echo.
echo [INFO] Installing vcpkg locally to: %SCRIPT_DIR%vcpkg
echo.

set "VCPKG_ROOT=%SCRIPT_DIR%vcpkg"
set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"

if not exist "%VCPKG_ROOT%" (
    echo [INFO] Cloning vcpkg repository...
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to clone vcpkg repository!
        echo Please ensure git is installed and accessible.
        pause
        exit /b 1
    )
    echo.
)

if not exist "%VCPKG_EXE%" (
    echo [INFO] Bootstrapping vcpkg...
    pushd "%VCPKG_ROOT%"
    call bootstrap-vcpkg.bat
    popd
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to bootstrap vcpkg!
        pause
        exit /b 1
    )
    echo.
)

if not exist "%VCPKG_EXE%" (
    echo [ERROR] vcpkg.exe still not found after installation!
    pause
    exit /b 1
)

echo [OK] vcpkg installed successfully
echo.
goto :vcpkg_found

:vcpkg_found
echo [OK] vcpkg found: %VCPKG_EXE%
echo     vcpkg root: %VCPKG_ROOT%
echo.

REM Set vcpkg toolchain file
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg toolchain file not found: %VCPKG_TOOLCHAIN%
    echo Please reinstall vcpkg.
    pause
    exit /b 1
)

echo [OK] vcpkg toolchain: %VCPKG_TOOLCHAIN%
echo.

REM ============================================================================
REM Check for vcpkg.json and install dependencies
REM ============================================================================

if exist "vcpkg.json" (
    echo [INFO] vcpkg.json manifest found
    echo.
    
    if not exist "vcpkg_installed" (
        echo [INFO] Installing dependencies via vcpkg manifest mode...
        echo.
        "%VCPKG_EXE%" install --triplet x64-windows
        if !errorlevel! neq 0 (
            echo [ERROR] vcpkg install failed!
            pause
            exit /b 1
        )
        echo.
        echo [OK] Dependencies installed successfully
        echo.
    ) else (
        echo [INFO] Dependencies already installed (vcpkg_installed folder exists)
        echo       Use -Force in setup_elasticsearch_deps.ps1 to reinstall
        echo.
    )
) else (
    echo [WARNING] vcpkg.json not found - skipping manifest dependencies
    echo.
)

REM ============================================================================
REM Create build directory
REM ============================================================================

if not exist "%BUILD_DIR%" (
    echo [INFO] Creating build directory: buildnew
    mkdir "%BUILD_DIR%"
) else (
    echo [INFO] Build directory exists: buildnew
)

echo.

REM ============================================================================
REM Detect Visual Studio version
REM ============================================================================

set "VS_GENERATOR="
set "VS_VERSION="

REM Check for Visual Studio 2022 first
set "VS2022_PATH=C:\Program Files\Microsoft Visual Studio\2022"
if exist "%VS2022_PATH%" (
    set "VS_GENERATOR=Visual Studio 17 2022"
    set "VS_VERSION=2022"
    echo [OK] Visual Studio 2022 detected
    goto :vs_found
)

REM Check for Visual Studio 2026
set "VS2026_PATH=C:\Program Files\Microsoft Visual Studio\2026"
if exist "%VS2026_PATH%" (
    set "VS_GENERATOR=Visual Studio 18 2026"
    set "VS_VERSION=2026"
    echo [OK] Visual Studio 2026 detected
    goto :vs_found
)

REM Visual Studio not found
echo [ERROR] Neither Visual Studio 2022 nor 2026 found!
echo Please install Visual Studio 2022 or 2026 with C++ development tools.
pause
exit /b 1

:vs_found
echo     Using generator: !VS_GENERATOR!
echo.

REM ============================================================================
REM Run CMake configuration
REM ============================================================================

echo [INFO] Running CMake configuration...
echo.
echo CMake command:
echo   cmake -B "%BUILD_DIR%" -S . ^
echo         -G "!VS_GENERATOR!" ^
echo         -A x64 ^
echo         -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"
echo.

cmake -B "%BUILD_DIR%" -S . ^
      -G "!VS_GENERATOR!" ^
      -A x64 ^
      -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"

set CMAKE_EXIT_CODE=!errorlevel!

if "!CMAKE_EXIT_CODE!"=="0" (
    echo.
    echo ============================================
    echo CMake Configuration Successful!
    echo ============================================
    echo.
    echo Build files generated in: %BUILD_DIR%
    echo.
    echo Solution file: %BUILD_DIR%\PerceptionEngine.sln
    echo.
    echo Next steps:
    echo   1. Open the solution in Visual Studio:
    echo      start %BUILD_DIR%\PerceptionEngine.sln
    echo.
    echo   2. Or build from command line:
    echo      cmake --build %BUILD_DIR% --config Release
    echo.
    pause
    exit /b 0
) else (
    echo.
    echo [ERROR] CMake configuration failed!
    echo.
    echo Possible issues:
    echo   - Visual Studio 2022 or 2026 not installed
    echo   - Missing dependencies (whisper.lib, ONNX Runtime, OpenCV)
    echo   - Check CMakeLists.txt for required paths
    echo.
    pause
    exit /b 1
)
