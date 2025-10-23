# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nova Perception Engine** is a real-time multi-modal AI system that monitors user context through three perception streams: screen activity (Win32 API), voice/audio (Whisper.cpp + Silero VAD), and camera vision (FastVLM via Python). It's a hybrid C++/Python Windows application that serves context data via HTTP to a web dashboard and MCP servers.

**Architecture:** C++ backend handles screen monitoring and audio transcription with GPU acceleration support. Python client handles camera vision. Both communicate via HTTP. Dashboard polls context API every 500ms.

## Build Commands

### Initial Setup (One-Time)
```powershell
# Automated setup (downloads models, builds everything)
.\setup.bat

# Or manual setup for specific components
cd windows_code

# Build whisper.cpp with CUDA support (if NVIDIA GPU detected)
.\rebuild_whisper_cuda.bat

# Build whisper.cpp without CUDA (CPU-only)
.\build_whisper.bat
```

### Building the Main Project
```powershell
cd windows_code

# Configure CMake (first time only)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build release version
cmake --build build --config Release --target PerceptionEngine

# Quick rebuild (after code changes)
cmake --build build --config Release
```

### Running
```powershell
# Option 1: Quick start (recommended - starts both C++ and Python)
cd windows_code
.\start_perception_engine.bat

# Option 2: Manual start - C++ server only
cd windows_code\build\bin\Release
.\PerceptionEngine.exe --console

# Option 3: Manual start - with Python camera client
# Terminal 1:
cd windows_code\build\bin\Release
.\PerceptionEngine.exe --console

# Terminal 2:
cd windows_code
python win_camera_fastvlm_pytorch.py

# Dashboard accessible at http://localhost:8777/dashboard
```

### Windows Service Commands
```powershell
# Install as Windows service
.\PerceptionEngine.exe --install

# Start/stop service
.\PerceptionEngine.exe --start
.\PerceptionEngine.exe --stop

# Uninstall service
.\PerceptionEngine.exe --uninstall
```

### Testing
```powershell
# Build and run audio test
cmake --build build --config Release --target test_audio
.\build\bin\Release\test_audio.exe

# Build and run vision encoder test
cmake --build build --config Release --target test_vision_encoder
.\build\bin\Release\test_vision_encoder.exe
```

### Common Development Tasks
```powershell
# Clean rebuild after major changes
cd windows_code
rmdir /s /q build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Copy dashboard.html after editing
copy dashboard.html build\bin\Release\dashboard.html

# Kill stuck processes
taskkill /F /IM PerceptionEngine.exe
taskkill /F /IM python.exe

# Check what's using port 8777
netstat -ano | findstr :8777
```

## High-Level Architecture

### System Design
The application follows a **multi-threaded producer-consumer architecture** with three perception pipelines feeding into a central context aggregator:

```
┌─────────────────────────────────────────────────────┐
│  PerceptionEngine.exe (C++ - Port 8777)             │
│                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────┐ │
│  │ Screen       │  │ Audio        │  │ HTTP      │ │
│  │ Monitor      │  │ Pipeline     │  │ Server    │ │
│  │ (Win32 API)  │  │ (Whisper.cpp)│  │ (Winsock) │ │
│  └──────┬───────┘  └──────┬───────┘  └─────┬─────┘ │
│         │                 │                 │       │
│         └─────────────────┼─────────────────┘       │
│                           ▼                         │
│         ┌────────────────────────────────┐          │
│         │   ContextCollector             │          │
│         │   • Aggregates all sources     │          │
│         │   • Thread-safe updates        │          │
│         │   • Periodic cache refresh     │          │
│         └────────────────────────────────┘          │
└─────────────────────────────────────────────────────┘
                           ▲
                           │ HTTP POST /update_context
                           │
               ┌───────────┴──────────┐
               │  Python Camera       │
               │  (FastVLM PyTorch)   │
               │  win_camera_         │
               │  fastvlm_pytorch.py  │
               └──────────────────────┘
```

### Key Components

**PerceptionEngine.cpp** - Main entry point
- Initializes all subsystems (audio, HTTP, context collector)
- Supports console mode (`--console`) and Windows service mode
- Routes HTTP requests to appropriate handlers

**ContextCollector.cpp** - Central data fusion engine
- Aggregates screen, voice, camera, and system metrics
- Thread-safe with multiple mutexes (cacheMutex, voiceMutex, cameraMutex, metricsMutex)
- Updates cached context every 1 second
- Serves unified JSON via `/context` endpoint

**AudioCaptureEngine.cpp** - Voice transcription pipeline
- WASAPI audio capture (16kHz)
- Silero VAD for speech detection (ONNX Runtime)
- Whisper.cpp with GPU acceleration (CUDA auto-detected, CPU fallback)
- AsyncWhisperQueue for non-blocking transcription

**AsyncWhisperQueue.cpp** - Non-blocking transcription queue
- Producer-consumer pattern prevents blocking audio capture
- Background thread processes Whisper inference
- Callbacks trigger when transcription completes

**HttpServer.cpp** - HTTP server implementation
- Custom Winsock-based server (no external dependencies)
- Handles GET /context, GET /dashboard, POST /update_context
- Multi-threaded request handling

**WindowsAPIs.cpp** - System monitoring utilities
- Active window detection via Win32 API
- System metrics (CPU, memory, battery via WinRT)
- Network status

### Data Flow

1. **Screen Pipeline:** Win32 API → ContextCollector (real-time on window change)
2. **Audio Pipeline:** Microphone → WASAPI → Silero VAD → AsyncWhisperQueue → Whisper.cpp → Callback → ContextCollector
3. **Camera Pipeline:** Camera → FastVLM (Python) → HTTP POST → ContextCollector
4. **Dashboard:** Browser → HTTP GET /context (every 500ms) → ContextCollector → JSON response

### Thread Architecture

- **Main Thread:** Initializes components, handles service lifecycle
- **HTTP Server Thread:** Accepts connections, handles requests
- **Audio Capture Thread:** Continuous WASAPI audio capture
- **Whisper Processing Thread:** Background transcription (AsyncWhisperQueue)
- **Audio Polling Thread:** Polls for transcription results, triggers callbacks
- **Camera Thread:** Python process, updates every 10 seconds via HTTP POST
- **Context Update Thread:** Periodic cache update (every 1 second)

### GPU Acceleration

The system automatically detects NVIDIA GPU + CUDA Toolkit and enables GPU acceleration:

**Voice Transcription:**
- Whisper.cpp with CUDA backend
- 2-3x faster than CPU (500ms → 200ms latency)
- Automatic CPU fallback if GPU unavailable

**Camera Vision:**
- PyTorch with CUDA + FP16 (Python client)
- 5-6x faster than CPU (10s → 1.7s latency)
- Automatic CPU fallback

**Build Configuration:**
- `rebuild_whisper_cuda.bat` builds whisper.cpp with CUDA support
- CMakeLists.txt copies CUDA DLLs (cudart64_13.dll, cublas64_13.dll, etc.)
- GPU/CPU selection happens at runtime based on available hardware

## Critical Implementation Details

### Mutex Deadlock Risk in ContextCollector

**IMPORTANT:** There is a known intermittent deadlock issue in ContextCollector.cpp due to multiple threads acquiring locks in different orders. This affects dashboard reliability after 5-30 minutes of operation.

**Affected methods:**
- `CollectCurrentContext()` - Called by HTTP GET /context (locks cacheMutex → voiceMutex → metricsMutex)
- `UpdateVoiceContext()` - Called by audio callback (locks metricsMutex → voiceMutex)
- `UpdateCache()` - Called by periodic update thread (locks cacheMutex → voiceMutex → cameraMutex)

**When making changes to ContextCollector:**
- Avoid introducing new lock acquisitions
- Keep critical sections as small as possible
- Consider lock-free alternatives (std::atomic)
- Acquire locks in consistent order across all methods
- Release locks before calling external callbacks

**Future fix:** Replace multiple mutexes with lock-free design or single mutex with scoped locks.

### Whisper.cpp Integration

**GPU Detection:**
- Whisper context initialization checks for GPU availability via `use_gpu=true` parameter
- If GPU unavailable, gracefully falls back to CPU
- No code changes needed - automatic detection

**Model Loading:**
- Models located at `models/whisper/ggml-small.bin` (465MB, 99 languages, 244M params)
- Smaller models available: tiny.en (40MB), base.en (140MB)
- Larger = better quality but slower

**Audio Format:**
- 16kHz sample rate (Whisper requirement)
- PCM float32 format
- WASAPI captures 16-bit, AudioCaptureEngine converts to float32

### Python Camera Client Communication

**Protocol:** HTTP POST to `http://localhost:8777/update_context`

**Expected JSON format:**
```json
{
  "device": "Camera",
  "data": {
    "objects": ["Scene description string"]
  }
}
```

**Parsing in C++:** Manual string parsing in PerceptionEngine.cpp (lines 425-457)
- No JSON library dependency to keep executable small
- Simple substring extraction for device type and caption
- Consider using nlohmann/json for robustness if extending

**Update Frequency:** Camera client posts every 10 seconds (configurable in Python)

### Dashboard Implementation

**File:** `dashboard.html` - Single-file HTML/CSS/JavaScript
**Polling:** JavaScript setInterval() calls `/context` every 500ms
**Auto-refresh:** Updates UI without page reload

**When modifying dashboard:**
- Edit `dashboard.html` in `windows_code/`
- Rebuild to copy to `build/bin/Release/dashboard.html`
- Or manually copy: `copy dashboard.html build\bin\Release\dashboard.html`
- Hard refresh in browser: Ctrl+F5

## File Organization

```
windows_code/
├── Core Components
│   ├── PerceptionEngine.cpp/h       # Main entry point
│   ├── ContextCollector.cpp/h       # Data aggregator (⚠️ deadlock risk)
│   ├── HttpServer.cpp/h             # HTTP server
│   └── Logger.cpp/h                 # Logging system
│
├── Perception Pipelines
│   ├── AudioCaptureEngine.cpp/h     # WASAPI + Whisper
│   ├── AsyncWhisperQueue.cpp/h      # Non-blocking transcription
│   ├── SileroVAD.cpp/h             # Speech detection
│   ├── CameraVisionEngine.cpp/h     # Camera (C++ ONNX - not used)
│   └── FastVLMTokenizer.cpp/h      # Vision tokenizer (not used)
│
├── System Integration
│   ├── WindowsAPIs.cpp/h           # Win32 API wrappers
│   ├── WindowsService.cpp/h        # Windows service support
│   ├── WindowEventMonitor.cpp/h    # Window change events
│   └── BrowserContentExtractor.cpp/h # Browser text extraction
│
├── Python Components
│   └── win_camera_fastvlm_pytorch.py # Camera vision client
│
├── Build & Scripts
│   ├── CMakeLists.txt              # Build configuration
│   ├── start_perception_engine.bat # Quick start
│   ├── rebuild_whisper_cuda.bat    # Build whisper with GPU
│   ├── build_whisper.bat           # Build whisper CPU-only
│   └── copy_cuda_dlls.bat          # Copy CUDA runtime
│
└── UI & Config
    └── dashboard.html              # Web dashboard
```

### Third-Party Dependencies

**Required (included in repo):**
- `third-party/whisper.cpp/` - Speech recognition (git submodule)
- `third-party/opencv/` - Camera capture
- `third-party/onnxruntime/` - Neural network runtime
- `third-party/include/nlohmann/json.hpp` - JSON parser (header-only)

**Auto-downloaded:**
- `models/whisper/ggml-small.bin` - Whisper model (setup.bat)
- `models/vad/silero_vad.onnx` - VAD model (setup.bat)

## Python Environment

```powershell
# Install Python dependencies for camera client
pip install -r requirements_windows.txt

# Key packages:
# - torch (PyTorch with CUDA support if GPU available)
# - transformers (Hugging Face for FastVLM)
# - opencv-python (Camera capture)
# - requests (HTTP communication)
```

**Camera client location:** `windows_code/win_camera_fastvlm_pytorch.py`

**Model auto-download:** FastVLM model (~1GB) downloads on first run via Hugging Face transformers

## API Endpoints

**GET /context** - Returns unified context JSON
```json
{
  "activeApp": "chrome.exe",
  "cpuUsage": 25.3,
  "memoryUsage": 65.2,
  "battery": 85,
  "voiceTranscription": "hello world",
  "voiceLatency": 180.5,
  "cameraDescription": "A person sitting at desk",
  "cameraLatency": 9200,
  "contextUpdateLatency": 28.3,
  "RecentPeriodActiveApps": [...],
  "fusedContext": "Active: chrome.exe | Said: \"hello world\"",
  "timestamp": "2025-10-10T10:30:45.123+08:00"
}
```

**GET /dashboard** or **GET /** - Serves dashboard.html

**POST /update_context** - Receives external updates (camera client)

## MCP Server Integration

The project includes a C# MCP server that bridges Perception Engine to Claude Desktop:

**Location:** `mcp_server/csharp/`

**Build:**
```powershell
cd mcp_server\csharp
dotnet build
dotnet publish -c Release -r win-x64 --self-contained false -o ./publish
```

**Claude Desktop Config:** Edit `%APPDATA%\Claude\claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "perception-engine": {
      "command": "path\\to\\publish\\PerceptionMcpBridge.exe",
      "args": []
    }
  }
}
```

**Tool Provided:** `get_perception_context` - Returns formatted context from Perception Engine

## Common Issues and Solutions

### Dashboard Not Updating
**Symptom:** Dashboard shows stale "Last updated" timestamp
**Cause:** Mutex deadlock in ContextCollector (known issue)
**Solution:** Restart PerceptionEngine.exe
```powershell
taskkill /F /IM PerceptionEngine.exe
cd windows_code\build\bin\Release
.\PerceptionEngine.exe --console
```

### Port 8777 Already in Use
```powershell
# Find process using port
netstat -ano | findstr :8777

# Kill process (replace PID)
taskkill /PID <PID> /F
```

### Whisper Model Not Found
**Error:** "Failed to initialize audio engine"
**Solution:** Run setup.bat or manually download model
```powershell
.\setup.bat
# Or manually download to models/whisper/ggml-small.bin
```

### Build Errors After Git Pull
```powershell
# Submodules may be out of sync
git submodule update --init --recursive

# Rebuild whisper.cpp
cd windows_code
.\rebuild_whisper_cuda.bat
```

### CUDA DLLs Missing
**Error:** "ggml-cuda.dll not found" or GPU not working
**Solution:** Copy CUDA DLLs to output directory
```powershell
cd windows_code
.\copy_cuda_dlls.bat
```

## Performance Considerations

**CPU Usage:**
- Idle: 3-5%
- Active (with GPU): 20-30%
- Active (CPU-only): 45-60%

**Memory Usage:**
- Typical: 800MB - 1.2GB
- Whisper model: ~500MB
- FastVLM model: ~1GB (Python process)

**Latency Targets:**
- Screen: <5ms
- Context update: 0.5-2ms
- Voice (GPU): 200-500ms
- Voice (CPU): 4-6s
- Camera (GPU): 1.5-2s
- Camera (CPU): 8-12s

**Optimization Tips:**
- Use GPU acceleration for 2-10x speedup
- Reduce camera update frequency (10s → 15s) to save CPU
- Use smaller Whisper model (tiny.en) for faster voice at cost of accuracy
- Disable camera client if not needed

## Development Workflow

1. **Make code changes** in `windows_code/*.cpp` or `windows_code/*.h`
2. **Rebuild:** `cmake --build build --config Release`
3. **Test:** `cd build\bin\Release && PerceptionEngine.exe --console`
4. **Check logs:** Review console output or `PerceptionEngine.log`
5. **Test dashboard:** Open http://localhost:8777/dashboard
6. **Debug:** Use Visual Studio debugger or add LOG_DEBUG statements

**Hot Tips:**
- Use `--console` mode for testing (easier to see logs)
- Test with Python camera client separately before integration
- Check JSON format with `curl http://localhost:8777/context`
- Use Logger macros: LOG_INFO, LOG_DEBUG, LOG_ERROR, LOG_WARN, LOG_FATAL

## When Modifying Audio Pipeline

1. Audio format must remain 16kHz PCM (Whisper requirement)
2. Maintain AsyncWhisperQueue non-blocking design
3. Update AudioCaptureEngine::GetMetrics() if adding metrics
4. Test both GPU and CPU modes
5. Verify hallucination filtering still works

## When Modifying ContextCollector

⚠️ **Critical:** Be extremely careful with thread synchronization
- Document any new mutexes
- Acquire locks in consistent order
- Keep critical sections minimal
- Consider using std::lock() for multiple mutexes
- Test for deadlocks by running for 30+ minutes

## Version Information

**Current Version:** 2.0.0 (Windows C++ Implementation)
**Platform:** Windows 10/11 (x64)
**Compiler:** MSVC (Visual Studio 2022)
**C++ Standard:** C++17
**CMake Version:** 3.15+
