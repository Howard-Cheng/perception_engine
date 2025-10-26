@echo off
REM Build script for Meeting Detection + Audio Capture test
REM This builds a test that integrates MicrophoneMonitor with AudioCaptureEngine

echo ========================================
echo Building Meeting Capture Test
echo ========================================
echo.

REM Check if Visual Studio environment is loaded
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Visual Studio compiler not found!
    echo.
    echo Please run this from a "x64 Native Tools Command Prompt for VS 2022"
    echo Or manually run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    echo.
    pause
    exit /b 1
)

REM Check if we're in x64 mode
cl 2>&1 | findstr /C:"x64" >nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compiler is not in x64 mode!
    echo.
    echo This project requires 64-bit compilation.
    echo.
    echo Please run this from "x64 Native Tools Command Prompt for VS 2022"
    echo Or manually run: "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    echo.
    pause
    exit /b 1
)

echo ✓ Visual Studio x64 compiler found
echo.

REM Create output directory
if not exist "test_build" mkdir test_build
cd test_build

echo Compiling...
echo.

REM Compile the test program with all dependencies
REM Note: This requires many source files from the full project
cl /EHsc /std:c++17 /W3 /O2 ^
   /I.. ^
   /I..\third-party\whisper.cpp\include ^
   /I..\third-party\whisper.cpp\ggml\include ^
   /I..\third-party\onnxruntime\include ^
   ..\test_meeting_detection_and_capture.cpp ^
   ..\MicrophoneMonitor.cpp ^
   ..\MicrophoneMonitor_AudioDetection.cpp ^
   ..\AudioCaptureEngine.cpp ^
   ..\AudioCaptureEngine_MeetingMode.cpp ^
   ..\SileroVAD.cpp ^
   ..\AsyncWhisperQueue.cpp ^
   ..\Logger.cpp ^
   /Fe:test_meeting_capture.exe ^
   /link ^
   ole32.lib ^
   psapi.lib ^
   ..\third-party\whisper.cpp\build_cuda\src\Release\whisper.lib ^
   ..\third-party\onnxruntime\lib\onnxruntime.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ✗ Build FAILED!
    echo Check error messages above.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo ✓ Build SUCCESS!
echo ========================================
echo.

echo Copying required DLLs...
echo.

REM Copy ALL DLLs from build\bin directory (simplest approach)
if exist "..\build\bin\*.dll" (
    copy /Y "..\build\bin\*.dll" . >nul
    echo   ✓ Copied all DLLs from build\bin\
    echo   (whisper, ggml, onnxruntime, opencv, CUDA, etc.)
) else (
    echo   ✗ WARNING: No DLLs found in ..\build\bin\
    echo   The test may fail to run!
)

echo.
echo Executable: test_build\test_meeting_capture.exe
echo.
echo To run the test:
echo   1. Join a Teams/Zoom meeting with people speaking
echo   2. cd test_build
echo   3. test_meeting_capture.exe
echo.
echo The test will:
echo   - Detect when you join a meeting
echo   - Capture 30 seconds of audio
echo   - Transcribe using Whisper
echo   - Show you the transcript
echo.

cd ..
pause
