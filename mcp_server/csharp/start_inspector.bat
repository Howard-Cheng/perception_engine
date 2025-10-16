@echo off
REM Quick Inspector Test for C# MCP Server
REM This script quickly starts Inspector for testing

echo ========================================
echo MCP Inspector Quick Start
echo ========================================
echo.

REM Check Node.js
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] Node.js not found!
    echo Install from: https://nodejs.org/
    pause
    exit /b 1
)

echo [OK] Node.js found
echo.

REM Get script directory and navigate there
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo Starting MCP Inspector...
echo Browser will open at http://localhost:5173
echo.
echo Press Ctrl+C to stop
echo.

REM Start Inspector with npx
npx @modelcontextprotocol/inspector dotnet run --configuration Release

pause
