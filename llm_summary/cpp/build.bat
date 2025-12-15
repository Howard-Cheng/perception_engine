@echo off
REM Build script for LLM Summary C++ Module

echo ========================================
echo Building LLM Summary C++ Module
echo ========================================

REM Create build directory
if not exist "build" mkdir build
cd build

REM Configure CMake
echo.
echo Configuring CMake...
cmake .. ^
    -DUSE_SQLITE=ON ^
    -DUSE_DUCKDB=OFF ^
    -DBUILD_EXAMPLES=ON ^
    -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build
echo.
echo Building...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Run example with: .\build\Release\example_usage.exe
echo.

pause
