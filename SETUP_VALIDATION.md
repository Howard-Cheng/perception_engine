# Setup Validation Checklist

**Purpose:** Verify that `setup.ps1` successfully sets up the Nova Perception Engine from a fresh clone.

**Last Updated:** 2025-10-10

---

## Pre-Setup Checklist

Before running `setup.ps1`, verify these prerequisites are installed:

### ✅ Required Software

1. **Windows 10/11 (64-bit)**
   ```powershell
   # Check Windows version
   winver
   # Should show Windows 10 or 11
   ```

2. **CMake 3.20+**
   ```powershell
   cmake --version
   # Should show: cmake version 3.20.x or higher
   ```
   - **If missing:** Download from https://cmake.org/download/
   - **Installation:** Select "Add CMake to system PATH" during install

3. **Visual Studio 2019/2022**
   ```powershell
   # Check if Visual Studio is installed
   "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest
   # Should show Visual Studio installation path
   ```
   - **If missing:** Install Visual Studio 2022 Community Edition
   - **Required workload:** "Desktop development with C++"

4. **Python 3.8+**
   ```powershell
   python --version
   # Should show: Python 3.8.x or higher
   ```
   - **If missing:** Download from https://python.org
   - **Installation:** Check "Add Python to PATH"

5. **Git**
   ```powershell
   git --version
   # Should show: git version 2.x.x
   ```

---

## Running Setup

### Step 1: Clone Repository

```powershell
# Clone to a clean directory
cd C:\Users\<YourUsername>\Desktop
git clone <repo-url> perception_engine
cd perception_engine
```

**Expected:** Repository clones successfully, size should be ~5-10 MB (no large binaries)

### Step 2: Verify Clean Clone

```powershell
# Check directory structure
dir

# Should see:
# - windows_code/
# - models/ (empty or missing)
# - README.md
# - CLAUDE.md
# - setup.ps1
# - .gitignore
# - requirements_windows.txt
```

**Expected:** No `models/` directory with AI models yet, no `windows_code/third-party/opencv/` or `windows_code/third-party/onnxruntime/`

### Step 3: Run Setup Script

```powershell
# Run automated setup
powershell -ExecutionPolicy Bypass -File setup.ps1
```

**Expected Output:** The script should show these steps:

```
━━━ Checking Prerequisites ━━━
✅ CMake found: cmake version 3.28.1
✅ Visual Studio found: C:\Program Files\Microsoft Visual Studio\2022\Community
✅ Python found: Python 3.11.5

━━━ Downloading Whisper Model ━━━
ℹ️  Downloading Whisper tiny.en Q8_0 model (~43MB)...
✅ Downloaded Whisper model: models\whisper\ggml-tiny.en-q8_0.bin

━━━ Downloading Silero VAD Model ━━━
ℹ️  Downloading Silero VAD model (~1.8MB)...
✅ Downloaded Silero VAD model: models\vad\silero_vad.onnx

━━━ Downloading OpenCV ━━━
ℹ️  Downloading OpenCV 4.10.0 (~120MB, this may take a few minutes)...
✅ Downloaded OpenCV installer
ℹ️  Extracting OpenCV (this may take 2-3 minutes)...
✅ OpenCV extracted successfully

━━━ Downloading ONNX Runtime ━━━
ℹ️  Downloading ONNX Runtime 1.16.3 (~15MB)...
✅ Downloaded ONNX Runtime
ℹ️  Extracting ONNX Runtime...
✅ ONNX Runtime extracted successfully

━━━ Initializing whisper.cpp ━━━
✅ whisper.cpp submodule initialized

━━━ Building whisper.cpp ━━━
ℹ️  Building whisper.cpp (this may take 5-10 minutes)...
✅ whisper.cpp built successfully

━━━ Installing Python Dependencies ━━━
✅ Python dependencies installed

━━━ Building PerceptionEngine ━━━
ℹ️  Configuring CMake...
ℹ️  Building PerceptionEngine (this may take 5-10 minutes)...
✅ PerceptionEngine built successfully!

━━━ Copying dashboard.html ━━━
✅ dashboard.html copied to build directory

━━━ Verifying Installation ━━━
✅ PerceptionEngine.exe found
✅ dashboard.html found
✅ Whisper model found (42.9MB)
✅ Silero VAD model found (1.8MB)
✅ OpenCV library found
✅ ONNX Runtime found
✅ whisper.cpp library found

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

✅ Setup Complete!

📦 All dependencies installed and project built successfully!

ℹ️  Next steps:
  1. Start the C++ server:
     cd windows_code\build\bin\Release
     .\PerceptionEngine.exe

  2. (Optional) Start Python camera client in another terminal:
     cd windows_code
     python win_camera_fastvlm_pytorch.py

  3. Open dashboard in browser:
     http://localhost:8777/dashboard

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**Expected Duration:**
- Total time: 15-25 minutes (depending on internet speed and CPU)
- Downloads: 5-10 minutes (~180 MB total)
- Compilation: 10-15 minutes (whisper.cpp + PerceptionEngine)

---

## Post-Setup Validation

### Verify File Structure

After setup completes, verify the following files exist:

```powershell
# Check AI models
dir models\whisper\ggml-tiny.en-q8_0.bin
dir models\vad\silero_vad.onnx

# Check third-party libraries
dir windows_code\third-party\opencv\opencv\build\x64\vc16\bin\opencv_world4100.dll
dir windows_code\third-party\onnxruntime\lib\onnxruntime.dll
dir windows_code\third-party\whisper.cpp\build\Release\whisper.lib

# Check built executable
dir windows_code\build\bin\Release\PerceptionEngine.exe
dir windows_code\build\bin\Release\dashboard.html
```

**Expected File Sizes:**
- `ggml-tiny.en-q8_0.bin`: ~42-43 MB
- `silero_vad.onnx`: ~1.8 MB
- `opencv_world4100.dll`: ~60 MB
- `onnxruntime.dll`: ~10-15 MB
- `whisper.lib`: ~5-10 MB
- `PerceptionEngine.exe`: ~1-2 MB

### Verify Python Dependencies

```powershell
# Check Python packages installed
python -c "import torch; import transformers; import cv2; import requests; print('All packages OK')"
# Should output: All packages OK
```

---

## Running the System

### Test 1: Start C++ Server

```powershell
cd windows_code\build\bin\Release
.\PerceptionEngine.exe
```

**Expected Output:**
```
🚀 Nova Perception Engine v2.0
📊 System Information:
   CPU Usage: 5.2%
   Memory: 8.3GB / 16.0GB (51.9%)
   Battery: 85% (Charging)
   Network: Connected (WiFi)

✅ Audio pipeline initialized
   Model: models\whisper\ggml-tiny.en-q8_0.bin
   VAD: Silero (ONNX Runtime)
   Sample Rate: 16000 Hz

✅ Context collector initialized
✅ HTTP server started on port 8777
✅ Dashboard: http://localhost:8777/dashboard

[INFO] Server is running...
```

**Expected Behavior:**
- No error messages
- Server starts on port 8777
- Screen monitoring updates appear periodically
- Voice monitoring shows transcriptions when you speak

### Test 2: Access Dashboard

Open browser and navigate to:
```
http://localhost:8777/dashboard
```

**Expected Display:**
- Dashboard loads successfully
- Shows "Active Application" card with current app
- Shows "System Health" with CPU/memory metrics
- Shows "Voice Transcription" (empty until you speak)
- Shows "Camera Vision" (empty until Python client starts)
- Dashboard updates every 500ms (watch timestamp)

**Test Voice Transcription:**
1. Speak into microphone: "Hello, this is a test"
2. Wait 5-10 seconds
3. Dashboard should show: `[DEBUG] Voice: hello this is a test` in console
4. "Voice Transcription" card should update with your text

### Test 3: Start Python Camera Client (Optional)

**Open a NEW terminal** (keep server running):

```powershell
cd windows_code
python win_camera_fastvlm_pytorch.py
```

**Expected Output:**
```
[Camera] Initializing FastVLM model...
[Camera] Model loaded: apple/FastVLM-0.5B
[Camera] Camera opened successfully
[Camera] Starting vision loop (update every 10 seconds)...
[Camera] Frame captured, generating description...
[Camera] Description: A person sitting at a desk with a laptop
[Camera] Posted to server: 200 OK
```

**Expected Behavior:**
- Camera LED turns on
- Model downloads on first run (~1GB, takes 5-10 minutes)
- After model loads, generates descriptions every 10 seconds
- Posts to C++ server successfully (200 OK)
- Dashboard "Camera Vision" card updates with descriptions

### Test 4: Context API

**Open a NEW terminal** (keep server + camera running):

```powershell
# Test /context endpoint
curl http://localhost:8777/context
```

**Expected JSON Response:**
```json
{
  "activeApp": "powershell.exe",
  "cpuUsage": 15.3,
  "memoryUsage": 52.1,
  "memoryUsedGB": 8.3,
  "totalMemoryGB": 16.0,
  "battery": 85,
  "isCharging": true,
  "networkConnected": true,
  "networkType": "WiFi",
  "voiceTranscription": "hello this is a test",
  "voiceLatency": 8234.5,
  "cameraDescription": "A person sitting at a desk with a laptop",
  "cameraLatency": 12480,
  "contextUpdateLatency": 1.23,
  "RecentPeriodActiveApps": [
    {"appName": "powershell.exe", "windowTitle": "Windows PowerShell", "durationSeconds": 120}
  ],
  "fusedContext": "Active: powershell.exe | Said: \"hello this is a test\"",
  "timestamp": "2025-10-10T14:32:15.123+08:00"
}
```

---

## Common Setup Issues

### Issue 1: TLS/SSL Download Failure

**Symptom:**
```
❌ Failed to download Whisper model: The request was aborted: Could not create SSL/TLS secure channel
```

**Fix:**
The script should automatically enable TLS 1.2 (line 31), but if it fails:
```powershell
# Manually enable TLS 1.2
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# Re-run setup
.\setup.ps1
```

### Issue 2: OpenCV Extraction Fails

**Symptom:**
```
❌ Failed to download/extract OpenCV: OpenCV DLL not found after extraction
```

**Fix:**
Manually download and extract OpenCV:
```powershell
# Download OpenCV 4.10.0
# URL: https://github.com/opencv/opencv/releases/download/4.10.0/opencv-4.10.0-windows.exe

# Run installer, extract to:
# windows_code\third-party\opencv\

# Re-run setup
.\setup.ps1
```

### Issue 3: Visual Studio Not Found

**Symptom:**
```
⚠️  Could not verify Visual Studio installation
```

**Fix:**
1. Install Visual Studio 2022 Community Edition
2. Select "Desktop development with C++" workload
3. Restart terminal and re-run `setup.ps1`

### Issue 4: whisper.cpp Build Fails

**Symptom:**
```
❌ Failed to build whisper.cpp
```

**Fix:**
1. Verify Visual Studio C++ tools are installed
2. Manually build whisper.cpp:
```powershell
cd windows_code\third-party\whisper.cpp
cmake -B build -G "Visual Studio 17 2022" -A x64 -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF
cmake --build build --config Release --target whisper
```

### Issue 5: Python Packages Fail to Install

**Symptom:**
```
⚠️  Failed to install Python dependencies
```

**Fix:**
Manually install Python packages:
```powershell
python -m pip install --upgrade pip
python -m pip install torch==2.5.1 transformers==4.46.3 opencv-python==4.10.0.84 requests==2.32.3 numpy==1.26.4 pillow==11.0.0
```

### Issue 6: Port 8777 Already in Use

**Symptom:**
```
[ERROR] Failed to bind server socket on port 8777
```

**Fix:**
```powershell
# Find what's using port 8777
netstat -ano | findstr :8777

# Kill the process (replace <PID> with actual PID)
taskkill /PID <PID> /F

# Restart PerceptionEngine.exe
```

---

## Performance Expectations

### Normal CPU/Memory Usage

| Component | CPU % | RAM (MB) | Notes |
|-----------|-------|----------|-------|
| **Idle (no speech/camera)** | 3-5% | 800 MB | Screen monitoring only |
| **With voice transcription** | 15-25% | 1 GB | During Whisper inference |
| **With camera (PyTorch)** | 20-30% | 1.2 GB | During FastVLM inference |
| **Peak (all active)** | 45-60% | 1.5 GB | Voice + camera both running |

### Latency Expectations

| Pipeline | Latency | Frequency | Notes |
|----------|---------|-----------|-------|
| Screen monitoring | <5ms | On window change | Real-time |
| Context update | 0.5-2ms | Every 1 second | Aggregation |
| Voice ASR (Whisper) | 6-15s | Per utterance | Background thread |
| Camera vision (FastVLM) | 15-35s | Every 10 seconds | CPU-only PyTorch |
| Dashboard refresh | 500ms | Continuous | HTTP polling |

---

## Success Criteria

✅ **Setup is successful if:**

1. All 10 setup steps complete without errors
2. All verification checks pass (models, libraries, executable)
3. `PerceptionEngine.exe` starts without errors
4. Dashboard loads at `http://localhost:8777/dashboard`
5. Voice transcription works (transcribes your speech within 10-15s)
6. Camera client (optional) starts and posts to server successfully
7. `/context` API returns valid JSON with all fields
8. CPU usage stays below 60% under normal load
9. No crashes or freezes during 5-minute test run

---

## Reporting Issues

If setup fails after following this checklist:

1. **Collect logs:**
   ```powershell
   # Re-run setup with output redirection
   .\setup.ps1 2>&1 | Tee-Object -FilePath setup_log.txt
   ```

2. **Document error:**
   - Which step failed?
   - Exact error message
   - Output of `setup_log.txt`
   - Windows version (`winver`)
   - CMake version (`cmake --version`)
   - Visual Studio version (`vswhere`)
   - Python version (`python --version`)

3. **Contact team with:**
   - `setup_log.txt`
   - Screenshot of error
   - System information

---

## Quick Troubleshooting Commands

```powershell
# Check if server is running
tasklist | findstr PerceptionEngine

# Check if port 8777 is listening
netstat -ano | findstr :8777

# Test API endpoint
curl http://localhost:8777/context --max-time 5

# Check Python packages
python -c "import torch; import transformers; import cv2; print('OK')"

# Re-run setup (skip Python if already installed)
.\setup.ps1 -SkipPython

# Re-run setup (skip build if already built)
.\setup.ps1 -SkipBuild
```

---

**Version:** 1.0
**Last Tested:** 2025-10-10
**Tested On:** Windows 11, Visual Studio 2022, Python 3.11
