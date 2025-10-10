@echo off
REM ============================================================================
REM Nova Perception Engine - Setup Launcher (Batch Wrapper)
REM ============================================================================
REM This is a simple wrapper that calls the PowerShell setup script
REM
REM Usage: Just double-click this file or run: setup.bat
REM ============================================================================

echo.
echo ============================================================================
echo Nova Perception Engine - Automated Setup
echo ============================================================================
echo.
echo This will download all required dependencies and build the project.
echo.
echo What it does:
echo   1. Downloads Whisper model (~43MB)
echo   2. Downloads Silero VAD model (~1.8MB)
echo   3. Downloads OpenCV (~120MB)
echo   4. Downloads ONNX Runtime (~15MB)
echo   5. Initializes whisper.cpp git submodule
echo   6. Builds whisper.cpp
echo   7. Installs Python dependencies
echo   8. Builds PerceptionEngine.exe
echo   9. Copies required files
echo   10. Verifies all components
echo.
echo Estimated time: 15-25 minutes
echo.
echo Press Ctrl+C to cancel or
pause

powershell -ExecutionPolicy Bypass -File "%~dp0setup.ps1"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================================
    echo Setup completed successfully!
    echo ============================================================================
    echo.
    echo Next steps:
    echo   1. Start the C++ server:
    echo      cd windows_code\build\bin\Release
    echo      PerceptionEngine.exe
    echo.
    echo   2. ^(Optional^) Start Python camera client in another terminal:
    echo      cd windows_code
    echo      python win_camera_fastvlm_pytorch.py
    echo.
    echo   3. Open dashboard in browser:
    echo      http://localhost:8777/dashboard
    echo.
    echo For detailed docs, see QUICK_START.md or SETUP_VALIDATION.md
    echo ============================================================================
) else (
    echo.
    echo ============================================================================
    echo Setup failed! Please check the errors above.
    echo ============================================================================
    echo.
    echo Troubleshooting:
    echo   - See QUICK_START.md for common issues
    echo   - See SETUP_VALIDATION.md for detailed validation
    echo.
    echo Common fixes:
    echo   - Install CMake 3.20+
    echo   - Install Visual Studio 2022 with C++ workload
    echo   - Install Python 3.8+
    echo   - Check internet connection
    echo ============================================================================
)

pause
