@echo off
echo ============================================
echo Building whisper.cpp using MSBuild
echo ============================================
echo.

REM Find MSBuild path
set MSBUILD_PATH=
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" (
    set MSBUILD_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe
) else (
    echo ERROR: Visual Studio 2022 MSBuild not found!
    echo Please install Visual Studio 2022 with C++ workload
    exit /b 1
)

echo Found MSBuild: %MSBUILD_PATH%
echo.

REM Check if we need to use CMake via Visual Studio
echo Building whisper.cpp requires CMake.
echo.
echo ============================================
echo Option 1: Use Visual Studio GUI (RECOMMENDED)
echo ============================================
echo.
echo 1. Open Visual Studio 2022
echo 2. File ^> Open ^> CMake
echo 3. Navigate to: %CD%\third-party\whisper.cpp\CMakeLists.txt
echo 4. Wait for CMake configuration to complete
echo 5. Select configuration: x64-Release
echo 6. Build ^> Build All
echo 7. Output will be in: out\build\x64-Release\whisper.lib
echo.
echo ============================================
echo Option 2: Install CMake CLI
echo ============================================
echo.
echo Download from: https://cmake.org/download/
echo Or use winget: winget install Kitware.CMake
echo Then re-run: build_whisper.bat
echo.
echo ============================================
echo Option 3: Use pre-built library (QUICKEST)
echo ============================================
echo.
echo Download pre-built whisper.lib from:
echo https://github.com/ggerganov/whisper.cpp/releases
echo Place in: third-party\whisper.cpp\build\Release\
echo.

pause
