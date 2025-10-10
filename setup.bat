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
echo   - Downloads Whisper models (~43MB)
echo   - Downloads Silero VAD model (~1.8MB)
echo   - Verifies third-party libraries
echo   - Builds whisper.cpp
echo   - Installs Python dependencies
echo   - Builds PerceptionEngine
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
) else (
    echo.
    echo ============================================================================
    echo Setup failed! Please check the errors above.
    echo ============================================================================
    echo.
)

pause
