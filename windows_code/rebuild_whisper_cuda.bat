@echo off
REM Rebuild whisper.cpp with CUDA support

echo ============================================
echo Rebuilding whisper.cpp with CUDA support
echo ============================================
echo.

set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0
set CudaToolkitDir=%CUDA_PATH%
set CUDA_HOME=%CUDA_PATH%
set PATH=%CUDA_PATH%\bin;%PATH%

echo CUDA_PATH: %CUDA_PATH%
echo CudaToolkitDir: %CudaToolkitDir%
echo.

cd third-party\whisper.cpp

REM Clean previous build
if exist build_cuda rmdir /s /q build_cuda
mkdir build_cuda
cd build_cuda

echo.
echo [1/3] Configuring CMake with CUDA support...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DGGML_CUDA=ON ^
    -DCUDA_ARCHITECTURES=native ^
    -DCMAKE_CUDA_COMPILER="%CUDA_PATH%\bin\nvcc.exe" ^
    -DCUDAToolkit_ROOT="%CUDA_PATH%"

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

echo.
echo [2/3] Building whisper.cpp with CUDA...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo [3/3] Verifying build...
if exist "bin\Release\whisper.dll" (
    echo SUCCESS: whisper.dll built successfully
) else (
    echo ERROR: whisper.dll not found
    pause
    exit /b 1
)

echo.
echo ============================================
echo Build complete!
echo ============================================
echo.
echo CUDA-enabled whisper.cpp is ready at:
echo   third-party\whisper.cpp\build_cuda\bin\Release\
echo.
pause
