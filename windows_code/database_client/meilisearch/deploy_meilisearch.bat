@echo off
REM ================================================================
REM Perception Engine - MeiliSearch Quick Deployment (Batch)
REM ================================================================

echo ========================================
echo   MeiliSearch Quick Deploy
echo ========================================
echo.

REM Check if MeiliSearch binary exists
if exist meilisearch.exe (
    echo [FOUND] MeiliSearch binary already downloaded
    echo.
) else (
    echo [DOWNLOAD] Downloading MeiliSearch...
    echo.
    echo Please download MeiliSearch manually from:
    echo   https://github.com/meilisearch/meilisearch/releases/latest/download/meilisearch-windows-amd64.exe
    echo.
    echo Save it as 'meilisearch.exe' in this directory, then run this script again.
    echo.
    pause
    exit /b 1
)

REM Stop existing MeiliSearch
echo [STOP] Stopping any existing MeiliSearch process...
taskkill /F /IM meilisearch.exe >nul 2>&1
timeout /t 2 /nobreak >nul

REM Create data directory
if not exist meili_data mkdir meili_data

REM Start MeiliSearch
echo.
echo [START] Starting MeiliSearch server...
echo.
echo Configuration:
echo   Port: 7700
echo   Environment: development
echo   Data Path: ./meili_data
echo   Master Key: perception_engine_key_2025
echo.

start "MeiliSearch" meilisearch.exe ^
    --db-path ./meili_data ^
    --env development ^
    --http-addr 127.0.0.1:7700 ^
    --master-key perception_engine_key_2025

REM Wait for startup
echo Waiting for MeiliSearch to start...
timeout /t 5 /nobreak >nul

REM Test connection
echo.
echo [TEST] Testing connection...
curl -s http://localhost:7700/health >nul 2>&1
if errorlevel 1 (
    echo   ! Connection test failed
    echo   MeiliSearch may still be starting...
) else (
    echo   ? MeiliSearch is running!
)

echo.
echo ========================================
echo   Deployment Complete!
echo ========================================
echo.
echo   Access URL: http://localhost:7700
echo   Dashboard:  Open browser to http://localhost:7700
echo   Health:     curl http://localhost:7700/health
echo.
echo   To stop: taskkill /F /IM meilisearch.exe
echo.
echo ========================================
echo.

REM Open dashboard
set /p OPEN="Open dashboard in browser? (Y/N): "
if /i "%OPEN%"=="Y" start http://localhost:7700

pause
