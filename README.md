# Nova Perception Engine

**Real-time multi-modal AI perception system for Windows** that fuses screen activity, camera vision, and voice input to generate contextual awareness.

**Current Version:** Windows C++ Implementation (v2.0) with hybrid Python camera pipeline

---

## 🚀 Quick Start

> **👉 First time setup? See [QUICK_START.md](QUICK_START.md) for a streamlined guide!**

### ⚡ Automated Setup (Recommended)

**The easiest way to get started:**

```powershell
# Clone the repository
git clone <your-repo-url> PE
cd PE

# Run automated setup (downloads models, builds everything)
.\setup.bat
```

**What it does:**
1. ✅ Downloads Whisper model (~43MB)
2. ✅ Downloads Silero VAD model (~1.8MB)
3. ✅ Verifies third-party libraries
4. ✅ Builds whisper.cpp
5. ✅ Installs Python dependencies
6. ✅ Builds PerceptionEngine.exe

**Time:** 10-15 minutes

**See [SETUP_GUIDE.md](SETUP_GUIDE.md) for detailed setup instructions and troubleshooting.**

---

### 📋 Prerequisites

**Required:**
- **Windows 10/11** (x64)
- **Visual Studio 2022** (with C++ development tools)
- **CMake 3.20+**
- **Python 3.8+** (for camera vision client)
- **Webcam + Microphone**

**Optional (for GPU acceleration):**
- **NVIDIA GPU** (Compute Capability 6.0+, e.g., GTX 1060+, RTX series)
- **CUDA Toolkit 13.0+** (automatically detected if installed)

### 🛠️ Manual Build Instructions

If you prefer to build manually or the automated setup fails:

```bash
# 1. Download models (see SETUP_GUIDE.md)
# 2. Build whisper.cpp (see SETUP_GUIDE.md)

# 3. Navigate to windows_code directory
cd windows_code

# 4. Create build directory
mkdir build
cd build

# 5. Configure CMake
"C:\Program Files\CMake\bin\cmake.exe" .. -G "Visual Studio 17 2022" -A x64

# 6. Build the project (Release mode)
"C:\Program Files\CMake\bin\cmake.exe" --build . --config Release
```

**For complete manual setup instructions, see [SETUP_GUIDE.md](SETUP_GUIDE.md)**

### Running the System

**Start in 2 separate terminals:**

```bash
# Terminal 1: Start the C++ Perception Engine
cd windows_code\build\bin\Release
PerceptionEngine.exe

# Terminal 2: Start Python Camera Vision Client
cd windows_code
python win_camera_fastvlm_pytorch.py
```

**Access Dashboard:**
```
http://localhost:8777/dashboard
```

**Expected Output:**
```
🚀 Nova Perception Engine v2.0
✅ HTTP Server started on port 8777
✅ Screen monitoring initialized
✅ Audio pipeline initialized (Whisper base.en)
✅ Dashboard available at http://localhost:8777/dashboard

[DEBUG] Screen context updated
[DEBUG] Voice transcription: hello world
[Camera] POST /update_context received
```

---

## 📊 What It Does

### Three Perception Pipelines

1. **Screen Monitoring (C++)**
   - Tracks active window/app via Win32 API
   - Lists all running applications
   - Detects window changes in real-time
   - **Latency:** <5ms

2. **Voice Transcription (C++)**
   - Captures microphone audio via WASAPI
   - Detects speech with Silero VAD (optimized: 800ms silence threshold)
   - Transcribes with Whisper.cpp (tiny.en-q8_0 model)
   - **GPU acceleration** when NVIDIA GPU + CUDA detected
   - Async queue prevents blocking
   - **Latency:** 200-500ms (CPU) or 150-300ms (GPU, 2-3x faster)

3. **Camera Vision (Python)**
   - Analyzes physical environment via webcam
   - Generates scene descriptions using FastVLM-0.5B (PyTorch)
   - **GPU acceleration** when NVIDIA GPU detected (FP16)
   - Posts to C++ server via HTTP
   - **Latency:** 1.5-2s (GPU) or 8-12s (CPU)

### Context Fusion

- Combines all three perception sources
- Generates unified context summary
- Real-time web dashboard with live metrics
- **Update frequency:** 500ms

### Example Dashboard Output

```
Active: chrome.exe | "Google - Search Results"
Said: "show me the latest news"
Camera: "A person sitting at a desk with a laptop"

Pipeline Latency:
- Voice ASR: 0.18s
- Camera Vision: 9.2s
- Context Update: 0.03s
```

---

## 💻 System Requirements

| Component | Requirement | Notes |
|-----------|-------------|-------|
| **CPU** | Intel Core i5/i7 (4+ cores, AVX2 support) | Required |
| **RAM** | 8GB minimum, 16GB recommended | Required |
| **Storage** | ~3GB for AI models | Required |
| **GPU** | Optional: NVIDIA GPU (Compute Capability 6.0+) | 5-10x faster voice/camera |
| **CUDA** | Optional: CUDA Toolkit 13.0+ | Auto-detected if installed |
| **OS** | Windows 10/11 (x64) | Required |

---

## 📈 Performance Benchmarks

### Windows C++ Implementation (v2.0)

#### CPU-Only Mode
| Component | Latency | CPU Usage | Update Frequency |
|-----------|---------|-----------|------------------|
| Screen monitoring | <5ms | 2-3% | On window change |
| Voice ASR (Whisper) | 200-500ms | 15-25% | Continuous stream |
| Camera Vision (FastVLM) | 8-12s | 20-30% | Every 10 seconds |
| Context fusion | 20-50ms | 3-5% | Every 500ms |
| HTTP server | <10ms | 2-3% | On request |
| **Total** | - | **~45-60%** | - |

**Hardware tested:** Intel Core i7 laptop, 16GB RAM

#### GPU-Accelerated Mode (with NVIDIA GPU + CUDA)
| Component | Latency | GPU Usage | CPU Usage | Update Frequency |
|-----------|---------|-----------|-----------|------------------|
| Screen monitoring | <5ms | 0% | 2-3% | On window change |
| Voice ASR (Whisper) | **150-300ms** ⚡ | 15-25% | 5-10% | Continuous stream |
| Camera Vision (FastVLM) | **1.5-2s** ⚡ | 30-40% | 5-8% | Every 10 seconds |
| Context fusion | 20-50ms | 0% | 3-5% | Every 500ms |
| HTTP server | <10ms | 0% | 2-3% | On request |
| **Total** | - | **~50%** | **~20-30%** | - |

**Hardware tested:** Intel Core i7 laptop, NVIDIA RTX 5000 Ada, 16GB RAM, CUDA 13.0

**GPU Performance Gains:**
- Voice transcription: **2-3x faster** (500ms → 200ms)
- Camera vision: **5-6x faster** (10s → 1.7s)
- CPU usage: **50% reduction** (offloaded to GPU)

**Key Improvements over Python v1.0:**
- Screen monitoring: 155-340ms → <5ms (30-70x faster)
- Voice ASR: 100-200ms → 100-300ms (similar, better quality)
- Camera: Still Python-based (pending C++ migration)
- Total CPU: 32-48% → 45-60% (similar due to Whisper)

---

## 🐛 Troubleshooting

### Build Errors

**CMake not found:**
```bash
# Install CMake from https://cmake.org/download/
# Add to PATH: C:\Program Files\CMake\bin
```

**Visual Studio not found:**
```bash
# Install Visual Studio 2022 Community Edition
# Include "Desktop development with C++" workload
```

**Missing vcpkg dependencies:**
```bash
# vcpkg is auto-managed via CMake FetchContent
# If issues occur, delete build directory and rebuild
```

### Runtime Errors

**Port 8777 already in use:**
```bash
# Check what's using the port
netstat -ano | findstr :8777

# Kill the process
taskkill /PID <PID> /F

# Or change port in PerceptionEngine.cpp (line 86)
```

**Whisper model not found:**
```bash
# Models should be in: windows_code/models/ggml-base.en.bin
# If missing, check CMakeLists.txt download logic
# Or manually download from: https://huggingface.co/ggerganov/whisper.cpp
```

**Camera not working:**
```bash
# Check Windows camera permissions
# Settings → Privacy → Camera → Allow Python

# Try different camera index in win_camera_fastvlm_pytorch.py
cap = cv2.VideoCapture(0)  # Try 0, 1, 2...
```

**Microphone not working:**
```bash
# Check Windows microphone permissions
# Settings → Privacy → Microphone → Allow PerceptionEngine.exe

# Verify audio device in Windows Sound settings
```

### Dashboard Issues

**Dashboard not updating:**

⚠️ **Known Issue:** Intermittent mutex deadlock causes dashboard to stop updating after 5-30 minutes.

**Symptoms:**
- Dashboard shows "Last updated: X minutes ago"
- Server logs show POST requests succeeding
- CPU usage drops to idle

**Workaround:**
1. Restart PerceptionEngine.exe
2. Refresh browser (F5)

**Root Cause:** Mutex deadlock in ContextCollector between HTTP handler thread and audio callback thread. See [CLAUDE.md Section 10](CLAUDE.md#10-known-issues) for detailed analysis.

**Status:** Under investigation by engineering team.

**Dashboard shows connection failed:**
- Verify PerceptionEngine.exe is running
- Check http://localhost:8777/context returns JSON
- Clear browser cache and refresh

**Camera/Voice data missing:**
- Ensure Python camera client is running
- Check PerceptionEngine.exe logs for POST requests
- Verify no firewall blocking localhost:8777

---

## 🔧 Configuration

### Change Camera Update Frequency

```python
# win_camera_fastvlm_pytorch.py - line 164
time.sleep(10)  # Update every 10 seconds (default)
# time.sleep(15)  # Update every 15 seconds (reduce CPU usage)
```

### Change Dashboard Refresh Rate

```html
<!-- dashboard.html - JavaScript section -->
setInterval(updateContext, 500);  // 500ms (default)
// setInterval(updateContext, 1000);  // 1000ms (reduce network traffic)
```

### Use Different Whisper Model

```cmake
# CMakeLists.txt - line ~80
set(WHISPER_MODEL_URL "https://huggingface.co/.../ggml-base.en.bin")
# Options: tiny.en (40MB), base.en (140MB), small.en (460MB)
# Larger = better quality but slower
```

### Adjust Voice Detection Threshold

```cpp
// PerceptionEngine.cpp - AudioEngine initialization
// Silero VAD threshold (0.0 - 1.0, higher = less sensitive)
// Default is typically 0.5, adjust in AudioEngine if needed
```

---

## 📁 Project Structure

```
windows_code/
├── PerceptionEngine.cpp          # Main server entry point
├── ContextCollector.h/cpp        # Context fusion engine
├── WindowsAPIs.h/cpp             # Win32 API wrappers (screen monitoring)
├── AudioEngine.h/cpp             # WASAPI audio capture + Silero VAD
├── AsyncWhisperQueue.h/cpp       # Async Whisper transcription queue
├── HTTPServer.h/cpp              # HTTP server for dashboard + API
├── Json.h/cpp                    # Lightweight JSON builder
├── dashboard.html                # Web dashboard UI
├── CMakeLists.txt                # Build configuration
├── models/
│   └── ggml-base.en.bin          # Whisper model (auto-downloaded)
├── third-party/
│   └── whisper.cpp/              # Whisper.cpp library (auto-cloned)
├── win_camera_fastvlm_pytorch.py # Python camera client
└── build/                        # Build output directory
    └── bin/Release/
        ├── PerceptionEngine.exe  # Main executable
        └── dashboard.html        # Dashboard (copied during build)
```

---

## 🤖 AI Models Used

| Model | Size | Purpose | Integration | GPU Support |
|-------|------|---------|-------------|-------------|
| **Whisper tiny.en-q8_0** | ~43MB | Voice transcription | C++ (whisper.cpp) | ✅ CUDA (auto-detected) |
| **Silero VAD** | ~1.8MB | Speech detection | C++ (ONNX Runtime) | ❌ CPU only |
| **FastVLM-0.5B** | ~1GB | Camera scene description | Python (PyTorch) | ✅ CUDA (auto-detected) |

**Total disk space:** ~1.1GB

**GPU Acceleration:** Automatically uses NVIDIA GPU when CUDA Toolkit is installed
- **Voice:** Whisper.cpp with CUDA backend (2-3x faster)
- **Camera:** PyTorch with CUDA + FP16 (5-6x faster)
- **Fallback:** Gracefully falls back to CPU if GPU unavailable

---

## 📡 API Reference

### GET /context

Returns current fused context as JSON.

**Example Response:**
```json
{
  "activeApp": "chrome.exe",
  "activeWindow": "Google - Chrome",
  "apps": [
    {"name": "chrome.exe", "window": "Google - Chrome"},
    {"name": "Code.exe", "window": "VS Code"}
  ],
  "batteryPercent": 85,
  "voiceTranscription": "hello world",
  "cameraDescription": "A person sitting at desk",
  "cameraLatency": 9200,
  "voiceLatency": 180.5,
  "contextUpdateLatency": 28.3,
  "fusedContext": "Active: chrome.exe | Said: \"hello world\"",
  "lastUpdated": "2025-10-10 14:32:15"
}
```

### POST /update_context

Receive perception updates from external clients (e.g., Python camera).

**Request Body:**
```json
{
  "device": "Camera",
  "data": {
    "objects": ["A person sitting at desk with laptop"]
  }
}
```

**Response:**
```json
{
  "status": "ok"
}
```

### GET /dashboard

Serves the web dashboard UI.

---

## 🔮 Roadmap

### ✅ Completed (v2.0 - Windows C++)

- [x] C++ HTTP server with web dashboard
- [x] Screen monitoring via Win32 API
- [x] Real-time audio capture via WASAPI
- [x] Silero VAD speech detection (optimized: 800ms silence, 400ms min speech)
- [x] Whisper.cpp GPU-accelerated transcription (CUDA auto-detect + CPU fallback)
- [x] Async transcription queue (non-blocking)
- [x] Hybrid architecture (C++ + Python camera)
- [x] FastVLM camera vision with GPU acceleration (PyTorch CUDA + FP16)
- [x] Thread-safe context fusion
- [x] Real-time latency metrics

### 🔨 In Progress

- [ ] **Fix mutex deadlock** (P0 - critical issue)
  - Affects dashboard reliability
  - Requires architectural refactoring
  - See [CLAUDE.md Section 10](CLAUDE.md#10-known-issues)

### 🚀 Future Work (v3.0)

- [ ] **Full C++ camera pipeline**
  - Port FastVLM to ONNX Runtime C++
  - Expected: 1.7s → 0.5-1s latency (with GPU)
  - Eliminate Python dependency

- [ ] **System audio capture**
  - WASAPI loopback mode
  - Transcribe device playback audio (meetings, videos, etc.)

- [ ] **Lock-free context fusion**
  - Replace mutexes with atomic operations
  - Eliminate deadlock possibility
  - Improve thread safety

- [ ] **Advanced model optimizations**
  - Whisper model switching (tiny/base/small)
  - Dynamic GPU memory management
  - Multi-GPU support

---

## 📚 Documentation

- **[QUICK_START.md](QUICK_START.md)** - 🚀 Quick reference for colleagues (START HERE!)
- **[SETUP_VALIDATION.md](SETUP_VALIDATION.md)** - Comprehensive setup validation checklist
- **[README.md](README.md)** - This file (user guide with full architecture overview)
- **[CLAUDE.md](CLAUDE.md)** - Complete technical documentation for developers
  - System architecture deep-dive
  - Component implementation details
  - Build system configuration
  - API reference
  - **Known issues and debugging**
  - Performance analysis

---

## ⚠️ Known Issues

### 1. Intermittent Dashboard Deadlock (CRITICAL)

**Status:** ❌ Unfixed
**Priority:** P0
**Frequency:** 5-30 minutes of operation

**Description:** Dashboard stops updating due to mutex deadlock in ContextCollector. Server continues running but appears frozen.

**Workaround:** Restart PerceptionEngine.exe

**Full Analysis:** See [CLAUDE.md Section 10](CLAUDE.md#10-known-issues)

### 2. Camera Vision Latency

**Status:** ⚠️ By Design
**Priority:** P2

**Description:** 8-12 second latency due to PyTorch CPU inference.

**Solution:** Planned for v3.0 with C++ ONNX Runtime migration.

---

## 📝 License

Proprietary - Nova Perception Engine Team

**Third-Party Libraries:**
- **whisper.cpp**: MIT License - https://github.com/ggerganov/whisper.cpp
- **Silero VAD**: MIT License - https://github.com/snakers4/silero-vad
- **FastVLM**: Apple Research - https://github.com/apple/ml-fastvlm
- **cpp-httplib**: MIT License - https://github.com/yhirose/cpp-httplib
- **onnxruntime**: MIT License - https://github.com/microsoft/onnxruntime

---

## 🤝 Contributing

For detailed technical documentation, architecture decisions, implementation guides, and debugging information, see **[CLAUDE.md](CLAUDE.md)**.

---

**Version:** 2.0.0 (Windows C++ Implementation)
**Last Updated:** 2025-10-10
**Platform:** Windows 10/11 (x64)
**Status:** Production-ready with known deadlock issue
