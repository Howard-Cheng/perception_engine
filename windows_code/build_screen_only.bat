@echo off
REM =================================================================
REM  Build Script for Perception Engine (with --screen-only support)
REM =================================================================
echo.
echo ===================================================
echo   Building Perception Engine
echo ===================================================
echo.

cd /d "%~dp0"

echo Building PerceptionEngine.exe...
"C:\Program Files\CMake\bin\cmake.exe" --build build --config Release --target PerceptionEngine

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ===================================================
echo   Build Successful!
echo ===================================================
echo.
echo Output location: build\bin\Release\PerceptionEngine.exe
echo.
pause
