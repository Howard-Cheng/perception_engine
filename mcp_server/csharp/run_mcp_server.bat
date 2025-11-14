@echo off
REM Perception Engine MCP Server (C# Version) - Quick Start Script
REM This script builds and runs the C# MCP Server

echo ======================================
echo Perception Engine MCP Server (C#)
echo ======================================
echo.

REM Check if dotnet is installed
where dotnet >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] .NET SDK not found!
    echo Please install .NET 8.0 SDK from: https://dotnet.microsoft.com/download/dotnet/8.0
    pause
    exit /b 1
)

REM Display .NET version
echo [INFO] .NET version:
dotnet --version
echo.

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo [INFO] Building project...
dotnet build --configuration Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build completed!
echo.
echo [INFO] Starting MCP Server...
echo [NOTE] This server uses stdio transport and should be started by Claude Desktop.
echo [NOTE] Press Ctrl+C to stop.
echo.

dotnet run --configuration Release

pause
