@echo off
REM ========================================
REM Elasticsearch Quick Deployment Script
REM ========================================

setlocal EnableDelayedExpansion

set CONTAINER_NAME=elasticsearch
set ES_PORT=9200
set IMAGE=docker.elastic.co/elasticsearch/elasticsearch:8.11.0

:MENU
cls
echo ========================================
echo   Elasticsearch Docker Management Tool
echo ========================================
echo.
echo 1. Start Elasticsearch
echo 2. Stop Elasticsearch
echo 3. Restart Elasticsearch
echo 4. View Status
echo 5. View Logs
echo 6. Test Connection
echo 7. Clean All Data
echo 8. Exit
echo.
echo ========================================
set /p choice="Please select an option (1-8): "

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto RESTART
if "%choice%"=="4" goto STATUS
if "%choice%"=="5" goto LOGS
if "%choice%"=="6" goto TEST
if "%choice%"=="7" goto CLEAN
if "%choice%"=="8" goto EXIT

echo Invalid selection!
pause
goto MENU

:START
echo.
echo ========================================
echo Starting Elasticsearch
echo ========================================
echo.

REM Check if Docker is running
docker info >nul 2>&1
if errorlevel 1 (
    echo [Error] Docker is not running, please start Docker Desktop first
    pause
    goto MENU
)

REM Set system parameters
echo [Info] Configuring system parameters...
wsl -d docker-desktop sysctl -w vm.max_map_count=262144 >nul 2>&1

REM Check if container exists
docker ps -a --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
if not errorlevel 1 (
    REM Container exists, check if running
    docker ps --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
    if not errorlevel 1 (
        echo [Warning] Elasticsearch is already running
        pause
        goto MENU
    ) else (
        echo [Info] Starting existing container...
        docker start %CONTAINER_NAME%
    )
) else (
    REM Create new container
    echo [Info] Creating new container...
    
    REM Create network
    docker network create elastic >nul 2>&1
    
    REM Run container
    docker run -d ^
        --name %CONTAINER_NAME% ^
        --net elastic ^
        -p %ES_PORT%:9200 ^
        -p 9300:9300 ^
        -e "discovery.type=single-node" ^
        -e "xpack.security.enabled=false" ^
        -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" ^
        -v elasticsearch_data:/usr/share/elasticsearch/data ^
        %IMAGE%
    
    if errorlevel 1 (
        echo [Error] Container creation failed
        pause
        goto MENU
    )
)

echo.
echo [Info] Waiting for Elasticsearch to start (about 30 seconds)...
timeout /t 30 /nobreak >nul

REM Test connection
echo [Info] Testing connection...
curl -s http://localhost:%ES_PORT% >nul 2>&1
if errorlevel 1 (
    echo [Warning] Service may not be fully started, please test manually later
) else (
    echo.
    echo [Success] Elasticsearch has started!
    echo.
    echo REST API: http://localhost:%ES_PORT%
    echo.
    echo Open http://localhost:%ES_PORT% in browser to view details
)

echo.
pause
goto MENU

:STOP
echo.
echo ========================================
echo Stopping Elasticsearch
echo ========================================
echo.

docker ps --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
if errorlevel 1 (
    echo [Info] Elasticsearch is not running
) else (
    echo [Info] Stopping container...
    docker stop %CONTAINER_NAME% >nul
    echo [Success] Elasticsearch has been stopped
)

echo.
pause
goto MENU

:RESTART
echo.
echo ========================================
echo Restarting Elasticsearch
echo ========================================
echo.

echo [Info] Stopping container...
docker stop %CONTAINER_NAME% >nul 2>&1

timeout /t 3 /nobreak >nul

echo [Info] Starting container...
docker start %CONTAINER_NAME%

echo [Info] Waiting for service to be ready...
timeout /t 20 /nobreak >nul

echo [Success] Elasticsearch has been restarted
echo.
pause
goto MENU

:STATUS
echo.
echo ========================================
echo Service Status
echo ========================================
echo.

docker ps --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
if errorlevel 1 (
    echo Elasticsearch: [Stopped]
) else (
    echo Elasticsearch: [Running]
    echo.
    echo Container Info:
    docker ps --filter "name=%CONTAINER_NAME%" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
    
    echo.
    echo Resource Usage:
    docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}" %CONTAINER_NAME%
    
    echo.
    echo Attempting to get cluster health status...
    curl -s http://localhost:%ES_PORT%/_cluster/health?pretty 2>nul
)

echo.
pause
goto MENU

:LOGS
echo.
echo ========================================
echo View Logs (Last 50 lines)
echo ========================================
echo.

docker ps -a --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
if errorlevel 1 (
    echo [Error] Container does not exist
) else (
    docker logs --tail 50 %CONTAINER_NAME%
)

echo.
pause
goto MENU

:TEST
echo.
echo ========================================
echo Test Connection
echo ========================================
echo.

docker ps --filter "name=%CONTAINER_NAME%" --format "{{.Names}}" | findstr "^%CONTAINER_NAME%$" >nul
if errorlevel 1 (
    echo [Error] Elasticsearch is not running
    echo.
    pause
    goto MENU
)

echo [Info] Testing basic connection...
curl -s http://localhost:%ES_PORT% 2>nul
if errorlevel 1 (
    echo.
    echo [Error] Connection failed
) else (
    echo.
    echo.
    echo [Success] Connection successful!
    echo.
    echo [Info] Testing cluster health...
    curl -s http://localhost:%ES_PORT%/_cluster/health?pretty 2>nul
)

echo.
pause
goto MENU

:CLEAN
echo.
echo ========================================
echo Clean All Resources
echo ========================================
echo.
echo [Warning] This will delete the container and all data!
echo.
set /p confirm="Confirm deletion? (yes/no): "

if not "%confirm%"=="yes" (
    echo [Info] Operation cancelled
    pause
    goto MENU
)

echo.
echo [Info] Stopping and removing container...
docker rm -f %CONTAINER_NAME% >nul 2>&1

echo [Info] Removing data volumes...
docker volume rm elasticsearch_data >nul 2>&1

echo [Info] Removing network...
docker network rm elastic >nul 2>&1

echo.
echo [Success] Cleanup complete!
echo.
pause
goto MENU

:EXIT
echo.
echo Thank you for using!
timeout /t 2 /nobreak >nul
exit /b 0
