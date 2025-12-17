@echo off
REM Build script for vectordb_cpp library (Windows)
REM This script configures and builds the library using CMake

setlocal enabledelayedexpansion

REM Get script directory
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Set build directory
set "BUILD_DIR=%SCRIPT_DIR%build"

REM ============================================================================
REM Detect vcpkg
REM ============================================================================

set "VCPKG_ROOT="
set "VCPKG_TOOLCHAIN="

REM Check common vcpkg installation paths (with case variations)
if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=C:\vcpkg"
    set "VCPKG_TOOLCHAIN=C:\vcpkg\scripts\buildsystems\vcpkg.cmake"
    goto :verify_toolchain
)

if exist "C:\Vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=C:\Vcpkg"
    set "VCPKG_TOOLCHAIN=C:\Vcpkg\scripts\buildsystems\vcpkg.cmake"
    goto :verify_toolchain
)

if exist "%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\vcpkg"
    set "VCPKG_TOOLCHAIN=%USERPROFILE%\vcpkg\scripts\buildsystems\vcpkg.cmake"
    goto :verify_toolchain
)

if exist "%USERPROFILE%\Vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set "VCPKG_ROOT=%USERPROFILE%\Vcpkg"
    set "VCPKG_TOOLCHAIN=%USERPROFILE%\Vcpkg\scripts\buildsystems\vcpkg.cmake"
    goto :verify_toolchain
)

REM Try to find vcpkg.exe in PATH
where vcpkg.exe >nul 2>&1
if %errorlevel% equ 0 (
    for /f "tokens=*" %%i in ('where vcpkg.exe') do (
        set "VCPKG_EXE=%%i"
        for %%j in ("%%i\..") do (
            set "VCPKG_ROOT=%%~fj"
            set "VCPKG_TOOLCHAIN=%%~fj\scripts\buildsystems\vcpkg.cmake"
            goto :verify_toolchain
        )
    )
)

echo [WARNING] vcpkg not found. Building without vcpkg toolchain.
echo           Dependencies must be installed manually.
goto :configure

:verify_toolchain
REM Verify that toolchain file actually exists
if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg toolchain file not found: %VCPKG_TOOLCHAIN%
    echo         Please check your vcpkg installation.
    echo.
    echo         Expected location: %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    echo.
    pause
    exit /b 1
)

echo [OK] vcpkg found: %VCPKG_ROOT%
echo     Toolchain: %VCPKG_TOOLCHAIN%
echo.

:configure
REM ============================================================================
REM Create build directory
REM ============================================================================

if not exist "%BUILD_DIR%" (
    echo [INFO] Creating build directory: build
    mkdir "%BUILD_DIR%"
)

echo.

REM ============================================================================
REM Run CMake configuration
REM ============================================================================

echo [INFO] Running CMake configuration...
echo.

if defined VCPKG_TOOLCHAIN (
    if exist "%VCPKG_TOOLCHAIN%" (
        cmake -B "%BUILD_DIR%" -S . ^
              -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%"
    ) else (
        echo [ERROR] vcpkg toolchain file does not exist: %VCPKG_TOOLCHAIN%
        pause
        exit /b 1
    )
) else (
    cmake -B "%BUILD_DIR%" -S .
)

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [OK] CMake configuration successful
echo.

REM ============================================================================
REM Build
REM ============================================================================

echo [INFO] Building library...
echo.

cmake --build "%BUILD_DIR%" --config Release

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ============================================
echo Build Successful!
echo ============================================
echo.
echo Library files are in: %BUILD_DIR%\lib
echo Example executable is in: %BUILD_DIR%\bin
echo.
echo To run the example:
echo   cd %BUILD_DIR%\bin
echo   vectordb_example.exe
echo.
pause
