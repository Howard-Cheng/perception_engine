@echo off
echo ================================================================================
echo Building MeetingAssistant Unified C++ Project
echo ================================================================================
echo.

REM Check if CMake is available
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found in PATH
    echo Please install CMake or add it to PATH
    pause
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build
cd build

echo [Step 1/3] Configuring project with CMake...
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [Step 2/3] Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [Step 3/3] Build completed successfully!
echo.
echo Output location: build\bin\Release\
echo   - MicrophoneMonitor.dll
echo   - MeetingAssistant.exe
echo.

cd ..

echo ================================================================================
echo Next Steps:
echo ================================================================================
echo 1. Copy MSFTCore.dll and dependencies to build\bin\Release\
echo    Example:
echo    copy D:\quantum_payattention\Quantum_PayAttention\x64\Release\*.dll build\bin\Release\
echo.
echo 2. Run the application:
echo    cd build\bin\Release
echo    MeetingAssistant.exe
echo ================================================================================
echo.

pause
