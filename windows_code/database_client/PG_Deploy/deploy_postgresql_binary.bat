@echo off
REM ============================================================================
REM PostgreSQL 18.1 Binary Deployment - Development Version
REM ============================================================================

echo ================================================================
echo      Deploy PostgreSQL 18.1 from Binary
echo ================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0deploy_postgresql_binary.ps1"
