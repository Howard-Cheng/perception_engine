@echo off
REM Start Windows Perception Engine - All Components

echo ============================================================
echo    WINDOWS PERCEPTION ENGINE - STARTUP
echo ============================================================
echo.

REM Check if Python is available
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found. Please install Python 3.9+
    pause
    exit /b 1
)

echo [INFO] Python found
echo.

REM Check if virtual environment exists
if not exist "venv\" (
    echo [INFO] Virtual environment not found. Creating...
    python -m venv venv
    echo [SUCCESS] Virtual environment created
    echo.
)

REM Activate virtual environment
echo [INFO] Activating virtual environment...
call venv\Scripts\activate.bat

REM Check if dependencies are installed
echo [INFO] Checking dependencies...
pip show onnxruntime >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARNING] Dependencies not installed
    echo [INFO] Installing requirements...
    pip install -r requirements_windows.txt
    echo [SUCCESS] Dependencies installed
    echo.
)

REM Check if models are downloaded
echo [INFO] Checking models...
if not exist "models\ocr\PP-OCRv5_server_det_infer.onnx" (
    echo [WARNING] PaddleOCR models not found
    echo [INFO] Run: python download_models_windows.py
    echo.
)

if not exist "models\whisper\ggml-tiny.en-q8_0.bin" (
    if not exist "models\vosk-model-small-en-us-0.15\" (
        echo [WARNING] Audio models not found
        echo [INFO] Run: python download_models_windows.py
        echo.
    )
)

echo.
echo ============================================================
echo    STARTING COMPONENTS
echo ============================================================
echo.
echo [INFO] Starting FastAPI Fusion Server...
echo [INFO] Open new terminal windows for perception clients
echo.
echo Available perception clients:
echo   - python win_screen_ocr.py      (Screen text extraction)
echo   - python win_camera_fastvlm_fast.py   (Camera environment)
echo   - python win_audio_asr.py       (Audio transcription)
echo.
echo Dashboard will be available at: http://127.0.0.1:8000/dashboard
echo.
echo Press Ctrl+C to stop the server
echo ============================================================
echo.

REM Start FastAPI server
python server.py

pause
