@echo off
REM Build script for MicrophoneMonitor standalone test
REM This script compiles just the test program without needing CMake

echo ========================================
echo Building MicrophoneMonitor Test
echo ========================================
echo.

REM Check if Visual Studio environment is loaded
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Visual Studio compiler not found!
    echo.
    echo Please run this from a "Developer Command Prompt for VS 2022"
    echo Or manually run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    echo.
    pause
    exit /b 1
)

echo  Visual Studio compiler found
echo.

REM Create output directory
if not exist "test_build" mkdir test_build
cd test_build

echo Compiling...
echo.

REM Compile the test program
REM Note: We need to compile all dependencies together
cl /EHsc /std:c++17 /W3 ^
   /I.. ^
   ..\test_mic_monitor.cpp ^
   ..\MicrophoneMonitor.cpp ^
   ..\MicrophoneMonitor_AudioDetection.cpp ^
   ..\Logger.cpp ^
   /Fe:test_mic_monitor.exe ^
   /link ole32.lib psapi.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo L Build FAILED!
    echo Check error messages above.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build SUCCESS!
echo ========================================
echo.
echo Executable: test_build\test_mic_monitor.exe
echo.
echo To run the test:
echo   cd test_build
echo   test_mic_monitor.exe
echo.
echo Then join a Zoom/Teams meeting to verify detection.
echo.

cd ..
pause
