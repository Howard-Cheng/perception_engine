@echo off
echo ============================================
echo Building whisper.cpp for Windows (x64)
echo ============================================

cd third-party\whisper.cpp

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo.
echo [1/3] Configuring CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DWHISPER_BUILD_EXAMPLES=OFF ^
    -DWHISPER_BUILD_TESTS=OFF ^
    -DBUILD_SHARED_LIBS=OFF

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

REM Build Release
echo.
echo [2/3] Building Release configuration...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [3/3] Build complete!
echo.
echo Library location: third-party\whisper.cpp\build\Release\whisper.lib
echo Header location: third-party\whisper.cpp\whisper.h
echo.

cd ..\..\..\

echo ============================================
echo Success! whisper.cpp built successfully
echo ============================================
