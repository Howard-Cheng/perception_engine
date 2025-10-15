@echo off
echo Copying CUDA runtime DLLs...

set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.0\bin
set TARGET_DIR=%~dp0build\bin\Release

echo Source: %CUDA_PATH%
echo Target: %TARGET_DIR%
echo.

REM Copy CUDA runtime DLLs
copy /Y "%CUDA_PATH%\cudart64_13.dll" "%TARGET_DIR%\" 2>nul && echo Copied cudart64_13.dll || echo WARNING: cudart64_13.dll not found
copy /Y "%CUDA_PATH%\cublas64_13.dll" "%TARGET_DIR%\" 2>nul && echo Copied cublas64_13.dll || echo WARNING: cublas64_13.dll not found
copy /Y "%CUDA_PATH%\cublasLt64_13.dll" "%TARGET_DIR%\" 2>nul && echo Copied cublasLt64_13.dll || echo WARNING: cublasLt64_13.dll not found

echo.
echo Done! CUDA runtime DLLs copied.
pause
