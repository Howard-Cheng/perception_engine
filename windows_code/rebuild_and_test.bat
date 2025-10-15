@echo off
echo Building PerceptionEngine...
cd /d "%~dp0build"
"C:\Program Files\CMake\bin\cmake.exe" --build . --config Release --target PerceptionEngine

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful! Copying CUDA runtime DLLs...
echo.

REM Copy CUDA runtime DLLs
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0\bin\x64
copy /Y "%CUDA_PATH%\cudart64_13.dll" "bin\Release\" 2>nul && echo Copied cudart64_13.dll || echo WARNING: cudart64_13.dll not found
copy /Y "%CUDA_PATH%\cublas64_13.dll" "bin\Release\" 2>nul && echo Copied cublas64_13.dll || echo WARNING: cublas64_13.dll not found
copy /Y "%CUDA_PATH%\cublasLt64_13.dll" "bin\Release\" 2>nul && echo Copied cublasLt64_13.dll || echo WARNING: cublasLt64_13.dll not found

echo.
echo Running executable...
echo.

cd bin\Release

REM Delete old log
if exist perception_engine.log del perception_engine.log

REM Run executable
echo Running: PerceptionEngine.exe --console
PerceptionEngine.exe --console

echo.
echo Executable finished. Checking log file...
echo.

if exist perception_engine.log (
    echo === LOG FILE CONTENT ===
    type perception_engine.log
) else (
    echo ERROR: No log file was created!
)

pause
