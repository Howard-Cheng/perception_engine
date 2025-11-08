@echo off
REM =========================================
REM Perception Engine Database - Complete Build Script
REM This script installs dependencies and builds the project
REM =========================================
set(ENV{VCPKG_DISABLE_SYMLINKS} "1")

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
set "LOCAL_VCPKG_DIR=%CD%\vcpkg_install"
set "LOCAL_VCPKG_EXE=%LOCAL_VCPKG_DIR%\vcpkg.exe"

if defined VCPKG_ROOT_TEST (
    echo [INFO] VCPKG_ROOT is set to: %VCPKG_ROOT%
    set "VCPKG_EXE=%VCPKG_ROOT%\vcpkg.exe"
    set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo [INFO] VCPKG_ROOT environment variable not set
    echo [INFO] Checking for local vcpkg installation...
    
    if exist "%LOCAL_VCPKG_EXE%" (
        echo [INFO] Found local vcpkg installation
        set "VCPKG_ROOT=%LOCAL_VCPKG_DIR%"
        set "VCPKG_EXE=%LOCAL_VCPKG_EXE%"
        set "VCPKG_TOOLCHAIN=%LOCAL_VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"
    ) else (
        echo [INFO] Local vcpkg not found, installing to: %LOCAL_VCPKG_DIR%
        echo.
        
        REM Clone vcpkg
        echo Cloning vcpkg repository...
        git clone https://github.com/microsoft/vcpkg.git "%LOCAL_VCPKG_DIR%"
        if !ERRORLEVEL! neq 0 (
            echo [ERROR] Failed to clone vcpkg repository
            exit /b 1
        )
        
        REM Bootstrap vcpkg
        echo Bootstrapping vcpkg...
        cd "%LOCAL_VCPKG_DIR%"
        call bootstrap-vcpkg.bat
        if !ERRORLEVEL! neq 0 (
            echo [ERROR] Failed to bootstrap vcpkg
            cd ..
            exit /b 1
        )
        cd ..
        
        echo [OK] vcpkg installed successfully
        echo.
        
        set "VCPKG_ROOT=%LOCAL_VCPKG_DIR%"
        set "VCPKG_EXE=%LOCAL_VCPKG_EXE%"
        set "VCPKG_TOOLCHAIN=%LOCAL_VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake"
    )
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

REM Fetch 7zip for proper extraction (required for duckdb)
echo Fetching 7-Zip for proper archive extraction...
"%VCPKG_EXE%" fetch 7zip
echo.

REM Check if DuckDB is installed
echo Checking for DuckDB installation...
"%VCPKG_EXE%" list duckdb | findstr /C:"duckdb" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [INFO] DuckDB not found. Installing DuckDB...
    "%VCPKG_EXE%" install duckdb --triplet x64-windows
    if !ERRORLEVEL! neq 0 (
        echo [WARNING] Failed to install DuckDB. Layer 1 DuckDB compression will be disabled.
        echo You can manually install it later with: vcpkg install duckdb
    ) else (
        echo [OK] DuckDB installed successfully
    )
) else (
    echo [OK] DuckDB is already installed
)
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