@echo off
REM =================================================================
REM  Perception Engine Launcher (C++ + Python Camera)
REM =================================================================
echo.
echo ===================================================
echo   Starting Perception Engine with Camera Pipeline
echo ===================================================
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
echo [1/2] Starting PerceptionEngine.exe...
start "PerceptionEngine" /D "%~dp0build\bin\Release" PerceptionEngine.exe --console

REM Wait 3 seconds for server to start
echo [1/2] Waiting for server to initialize...
timeout /t 3 /nobreak >nul

REM Go back to windows_code directory for Python script
cd /d "%~dp0"

REM Check if Python camera script exists
if not exist "win_camera_fastvlm_pytorch.py" (
    echo ERROR: win_camera_fastvlm_pytorch.py not found!
    pause
    exit /b 1
)

REM Start Python camera client in new window
echo [2/2] Starting Python camera pipeline...
start "Camera Pipeline" python win_camera_fastvlm_pytorch.py

echo.
echo ===================================================
echo   Perception Engine Started Successfully!
echo ===================================================
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
echo   To stop: Close both console windows
echo.
