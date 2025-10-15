# CLAUDE.md - Nova Perception Engine Technical Documentation

**Last Updated:** 2025-10-14
**Version:** 2.0.1 - Windows C++ Production Implementation with GPU Acceleration
**Status:** Production with Known Deadlock Issue (see Section 10)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Quick Start](#quick-start)
3. [System Architecture](#system-architecture)
4. [Project Structure](#project-structure)
5. [Core Components](#core-components)
6. [Technology Stack](#technology-stack)
7. [Building from Source](#building-from-source)
8. [Performance Metrics](#performance-metrics)
9. [API Reference](#api-reference)
10. [🔴 Known Issues - CRITICAL DEADLOCK](#known-issues)
11. [Troubleshooting](#troubleshooting)
12. [Future Improvements](#future-improvements)

---

## 1. Executive Summary

**Nova Perception Engine** is a real-time multi-modal AI system that monitors and understands user context through three perception streams:

### Perception Streams
1. **Screen Activity** - Monitors active applications via Win32 API
2. **Voice/Audio** - Transcribes speech using Whisper.cpp + Silero VAD
3. **Camera Vision** - Understands environment via FastVLM (Python HTTP client)

### Architecture
- **Hybrid C++/Python Design**
  - C++ Backend: High-performance server (screen + audio + HTTP)
  - Python Client: Camera vision (PyTorch FastVLM communicates via POST)
  - Web Dashboard: Real-time HTML/JS UI polling every 500ms

### Performance

#### CPU-Only Mode
- **CPU:** 20-30% (Intel i7)
- **RAM:** 800MB-1.2GB
- **Latencies:**
  - Screen: <5ms
  - Context Update: 0.5-2ms
  - Voice ASR: 200-500ms (background)
  - Camera: 8-12s

#### GPU-Accelerated Mode (NVIDIA GPU + CUDA)
- **CPU:** 10-15% (Intel i7)
- **GPU:** 40-50% (NVIDIA RTX 5000 Ada)
- **RAM:** 800MB-1.2GB
- **Latencies:**
  - Screen: <5ms
  - Context Update: 0.5-2ms
  - Voice ASR: 150-300ms ⚡ (2-3x faster)
  - Camera: 1.5-2s ⚡ (5-6x faster)

### Key Features
- ✅ Fully local processing (privacy-focused)
- ✅ GPU acceleration with automatic CPU fallback
- ✅ Real-time dashboard
- ✅ Three parallel perception pipelines
- ✅ Optimized VAD (800ms silence threshold, 400ms min speech)
- ❌ **Intermittent deadlock** (see Known Issues)

---

## 2. Quick Start

```powershell
# Navigate to windows_code directory
cd windows_code

# Start all components
.\start_perception_engine.bat

# Dashboard opens at:
# http://localhost:8777/dashboard
```

**What `start_perception_engine.bat` does:**
1. Starts `PerceptionEngine.exe` (C++ server)
2. Starts `win_camera_fastvlm_pytorch.py` (Python camera client)
3. Both run in parallel, dashboard auto-refreshes

---

## 3. System Architecture

```
┌────────────────────────────────────────────────────────────┐
│       PerceptionEngine.exe (C++ - Port 8777)               │
│                                                             │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ Screen Mon  │  │ Audio        │  │ HTTP Server      │  │
│  │ (Win32 API) │  │ (Whisper.cpp)│  │ (Custom Winsock) │  │
│  └──────┬──────┘  └──────┬───────┘  └─────────┬────────┘  │
│         │                │                     │           │
│         └────────────────┼─────────────────────┘           │
│                          ▼                                 │
│         ┌───────────────────────────────────────┐          │
│         │   ContextCollector (Fusion Engine)    │          │
│         │  • Aggregates all sources             │          │
│         │  • Updates cache every 1 second       │          │
│         │  • Tracks performance metrics         │          │
│         │  • Thread-safe with mutexes           │          │
│         └───────────────────────────────────────┘          │
└────────────────────────────────────────────────────────────┘
                           ▲
                           │ HTTP POST /update_context
                           │ {"device": "Camera", "data": {...}}
                           │
               ┌───────────┴──────────┐
               │  Python Camera Client│
               │  (FastVLM PyTorch)   │
               │  • win_camera_       │
               │    fastvlm_pytorch.py│
               └──────────────────────┘

                           │ HTTP GET /context (every 500ms)
                           ▼
               ┌──────────────────────┐
               │  Web Dashboard       │
               │  (dashboard.html)    │
               │  • Real-time UI      │
               │  • Auto-refresh      │
               └──────────────────────┘
```

### Data Flow

1. **Screen Pipeline (C++):** Win32 API → ContextCollector
2. **Audio Pipeline (C++):** Microphone → WASAPI → Silero VAD → AsyncWhisperQueue → Whisper.cpp → ContextCollector
3. **Camera Pipeline (Python):** Camera → FastVLM → HTTP POST → ContextCollector
4. **Dashboard:** HTTP GET /context → ContextCollector → JSON → Browser

---

## 4. Project Structure

```
PE/
├── windows_code/                        # Main C++ implementation
│   ├── PerceptionEngine.cpp/h           # Main server + HTTP routing
│   ├── ContextCollector.cpp/h           # Data fusion engine (⚠️ HAS DEADLOCK)
│   ├── AudioCaptureEngine.cpp/h         # WASAPI + Whisper pipeline
│   ├── AsyncWhisperQueue.cpp/h          # Non-blocking transcription queue
│   ├── WindowsAPIs.cpp/h                # System monitoring utilities
│   ├── HttpServer.cpp/h                 # Custom HTTP server (Winsock)
│   ├── Json.cpp/h                       # Lightweight JSON builder
│   ├── dashboard.html                   # Web UI (served by HTTP server)
│   ├── win_camera_fastvlm_pytorch.py    # Python camera client
│   ├── start_perception_engine.bat      # Quick start launcher
│   ├── CMakeLists.txt                   # CMake build configuration
│   ├── models/
│   │   ├── whisper/ggml-tiny.en.bin     # Whisper Tiny English model
│   │   └── silero_vad.onnx              # Silero VAD model
│   ├── third-party/
│   │   ├── whisper.cpp/                 # Speech recognition library
│   │   └── opencv/                      # Camera capture library
│   └── build/
│       └── bin/Release/
│           ├── PerceptionEngine.exe     # Final executable
│           ├── dashboard.html           # Copied dashboard
│           ├── onnxruntime.dll          # ONNX Runtime
│           └── whisper.dll              # Whisper library
├── models/                              # Downloaded AI models (shared)
│   └── whisper/
├── .gitignore
├── requirements_windows.txt             # Python dependencies (for camera)
├── README.md                            # User-facing guide
└── CLAUDE.md                            # This file (technical docs)
```

### File Responsibilities

**PerceptionEngine.cpp** - Main entry point
- Initializes all subsystems
- Routes HTTP requests
- Coordinates C++ ↔ Python communication

**ContextCollector.cpp** - Data aggregator (⚠️ HAS DEADLOCK BUG)
- Collects screen, voice, camera, system data
- Updates cached context every 1 second
- Serves unified JSON via `/context` endpoint
- **CRITICAL ISSUE:** Mutex deadlock between threads

**AudioCaptureEngine.cpp** - Voice pipeline
- WASAPI audio capture (16kHz)
- Silero VAD for speech detection
- AsyncWhisperQueue for non-blocking transcription
- Hallucination filtering

**dashboard.html** - Web UI
- Polls `/context` every 500ms
- Displays all perception data real-time
- Shows performance metrics

**win_camera_fastvlm_pytorch.py** - Camera client
- Captures frames via OpenCV
- Generates descriptions via FastVLM (PyTorch)
- POSTs to C++ server every 10 seconds

---

## 5. Core Components

### 5.1 PerceptionEngine.cpp - Main Server

**Responsibilities:**
- Initialize subsystems (screen, audio, HTTP)
- Handle HTTP endpoints
- Coordinate perception streams

**Key Endpoints:**
- `GET /` → Redirect to /dashboard
- `GET /dashboard` → Serve dashboard.html
- `GET /context` → Return JSON with all perception data
- `POST /update_context` → Receive camera updates from Python

**Example Code:**
```cpp
// Initialize audio pipeline
audioEngine = std::make_unique<AudioCaptureEngine>();
if (!audioEngine->Initialize("models/whisper/ggml-tiny.en.bin")) {
    LogMessage("[ERROR] Failed to initialize audio engine");
    return;
}

// Set transcription callback
audioEngine->SetTranscriptionCallback([this](const std::string& transcription) {
    if (contextCollector) {
        auto metrics = audioEngine->GetMetrics();
        contextCollector->UpdateVoiceContext(transcription, metrics.whisperLatencyMs);
    }
});

// Start HTTP server on port 8777
HttpServer server(8777);
server.Start();
```

### 5.2 AudioCaptureEngine - Voice Pipeline

**Architecture:**
```
Microphone
    ↓
WASAPI Capture (16kHz PCM)
    ↓
Silero VAD (ONNX Runtime)
    ↓
Speech Detection (threshold: 0.5, silence: 800ms, min speech: 400ms)
    ↓
AsyncWhisperQueue::QueueAudio() [NON-BLOCKING]
    ↓
Background Thread: Whisper Transcription (GPU: 150-300ms | CPU: 200-500ms)
    ↓
Hallucination Filtering
    ↓
Callback → ContextCollector::UpdateVoiceContext()
```

**Key Features:**
1. **GPU Acceleration (Auto-Detected)**
   - Whisper.cpp with CUDA backend when NVIDIA GPU + CUDA Toolkit detected
   - Automatic CPU fallback if GPU unavailable
   - 2-3x faster transcription with GPU (500ms → 200ms)
   - Non-blocking design: main thread captures audio continuously
   - Background thread processes Whisper transcription
   - Uses `AsyncWhisperQueue` with producer-consumer pattern

2. **Optimized Silero VAD**
   - Detects speech vs silence
   - **800ms silence threshold** to end utterance (prevents mid-sentence cutoff)
   - **400ms minimum speech** duration (filters noise, captures short words)
   - Prevents false triggers from breathing pauses

3. **Hallucination Filtering**
   - Removes `[BLANK_AUDIO]`, `[Silence]`, etc.
   - Cleans common Whisper artifacts

**Performance:**
- Audio capture: <5ms latency
- VAD inference: 10-20ms
- Whisper transcription: 6-15s (background, non-blocking)
- Total pipeline: Real-time (audio never blocked)

**Code Example:**
```cpp
// Silero VAD
std::vector<Ort::Value> vadOutputs = vadSession.Run(/* inputs */);
float speechProb = vadOutputs[0].GetTensorData<float>()[0];

if (speechProb > 0.5f && isSilence) {
    // Speech started
    isSilence = false;
}

if (speechProb < 0.5f && !isSilence) {
    silenceDuration += frameDuration;
    if (silenceDuration >= 300ms) {
        // Utterance ended, queue for transcription
        asyncWhisperQueue->QueueAudio(speechBuffer);
        speechBuffer.clear();
    }
}

// Get result (non-blocking)
std::string result = asyncWhisperQueue->GetLatestResult();
if (!result.empty()) {
    // Trigger callback
    transcriptionCallback(result);
}
```

### 5.3 ContextCollector - Data Fusion (⚠️ HAS DEADLOCK BUG)

**Responsibilities:**
1. Aggregate data from all perception sources
2. Update cached context every 1 second
3. Serve unified JSON to dashboard
4. Track performance metrics (latencies)

**Data Sources:**
1. **Screen Context** (Win32 APIs)
   - Active application name
   - Recent app history (last 10 apps)
   - Window titles
   - Durations

2. **Voice Context** (Audio Pipeline)
   - Latest transcription
   - Voice ASR latency (ms)

3. **Camera Context** (Python POST)
   - Scene description
   - Camera inference latency (ms)

4. **System Context** (WinRT/Win32)
   - CPU usage %
   - Memory usage GB / %
   - Battery % / charging status
   - Network connectivity
   - Location (GPS if available)

**JSON Output Format:**
```json
{
  "activeApp": "chrome.exe",
  "cpuUsage": 25.3,
  "memoryUsage": 65.2,
  "memoryUsedGB": 10.4,
  "totalMemoryGB": 16.0,
  "battery": 85,
  "isCharging": false,
  "networkConnected": true,
  "networkType": "WiFi",
  "voiceTranscription": "hey this is perception engine",
  "voiceLatency": 8234.5,
  "cameraDescription": "A person sitting at a desk with a laptop",
  "cameraLatency": 25480,
  "contextUpdateLatency": 1.23,
  "RecentPeriodActiveApps": [
    {"appName": "chrome.exe", "windowTitle": "Google", "durationSeconds": 120},
    {"appName": "Code.exe", "windowTitle": "VS Code", "durationSeconds": 85}
  ],
  "fusedContext": "Active: chrome.exe | Said: \"hey this is perception engine\"",
  "timestamp": "2025-10-10T10:30:45.123+08:00"
}
```

**Update Mechanism:**
- Cache updated every 1 second via `ShouldUpdateCache()`
- Separate mutexes for different data types:
  - `cacheMutex` - Protects cached context
  - `voiceMutex` - Protects voice transcription
  - `cameraMutex` - Protects camera data
  - `metricsMutex` - Protects latency metrics
- **⚠️ DEADLOCK RISK:** Multiple threads can acquire locks in different orders

### 5.4 Python Camera Client

**File:** `win_camera_fastvlm_pytorch.py`

**Why Python?**
- FastVLM requires PyTorch (no stable C++ bindings)
- Easier model loading and inference
- Can be replaced with ONNX C++ in v3.0

**Key Implementation:**
```python
class FastVLMEngine:
    def __init__(self):
        model_id = "apple/FastVLM-0.5B"
        self.model = AutoModelForCausalLM.from_pretrained(
            model_id,
            dtype=torch.float32,  # CPU float32
            device_map="cpu",
            trust_remote_code=True
        )

    def describe_scene(self, frame):
        # Resize to 224x224 for CPU performance
        frame = cv2.resize(frame, (224, 224))

        # Generate caption
        output = self.model.generate(
            inputs=input_ids,
            images=pixel_values,
            max_new_tokens=50,      # Short captions
            do_sample=False,        # Greedy decoding
            use_cache=False         # ⚠️ CRITICAL: Prevent KV cache pollution
        )

        # POST to C++ server
        requests.post("http://127.0.0.1:8777/update_context",
            json={"device": "Camera", "data": {"objects": [description]}})
```

**Performance Optimizations:**
- Input resolution: 224x224 (balance speed vs quality)
- Max tokens: 50 (short captions only)
- Greedy decoding (no sampling for speed)
- **`use_cache=False`** - Prevents KV cache pollution that causes model degradation after ~7 frames
- Update frequency: Every 10 seconds

**KV Cache Issue:**
Without `use_cache=False`, the model accumulates attention cache across frames, causing:
- Frame 1-6: Correct descriptions
- Frame 7+: Hallucinations ("black screens", "distorted images")
- Solution: Disable cache to ensure each frame is independent

### 5.5 Dashboard (dashboard.html)

**Architecture:**
- Static HTML/CSS/JavaScript
- Polls `/context` endpoint every 500ms
- Updates UI without page refresh

**UI Cards:**
1. **Fused Context Banner** - AI-generated summary
2. **Active Application** - Current app
3. **System Health** - CPU/Memory with progress bars
4. **Power & Network** - Battery and connectivity
5. **Voice Transcription** - Latest speech
6. **Pipeline Latency** - Performance metrics (Camera, Voice, Context)
7. **Camera Vision** - Scene description
8. **Recent Applications** - App history

**JavaScript Polling:**
```javascript
async function fetchContext() {
    const response = await fetch('http://localhost:8777/context');
    const data = await response.json();
    updateDashboard(data);
}

// Poll every 500ms
setInterval(fetchContext, 500);
```

---

## 6. Technology Stack

### C++ Libraries

| Library | Purpose | Version |
|---------|---------|---------|
| **whisper.cpp** | Offline speech recognition | Latest (git submodule) |
| **onnxruntime** | Silero VAD inference | 1.16+ |
| **Windows APIs** | System monitoring | Win10/11 SDK |
| - Win32 API | Active app tracking | - |
| - WASAPI | Audio capture | - |
| - WinRT | System metrics | - |

### Python Dependencies

```txt
# Camera Vision
torch==2.5.1
transformers==4.46.3
opencv-python==4.10.0.84

# HTTP Communication
requests==2.32.3

# Utilities
numpy==1.26.4
pillow==11.0.0
```

### C++ Third-Party Libraries

**OpenCV 4.10.0** (Required for camera vision in C++)
- **Location:** `windows_code/third-party/opencv/`
- **Purpose:** Camera capture and image preprocessing
- **Usage:** Both C++ (`CameraVisionEngine.cpp`) and Python (`win_camera_fastvlm_pytorch.py`)
- **Status:** ✅ Already included in repository
- **If Missing:** Download from https://opencv.org/releases/ and extract to `third-party/opencv/`

**whisper.cpp** (Required for speech recognition)
- **Location:** `windows_code/third-party/whisper.cpp/`
- **Purpose:** CPU-optimized Whisper inference
- **Status:** Git submodule (auto-cloned)

**ONNX Runtime** (Required for Silero VAD)
- **Location:** `windows_code/third-party/onnxruntime/`
- **Purpose:** VAD model inference
- **Status:** Auto-downloaded by CMake

### Build Tools

- **CMake:** 3.20+
- **Visual Studio:** 2019+ (MSVC C++17)
- **Python:** 3.8+ (for camera client)

---

## 7. Building from Source

### ⚡ Automated Setup (Recommended)

**The easiest way to build the project:**

```powershell
# Clone repository
git clone <repo-url> PE
cd PE

# Run automated setup script
.\setup.bat
```

**What it does:**
1. Downloads Whisper model (~43MB)
2. Downloads Silero VAD model (~1.8MB)
3. Verifies third-party libraries (OpenCV, ONNX Runtime)
4. Builds whisper.cpp
5. Installs Python dependencies
6. Builds PerceptionEngine
7. Copies dashboard.html

**Time:** 10-15 minutes (depending on internet speed and CPU)

**See [SETUP_GUIDE.md](../SETUP_GUIDE.md) for troubleshooting and manual setup.**

---

### 🛠️ Manual Build Steps

If automated setup fails or you prefer manual control:

### Prerequisites

**Required:**
- Windows 10/11
- Visual Studio 2019+ with "Desktop development with C++"
- CMake 3.20+
- Python 3.8+ (for camera client)

### Build Steps

```powershell
# 1. Clone repository
cd "C:\Users\<you>\Desktop"
git clone <repo-url> PE
cd PE\windows_code

# 2. Install Python dependencies (for camera client)
pip install -r ..\requirements_windows.txt

# 3. Download models
# Whisper: Place ggml-tiny.en.bin in models/whisper/
# Silero VAD: Auto-downloads on first run
# FastVLM: Auto-downloads via Hugging Face on first camera run

# 4. Configure CMake
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# 5. Build Release
cmake --build . --config Release --target PerceptionEngine

# 6. Verify build
dir bin\Release\PerceptionEngine.exe

# 7. Copy dashboard (if not auto-copied)
copy ..\dashboard.html bin\Release\

# 8. Run
cd bin\Release
PerceptionEngine.exe
```

### Quick Build

```powershell
# From windows_code directory
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## 8. Performance Metrics

### Latencies (Windows 11, Intel i7 CPU)

| Component | Latency | Update Frequency | Notes |
|-----------|---------|------------------|-------|
| Screen monitoring | <5ms | On window change | Win32 API |
| Context update | 0.5-2ms | Every 1 second | Aggregation |
| Voice ASR (Whisper) | 6-15s | Per utterance | Background thread |
| Camera vision (FastVLM) | 15-35s | Every 10 seconds | PyTorch CPU |
| Dashboard refresh | 500ms | Continuous | HTTP polling |

### Resource Usage

| Metric | Idle | Active Transcription | Peak |
|--------|------|---------------------|------|
| CPU | 3-5% | 15-25% | 30% |
| RAM | 800MB | 1GB | 1.2GB |
| GPU | 0% (CPU-only) | 0% | 0% |

### Throughput

- **Screen updates:** Real-time (on window change)
- **Voice transcriptions:** ~4-6 per minute (depends on speech)
- **Camera updates:** 6 per minute (every 10s)
- **Dashboard refreshes:** 120 per minute (every 500ms)

---

## 9. API Reference

### GET /

**Description:** Redirects to /dashboard

**Response:** HTTP 302 Redirect

---

### GET /dashboard

**Description:** Serves the web dashboard HTML

**Response:**
- Content-Type: `text/html`
- Body: `dashboard.html` content

---

### GET /context

**Description:** Returns unified perception context as JSON

**Response:**
```json
{
  "activeApp": "string",
  "cpuUsage": float,
  "memoryUsage": float,
  "memoryUsedGB": float,
  "totalMemoryGB": float,
  "battery": int,
  "isCharging": bool,
  "networkConnected": bool,
  "networkType": "string",
  "voiceTranscription": "string" | null,
  "voiceLatency": float,
  "cameraDescription": "string" | null,
  "cameraLatency": float,
  "contextUpdateLatency": float,
  "RecentPeriodActiveApps": [
    {
      "appName": "string",
      "windowTitle": "string",
      "durationSeconds": int,
      "timestamp": "ISO8601"
    }
  ],
  "fusedContext": "string",
  "timestamp": "ISO8601"
}
```

---

### POST /update_context

**Description:** Receives perception updates from external clients (Python camera)

**Request Body:**
```json
{
  "device": "Camera",
  "data": {
    "objects": ["Scene description string"]
  }
}
```

**Response:**
```json
{
  "status": "ok"
}
```

**Status Codes:**
- 200: Success
- 400: Invalid JSON
- 500: Internal error

---

## 10. 🔴 Known Issues - CRITICAL DEADLOCK

### Issue: Intermittent Mutex Deadlock in ContextCollector

**Status:** ❌ **CRITICAL** - Unfixed
**Priority:** P0
**Frequency:** Intermittent (5-30 minutes of operation)

### Symptom

Dashboard stops updating, server appears frozen but process is still running. No crash, no error logs, just silent hang.

### Root Cause

Mutex deadlock in `ContextCollector` between multiple threads:
- **Thread A:** Dashboard GET /context
- **Thread B:** Python POST /update_context (camera)
- **Thread C:** Audio transcription callback
- **Thread D:** Periodic update thread

### Deadlock Scenario

**Thread A (GET /context):**
```
1. CollectCurrentContext() locks cacheMutex (line 215)
2. Locks voiceMutex (line 220)
3. Releases voiceMutex (line 227)
4. Locks metricsMutex (line 243)
   ↓ WAITS if Thread C holds metricsMutex
```

**Thread C (Voice callback):**
```
1. UpdateVoiceContext(text, latency) called
2. Locks metricsMutex (line 300)
3. Releases metricsMutex
4. Calls UpdateVoiceContext(text)
5. Locks voiceMutex (line 259)
   ↓ WAITS if Thread A holds voiceMutex
```

**Result:** Both threads wait for each other → Deadlock

### Affected Code

**File:** `ContextCollector.cpp`

**Lines:**
- `CollectCurrentContext()`: 215-256
- `UpdateCache()`: 33-208
- `UpdateVoiceContext(text, latency)`: 297-306
- `GenerateFusedContext()`: 317-324

### Attempted Fixes

✅ **Fix 1:** Reordered locks to avoid nesting
- Moved metricsMutex lock before voiceMutex in `UpdateVoiceContext()`
- Result: Reduced frequency but not eliminated

✅ **Fix 2:** Added `GenerateFusedContext(voiceText)` overload
- Pass voice text as parameter to avoid re-locking voiceMutex
- Result: Helped but deadlock persists

✅ **Fix 3:** Release cacheMutex before locking metricsMutex in `UpdateCache()`
- Move latency calculation outside cacheMutex scope
- Result: Still experiencing deadlock

❌ **Root issue:** Multiple lock acquisition orders across threads

### TODO for Engineer

**Recommended Solutions (in priority order):**

1. **Lock-Free Design** (Preferred)
   - Use `std::atomic` for all shared variables
   - Eliminate mutexes entirely
   - Example:
     ```cpp
     std::atomic<float> latestVoiceLatency;
     std::atomic<float> latestCameraLatency;
     std::atomic<float> latestContextUpdateLatency;
     // Strings require special handling or message queue
     ```

2. **Single Mutex Design**
   - Use one mutex for entire ContextCollector
   - Use RAII scoped locks with minimal critical sections
   - Example:
     ```cpp
     std::mutex globalMutex;
     // Lock only when reading/writing, release immediately
     ```

3. **Message Queue Pattern**
   - Producer-consumer design
   - Camera/Voice/Screen → Queue → ContextCollector consumer thread
   - No mutex contention between producers

4. **Deadlock Detection**
   - Add timeout to lock acquisition (`std::try_lock_for`)
   - Log and recover if timeout occurs
   - Example:
     ```cpp
     if (!mutex.try_lock_for(std::chrono::seconds(5))) {
         LogError("Deadlock detected!");
         // Recovery logic
     }
     ```

### Workaround

**For users:** Restart PerceptionEngine.exe when dashboard freezes

**Detection:**
```powershell
# Check if server responds
curl http://localhost:8777/context --max-time 5

# If timeout → Restart
taskkill /IM PerceptionEngine.exe /F
cd windows_code\build\bin\Release
PerceptionEngine.exe
```

### Impact

- **User Experience:** Dashboard freezes intermittently
- **Data Loss:** Perception data not collected during freeze
- **Stability:** Requires manual restart

---

## 11. Troubleshooting

### Dashboard Not Updating

**Symptoms:**
- Dashboard shows old data
- "Last updated" timestamp is stale
- No errors in browser console

**Causes & Solutions:**

1. **Server deadlocked** (most common)
   ```powershell
   # Test if server responds
   curl http://localhost:8777/context --max-time 5

   # If timeout → Deadlock, restart server
   taskkill /IM PerceptionEngine.exe /F
   .\start_perception_engine.bat
   ```

2. **Server crashed**
   ```powershell
   # Check if running
   tasklist | findstr PerceptionEngine

   # If not running → Restart
   .\start_perception_engine.bat
   ```

3. **Browser cache**
   ```
   # Hard refresh (clears cache)
   Ctrl + F5  or  Ctrl + Shift + R
   ```

### Audio Not Transcribing

**Check 1: Whisper model exists**
```powershell
dir models\whisper\ggml-tiny.en.bin
# If missing → Download from:
# https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin
```

**Check 2: Microphone permissions**
```
Settings → Privacy → Microphone → Allow apps
```

**Check 3: Check logs**
```
# Should see in console:
[AsyncQueue] Transcribed: "..." (8485ms)
[DEBUG] Voice: ...
```

**Check 4: Test microphone**
```powershell
# Windows Sound Recorder or:
python -c "import sounddevice; print(sounddevice.query_devices())"
```

### Camera Not Working

**Check 1: Python client running**
```powershell
tasklist | findstr python
# Should see python.exe running win_camera_fastvlm_pytorch.py
```

**Check 2: Camera permissions**
```
Settings → Privacy → Camera → Allow apps
```

**Check 3: Check camera index**
```python
# In win_camera_fastvlm_pytorch.py
cap = cv2.VideoCapture(0)  # Try 0, 1, 2...
```

**Check 4: Test camera**
```python
import cv2
cap = cv2.VideoCapture(0)
ret, frame = cap.read()
print(f"Camera working: {ret}")
```

### High CPU Usage

**Option 1: Reduce camera update frequency**
```python
# In win_camera_fastvlm_pytorch.py, line 159
time.sleep(15)  # Increase from 10 to 15 seconds
```

**Option 2: Disable camera**
```powershell
# Don't run win_camera_fastvlm_pytorch.py
# Only screen + audio will run
```

### Build Errors

**Error: CMake not found**
```powershell
# Install CMake 3.20+
choco install cmake
# or download from https://cmake.org/download/
```

**Error: whisper.cpp not found**
```powershell
# Initialize git submodules
cd windows_code\third-party
git submodule update --init --recursive
```

**Error: ONNX Runtime not found**
```powershell
# CMake should auto-download
# Or manually download and place in third-party/onnxruntime/
```

**Error: OpenCV not found**
```
CMake Error: opencv_world*.lib not found
# Or
fatal error C1083: Cannot open include file: 'opencv2/opencv.hpp'
```

**Solution:**
OpenCV should already be included in the repository at `windows_code/third-party/opencv/`.

If missing:
```powershell
# Download OpenCV 4.10.0 for Windows
# Visit: https://opencv.org/releases/
# Download: opencv-4.10.0-windows.exe
# Extract to: windows_code/third-party/opencv/

# Verify structure:
dir windows_code\third-party\opencv\opencv\build\x64\vc16\lib\opencv_world4100.lib
```

**Note:** The repository already contains OpenCV, so your colleague should not need to download it separately. If they're missing it, they may need to re-clone the repository or check their `.gitignore` settings.

---

## 12. Future Improvements

### v3.0 Roadmap

**Priority 1: Fix Deadlock** (CRITICAL)
- Implement lock-free design
- OR use message queue pattern
- Add deadlock detection
- Target: 100% stability

**Priority 2: ONNX Camera** (Performance)
- Port FastVLM to ONNX Runtime
- Expected improvement: 15-35s → 3-5s (5-10x faster)
- Eliminates Python dependency

**Priority 3: GPU Support** (Optional)
- CUDA acceleration for Whisper
- CUDA acceleration for FastVLM ONNX
- Expected improvement: 6-15s → 1-2s voice, 3-5s → <1s camera

**Priority 4: System Audio** (Feature)
- WASAPI loopback capture
- Transcribe device playback (YouTube, Zoom, etc.)
- Use case: Meeting transcription

**Priority 5: OCR Pipeline** (Feature)
- Screen text extraction (PaddleOCR ONNX)
- Expected latency: 50-100ms
- Use case: Reading on-screen content

### Performance Targets

| Component | Current (v2.0) | Target (v3.0) | Improvement |
|-----------|---------------|---------------|-------------|
| Camera latency | 15-35s | 3-5s | 5-10x faster |
| Voice latency | 6-15s | 2-4s | 2-3x faster |
| CPU usage | 20-30% | 10-15% | 2x reduction |
| RAM usage | 1GB | 400MB | 2.5x reduction |
| Stability | Intermittent deadlock | 100% stable | ✅ |

---

## License

**Proprietary** - Nova Perception Engine Team

### Third-Party Licenses

- **whisper.cpp:** MIT License
- **FastVLM:** Apple Research License
- **ONNX Runtime:** MIT License
- **OpenCV:** Apache 2.0 License

---

## Support

For technical issues:
1. Check [Troubleshooting](#troubleshooting)
2. Review [Known Issues](#known-issues)
3. Contact Nova Perception Engine team

**For engineers:** Focus on fixing the deadlock issue before adding new features.

---

**End of Technical Documentation**
