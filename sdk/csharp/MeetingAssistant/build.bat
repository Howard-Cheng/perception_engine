@echo off
echo ========================================
echo Building Meeting Assistant
echo ========================================
echo.

echo Step 1: Building MicrophoneMonitor.dll
echo ----------------------------------------
echo This must be built from x64 Native Tools Command Prompt
echo.

REM Check if DLL already exists
if exist "..\..\..\windows_code\build\bin\Release\MicrophoneMonitor.dll" (
    echo [✓] MicrophoneMonitor.dll already exists
    echo     Location: ..\..\..\windows_code\build\bin\Release\MicrophoneMonitor.dll
) else (
    echo [!] MicrophoneMonitor.dll NOT FOUND
    echo.
    echo Please build it first:
    echo   1. Open "x64 Native Tools Command Prompt for VS 2022"
    echo   2. cd windows_code
    echo   3. build_microphone_monitor_dll.bat
    echo.
    pause
    exit /b 1
)

echo.
echo Step 2: Building MeetingAssistant.exe
echo ----------------------------------------

dotnet build -c Release

if errorlevel 1 (
    echo.
    echo [✗] Build FAILED!
    pause
    exit /b 1
)

echo.
echo ========================================
echo [✓] Build SUCCESS!
echo ========================================
echo.
echo Output: bin\Release\net6.0-windows10.0.19041.0\MeetingAssistant.exe
echo.
echo To run:
echo   cd bin\Release\net6.0-windows10.0.19041.0
echo   .\MeetingAssistant.exe
echo.

pause
