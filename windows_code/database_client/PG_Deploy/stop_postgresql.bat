@echo off
REM ============================================================================
REM PostgreSQL 18.1 Stop - Development Version
REM ============================================================================

echo ================================================================
echo      Stop PostgreSQL 18.1
echo ================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0stop_postgresql.ps1"
