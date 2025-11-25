@echo off
REM ============================================================================
REM Elasticsearch 9.2.1 Development Deployment
REM Description: Deploy ES with localhost binding and security disabled
REM ============================================================================

echo ================================================================
echo      Elasticsearch 9.2.1 Development Deployment
echo ================================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0deploy_elasticsearch_dev.ps1"
