@echo off
REM =========================================
REM Perception Engine Database - Complete Build Script
REM This script installs dependencies and builds the project
REM =========================================

setlocal enabledelayedexpansion

echo.
echo =========================================
echo  Perception Engine Database Builder
echo  Complete Build with vcpkg
echo =========================================
echo.

REM Set build configuration
set "BUILD_TYPE=Release"
if not "%1"=="" set "BUILD_TYPE=%1"

set "BUILD_DIR=build"

REM Check for vcpkg
if defined VCPKG_ROOT (
    echo [INFO] VCPKG_ROOT is set to: %VCPKG_ROOT%
    set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
    set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo [ERROR] VCPKG_ROOT environment variable not set
    echo.
    echo Please set VCPKG_ROOT to your vcpkg installation directory
    echo Example: set VCPKG_ROOT=C:\vcpkg
    echo.
    exit /b 1
)

REM Verify vcpkg exists
if not exist "%VCPKG_EXE%" (
    echo [ERROR] vcpkg.exe not found at: %VCPKG_EXE%
    exit /b 1
)

if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg.cmake not found at: %VCPKG_TOOLCHAIN%
    exit /b 1
)

echo [OK] Found vcpkg at: %VCPKG_EXE%
echo [OK] Found toolchain at: %VCPKG_TOOLCHAIN%
echo.

REM Step 1: Install dependencies
echo =========================================
echo  Step 1/3: Installing Dependencies
echo =========================================
echo.

"%VCPKG_EXE%" install --triplet x64-windows

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to install dependencies
    exit /b 1
)

echo.
echo [OK] Dependencies installed successfully
echo.

REM Step 2: Configure with CMake
echo =========================================
echo  Step 2/3: Configuring with CMake
echo =========================================
echo.

if exist "%BUILD_DIR%" (
    echo Cleaning old build directory...
    rmdir /s /q "%BUILD_DIR%"
)

cmake -B "%BUILD_DIR%" -S . ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo.
echo [OK] CMake configuration successful
echo.

REM Step 3: Build
echo =========================================
echo  Step 3/3: Building Project
echo =========================================
echo.

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE%

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [OK] Build successful
echo.

REM Summary
echo =========================================
echo  Build Complete!
echo =========================================
echo.
echo Build type: %BUILD_TYPE%
echo Build directory: %BUILD_DIR%
echo.
echo Executables:
if exist "%BUILD_DIR%\bin\%BUILD_TYPE%\perception_db_service.exe" (
    echo   [OK] Service:   %BUILD_DIR%\bin\%BUILD_TYPE%\perception_db_service.exe
)
if exist "%BUILD_DIR%\bin\%BUILD_TYPE%\basic_ingestion_example.exe" (
    echo   [OK] Example:   %BUILD_DIR%\bin\%BUILD_TYPE%\basic_ingestion_example.exe
)
if exist "%BUILD_DIR%\bin\%BUILD_TYPE%\perception_data_collector.exe" (
    echo   [OK] Collector: %BUILD_DIR%\bin\%BUILD_TYPE%\perception_data_collector.exe
)
echo.
echo To run the database service:
echo   cd %BUILD_DIR%\bin\%BUILD_TYPE%
echo   perception_db_service.exe --help
echo.

endlocal
