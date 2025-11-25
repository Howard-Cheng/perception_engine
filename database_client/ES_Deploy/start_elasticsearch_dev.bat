@echo off
REM ============================================================================
REM Elasticsearch 9.2.1 Startup - Development Version
REM ============================================================================

echo ================================================================
echo      Start Elasticsearch 9.2.1
echo ================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0start_elasticsearch_dev.ps1"
