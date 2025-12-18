@echo off
REM Quick launcher for revise_database tool
REM =======================================

setlocal

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0

REM Navigate to build directory
cd "%SCRIPT_DIR%..\..\buildnew\bin\Release"

if not exist "revise_database.exe" (
    echo [ERROR] revise_database.exe not found!
    echo.
    echo Please build the tool first:
    echo   cd windows_code/buildnew
    echo   cmake --build . --target revise_database --config Release
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo  revise_database - Quick Launcher
echo ========================================
echo.
echo Select an option:
echo   1. Run with default settings
echo   2. Dry run (show what would be updated)
echo   3. Custom Elasticsearch host/port
echo   4. Show help
echo   5. Exit
echo.

set /p choice="Enter your choice (1-5): "

if "%choice%"=="1" goto run_default
if "%choice%"=="2" goto dry_run
if "%choice%"=="3" goto custom
if "%choice%"=="4" goto show_help
if "%choice%"=="5" goto end

echo Invalid choice. Exiting.
goto end

:run_default
echo.
echo Running with default settings (localhost:9200, perception_context)...
echo.
revise_database.exe
goto end

:dry_run
echo.
echo Running in dry-run mode...
echo.
revise_database.exe --dry-run
goto end

:custom
echo.
echo Custom Elasticsearch connection
echo.
set /p host="Enter Elasticsearch host [localhost]: "
if "%host%"=="" set host=localhost

set /p port="Enter Elasticsearch port [9200]: "
if "%port%"=="" set port=9200

set /p index="Enter index name [perception_context]: "
if "%index%"=="" set index=perception_context

echo.
echo Connecting to %host%:%port%, index: %index%
echo.
revise_database.exe --host %host% --port %port% --index %index%
goto end

:show_help
echo.
revise_database.exe --help
goto end

:end
echo.
pause
endlocal
