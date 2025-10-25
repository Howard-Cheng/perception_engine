@echo off
REM =================================================================
REM  Perception Engine Launcher (C++ + Python Camera)
REM =================================================================

REM Parse command line arguments
set SCREEN_ONLY=0
if "%1"=="--screen-only" set SCREEN_ONLY=1
if "%2"=="--screen-only" set SCREEN_ONLY=1

echo.
if %SCREEN_ONLY%==1 (
    echo ===================================================
    echo   Starting Perception Engine [SCREEN-ONLY MODE]
    echo ===================================================
    echo   Mode: Lightweight - Audio and Camera DISABLED
) else (
    echo ===================================================
    echo   Starting Perception Engine [FULL MODE]
    echo ===================================================
    echo   Mode: Screen + Audio + Camera
)
echo.

REM Change to the build directory
cd /d "%~dp0build\bin\Release"

REM Check if PerceptionEngine.exe exists
if not exist "PerceptionEngine.exe" (
    echo ERROR: PerceptionEngine.exe not found!
    echo Please build the project first using CMake.
    pause
    exit /b 1
)

REM Start PerceptionEngine in new window
if %SCREEN_ONLY%==1 (
    echo [1/1] Starting PerceptionEngine.exe in screen-only mode...
    start "PerceptionEngine [Screen-Only]" /D "%~dp0build\bin\Release" PerceptionEngine.exe --console --screen-only
) else (
    echo [1/2] Starting PerceptionEngine.exe...
    start "PerceptionEngine [Full]" /D "%~dp0build\bin\Release" PerceptionEngine.exe --console
)

REM Wait 3 seconds for server to start
echo Waiting for server to initialize...
timeout /t 3 /nobreak >nul

if %SCREEN_ONLY%==0 (
    REM Go back to windows_code directory for Python script
    cd /d "%~dp0"

    REM Debug: Show current directory
    echo Current directory: %CD%

    REM Check if Python camera script exists
    if not exist "win_camera_fastvlm_pytorch.py" (
        echo WARNING: win_camera_fastvlm_pytorch.py not found!
        echo Looking in: %CD%
        echo Camera pipeline will not be started.
        echo.
        echo To start camera manually, run:
        echo   cd windows_code
        echo   python win_camera_fastvlm_pytorch.py
        echo.
    ) else (
        REM Start Python camera client in new window
        echo [2/2] Starting Python camera pipeline...
        echo Starting: python win_camera_fastvlm_pytorch.py
        REM Use cmd /k to keep window open on error
        start "Camera Pipeline" cmd /k "cd /d ""%~dp0"" && python win_camera_fastvlm_pytorch.py || pause"
    )
)

echo.
echo ===================================================
echo   Perception Engine Started Successfully!
echo ===================================================
echo.
if %SCREEN_ONLY%==1 (
    echo   Mode: SCREEN-ONLY ^(lightweight^)
) else (
    echo   Mode: FULL ^(screen + audio + camera^)
)
echo.
echo   - PerceptionEngine: http://localhost:8777
echo   - Dashboard:        http://localhost:8777/dashboard
echo   - API:              http://localhost:8777/context
echo.
echo   Press any key to open dashboard in browser...
pause >nul

REM Open dashboard in default browser
start http://localhost:8777/dashboard

echo.
if %SCREEN_ONLY%==1 (
    echo   To stop: Close the console window
) else (
    echo   To stop: Close both console windows
)
echo.
echo   Usage:
echo     .\start_perception_engine.bat              Full mode
echo     .\start_perception_engine.bat --screen-only Screen-only mode
echo.
