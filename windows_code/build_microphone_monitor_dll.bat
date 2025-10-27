@echo off
echo ========================================
echo Building MicrophoneMonitor.dll
echo ========================================
echo.

REM Check for compiler (skip architecture check - just verify cl.exe exists)
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: Visual Studio compiler not found!
    echo Please run from "Developer Command Prompt for VS 2022"
    pause
    exit /b 1
)

echo Checking for Visual Studio compiler...
echo ✓ Visual Studio compiler found
echo.

REM Create output directory if it doesn't exist
if not exist "build\bin\Release" mkdir build\bin\Release

echo Compiling MicrophoneMonitor as DLL...
echo.

REM Compile MicrophoneMonitor files
cl /c ^
  /EHsc ^
  /std:c++17 ^
  /O2 ^
  /MD ^
  /DBUILDING_DLL ^
  /I. ^
  MicrophoneMonitor.cpp ^
  MicrophoneMonitor_AudioDetection.cpp ^
  MicrophoneMonitorDLL.cpp ^
  Logger.cpp

if errorlevel 1 (
    echo.
    echo ========================================
    echo ✗ Compilation FAILED!
    echo ========================================
    pause
    exit /b 1
)

echo.
echo Linking DLL...
echo.

REM Link as DLL
link /DLL ^
  /OUT:build\bin\Release\MicrophoneMonitor.dll ^
  ole32.lib ^
  psapi.lib ^
  MicrophoneMonitor.obj ^
  MicrophoneMonitor_AudioDetection.obj ^
  MicrophoneMonitorDLL.obj ^
  Logger.obj

if errorlevel 1 (
    echo.
    echo ========================================
    echo ✗ Linking FAILED!
    echo ========================================
    pause
    exit /b 1
)

echo.
echo Cleaning up intermediate files...
del *.obj 2>nul

echo.
echo ========================================
echo ✓ Build SUCCESS!
echo ========================================
echo.
echo DLL location: build\bin\Release\MicrophoneMonitor.dll
echo.

pause
