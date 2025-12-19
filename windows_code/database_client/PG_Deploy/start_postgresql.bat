@echo off
REM ============================================================================
REM PostgreSQL 18.1 Startup - Development Version
REM ============================================================================

echo ================================================================
echo      Start PostgreSQL 18.1
echo ================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0start_postgresql.ps1"
