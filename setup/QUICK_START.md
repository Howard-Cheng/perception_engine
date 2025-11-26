# Quick Start Guide - Nova Perception Engine

**For colleagues setting up for the first time** 🚀

---

## 1️⃣ One-Line Setup (After Prerequisites)

```powershell
git clone <repo-url> perception_engine && cd perception_engine && powershell -ExecutionPolicy Bypass -File setup.ps1
```

**Wait 20-30 minutes** ☕ (downloads ~600MB + compilation)
**GPU Support:** Automatically enabled if NVIDIA GPU + CUDA Toolkit detected

---

## 2️⃣ Prerequisites (Install These First)

| Software | Required | Check | Install Link |
|----------|----------|-------|--------------|
| **CMake 3.20+** | ✅ Required | `cmake --version` | https://cmake.org/download/ |
| **Visual Studio 2022** | ✅ Required | `vswhere` | https://visualstudio.microsoft.com/downloads/ |
| **Python 3.8+** | ✅ Required | `python --version` | https://python.org |
| **Git** | ✅ Required | `git --version` | https://git-scm.com/downloads |
| **CUDA Toolkit 13.0+** | 🔷 Optional (GPU) | `nvcc --version` | https://developer.nvidia.com/cuda-downloads |

**Visual Studio:** Make sure to select "Desktop development with C++" workload during installation.
**CUDA Toolkit:** Optional - Only needed for GPU acceleration (2-10x speedup). Auto-detected during setup.

---

## 3️⃣ What Gets Downloaded

| Component | Size | Purpose |
|-----------|------|---------|
| Whisper small model | ~465 MB | Voice transcription (99 languages, 244M params) |
| Silero VAD model | ~1.8 MB | Speech detection |
| OpenCV 4.10.0 | ~120 MB | Camera capture |
| ONNX Runtime 1.16.3 | ~15 MB | Neural network inference |
| Python packages | ~1 GB | FastVLM model (first run, with CUDA support if GPU detected) |

**Total:** ~1.6 GB

**GPU Benefits:**
- Voice: 4-6s (CPU) → 200-500ms (GPU) = 10-20x faster
- Camera: 8-12s (CPU) → 1.5-2s (GPU) = 5-6x faster

---

## 4️⃣ Running the System

### Option A: Quick Start (All-In-One)

```powershell
cd windows_code
.\start_perception_engine.bat
```

Opens:
- ✅ C++ server (screen + audio)
- ✅ Python camera client
- ✅ Dashboard in browser

### Option B: Manual Start (Step-by-Step)

**Terminal 1 - C++ Server:**
```powershell
cd windows_code\build\bin\Release
.\PerceptionEngine.exe
```

**Terminal 2 - Python Camera (Optional):**
```powershell
cd windows_code
python win_camera_fastvlm_pytorch.py
```

**Browser:**
```
http://localhost:8777/dashboard
```

---

## 5️⃣ Verify It's Working

### ✅ Server Started Successfully

```
🚀 Nova Perception Engine v2.0
✅ Audio pipeline initialized
✅ HTTP server started on port 8777
✅ Dashboard: http://localhost:8777/dashboard
```

### ✅ Dashboard Shows Data

- **Active Application:** Updates when you switch windows
- **System Health:** Shows CPU/memory in real-time
- **Voice Transcription:** Appears 5-10s after you speak
- **Camera Vision:** Updates every 10s (if camera client running)

### ✅ API Works

```powershell
curl http://localhost:8777/context
# Should return JSON with all context data
```

---

## 6️⃣ Common Issues

### ❌ "CMake not found"
```powershell
# Install CMake, add to PATH
# Then restart terminal
```

### ❌ "Visual Studio not found"
```powershell
# Install Visual Studio 2022 with C++ workload
```

### ❌ "Port 8777 already in use"
```powershell
# Kill existing process
netstat -ano | findstr :8777
taskkill /PID <PID> /F
```

### ❌ "Whisper model not found"
```powershell
# Re-run setup (it will skip existing files)
.\setup.ps1
```

### ❌ Dashboard not updating
```powershell
# Restart server
taskkill /IM PerceptionEngine.exe /F
cd windows_code\build\bin\Release
.\PerceptionEngine.exe
```

**For detailed troubleshooting:** See [SETUP_VALIDATION.md](SETUP_VALIDATION.md)

---

## 7️⃣ Testing Checklist

- [ ] Setup completes without errors (all ✅ green checkmarks)
- [ ] `PerceptionEngine.exe` starts and shows "Server is running"
- [ ] Dashboard loads at http://localhost:8777/dashboard
- [ ] Dashboard shows current active application
- [ ] Voice transcription works (speak → text appears within 10-15s)
- [ ] Camera client (optional) starts and posts to server
- [ ] No crashes after 5 minutes of running

---

## 8️⃣ Performance Expectations

| What | Expected (GPU) | Expected (CPU) | Abnormal |
|------|----------------|----------------|----------|
| **Setup time** | 20-30 min | 25-35 min | >45 min (slow internet?) |
| **Server start** | <5 sec | <10 sec | >15 sec (model loading issue?) |
| **CPU usage (idle)** | 3-5% | 3-5% | >20% (check processes) |
| **CPU usage (active)** | 10-20% | 40-60% | >80% (too many apps?) |
| **GPU usage (active)** | 30-50% | N/A | >90% (other GPU apps?) |
| **RAM usage** | 800 MB - 1.2 GB | 800 MB - 1.2 GB | >2 GB (memory leak?) |
| **Voice latency** | 200-500ms | 4-6 sec | >10 sec (CPU overload?) |
| **Camera latency** | 1.5-2 sec | 8-12 sec | >20 sec (CPU overload?) |

---

## 9️⃣ Stopping the System

```powershell
# Kill server
taskkill /IM PerceptionEngine.exe /F

# Kill camera client
taskkill /IM python.exe /F

# Or just close terminals (Ctrl+C)
```

---

## 🔟 File Locations (After Setup)

```
perception_engine/
├── models/
│   ├── whisper/ggml-small.bin              ← Voice model (465MB, 99 languages)
│   └── vad/silero_vad.onnx                 ← Speech detection (1.8MB)
├── windows_code/
│   ├── third-party/
│   │   ├── opencv/                         ← Camera library
│   │   ├── onnxruntime/                    ← Neural network runtime
│   │   └── whisper.cpp/
│   │       ├── build/                      ← CPU build (if no CUDA)
│   │       └── build_cuda/                 ← GPU build (if CUDA detected)
│   └── build/bin/Release/
│       ├── PerceptionEngine.exe            ← Main executable
│       ├── dashboard.html                  ← Web UI
│       ├── whisper.dll                     ← Whisper library
│       ├── ggml-cuda.dll                   ← GPU acceleration (if CUDA)
│       └── models/whisper/ggml-small.bin   ← Model copy
├── setup.bat                               ← Quick setup launcher
├── setup.ps1                               ← Automated setup script
├── README.md                               ← User guide
├── CLAUDE.md                               ← Technical docs
├── SETUP_VALIDATION.md                     ← Full validation guide
└── QUICK_START.md                          ← This file
```

---

## 📚 Documentation

| File | Purpose |
|------|---------|
| **QUICK_START.md** | This file - Quick reference |
| **SETUP_VALIDATION.md** | Detailed validation checklist |
| **README.md** | User guide with architecture overview |
| **CLAUDE.md** | Technical documentation for developers |

---

## 🆘 Need Help?

1. **Check logs:**
   ```powershell
   # Re-run setup with logging
   .\setup.ps1 2>&1 | Tee-Object -FilePath setup_log.txt
   ```

2. **Check [SETUP_VALIDATION.md](SETUP_VALIDATION.md)** - Comprehensive troubleshooting

3. **Contact team** with:
   - Screenshot of error
   - `setup_log.txt` output
   - System info: Windows version, CMake version, Visual Studio version

---

**Version:** 1.0
**Last Updated:** 2025-10-10
**Platform:** Windows 10/11 (x64)
**Status:** Production-ready with automated setup
