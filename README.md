# Nova Perception Engine

**Multi-modal AI perception system for Windows** that fuses screen activity, camera vision, and voice input to generate real-time contextual recommendations.

---

## 🚀 Quick Start (Windows)

### Prerequisites
- Windows 10/11
- Python 3.8+
- Webcam + Microphone
- OpenAI API key (get from https://platform.openai.com/api-keys)

### Installation

```bash
# 1. Create virtual environment (recommended)
python -m venv venv
venv\Scripts\activate

# 2. Install dependencies
pip install -r requirements_windows.txt

# 3. Download models (optional - will auto-download on first run)
python download_models_windows.py

# 4. Set OpenAI API key
set OPENAI_API_KEY=sk-your-key-here
```

### Running the System

Start in **4 separate terminals** (order matters):

```bash
# Terminal 1: Start Fusion Server
python server.py

# Terminal 2: Start Screen Monitoring + OCR
python win_screen_ocr.py

# Terminal 3: Start Camera Vision
python win_camera_vision.py

# Terminal 4: Start Voice/Microphone Transcription
python win_audio_asr.py
```

**Dashboard:** Open http://127.0.0.1:8000/dashboard in your browser

---

## 📊 What It Does

### Perception Sources
1. **Screen Monitoring** - Tracks active window/apps and extracts visible text via OCR
2. **Camera Vision** - Analyzes physical environment through webcam
3. **Voice Input** - Transcribes microphone speech in real-time

### AI Fusion
- Combines all perception signals into unified context
- Generates 3 actionable recommendations via GPT-4o-mini
- Updates in real-time on web dashboard

### Example Output
```
Context: User is coding in VS Code with Python file open,
         discussing "debug this function" via microphone,
         sitting at desk with laptop visible

Recommendations:
1. Set breakpoint at line 42 in current function
2. Check Python debugger console for error messages
3. Review function parameters for type mismatches
```

---

## 💻 System Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | Intel Core i5/i7 (4+ cores) |
| **RAM** | 8GB minimum, 16GB recommended |
| **Storage** | ~2GB for AI models |
| **GPU** | Not required (CPU-optimized) |

---

## 📈 Performance (CPU-Only)

| Component | Latency | CPU Usage | Update Frequency |
|-----------|---------|-----------|------------------|
| Screen OCR | 155-340ms | 3-5% | On window change (~3s) |
| Camera Vision | 5-10s | 15-20% | Every 10 seconds |
| Voice/Mic | 100-200ms | 10-15% | Continuous streaming |
| **Total** | - | **30-43%** | - |

**Hardware tested:** Intel Core i5/i7 laptop, 8GB+ RAM, no GPU

---

## 🐛 Troubleshooting

### Server Won't Start

```bash
# Check if OpenAI API key is set
echo %OPENAI_API_KEY%

# Set API key (same terminal as python server.py)
set OPENAI_API_KEY=sk-your-key-here

# Check if port 8000 is available
netstat -ano | findstr :8000
```

### Models Not Found

```bash
# Re-run download script
python download_models_windows.py

# Or let models auto-download on first run
```

### Camera Not Working

- Go to Windows Settings → Privacy → Camera
- Allow Python to access camera
- Try different camera index in `win_camera_vision.py` (0, 1, 2...)

### Microphone Not Working

- Go to Windows Settings → Privacy → Microphone
- Allow Python to access microphone
- Verify Vosk model exists: `models\vosk-model-small-en-us-0.15\`

### Dashboard Shows No Data

- Ensure all 4 scripts are running
- Check server logs for errors
- Verify clients are posting to http://127.0.0.1:8000/update_context

### High CPU Usage

- Close other applications
- Reduce update frequency in scripts (edit `time.sleep()` values)
- Use smaller image resolution in OCR (edit `max_width` in `win_screen_ocr.py`)

---

## 🔧 Configuration

### Change Update Frequencies

```python
# win_screen_ocr.py - line ~100
time.sleep(3)  # Update every 3 seconds (on window change)

# win_camera_vision.py - line ~163
time.sleep(10)  # Update every 10 seconds

# templates/dashboard.html - line 8
<meta http-equiv="refresh" content="3">  # Dashboard refresh rate
```

### Change AI Model

```python
# server.py - line ~140
model="gpt-4o-mini"  # Fast, cheap (default)
# model="gpt-4o"     # Slower, better quality
```

### Enable GPU Acceleration (if available)

Requires GPU with DirectML support:

```bash
# Install GPU-accelerated ONNX Runtime
pip install onnxruntime-directml

# Modify provider in win_screen_ocr.py
providers=['DmlExecutionProvider', 'CPUExecutionProvider']
```

---

## 📁 Project Structure

```
perception_engine/
├── server.py                   # FastAPI fusion server
├── win_screen_ocr.py          # Screen monitoring + OCR
├── win_camera_vision.py       # Camera vision (FastVLM)
├── win_audio_asr.py           # Voice transcription (Vosk)
├── templates/
│   └── dashboard.html         # Web dashboard UI
├── models/
│   └── vosk-model-small-en-us-0.15/  # Vosk ASR model
├── requirements_windows.txt   # Python dependencies
├── download_models_windows.py # Model downloader
└── archive/                   # Old implementations (macOS, tests)
```

---

## 🤖 AI Models Used

| Model | Size | Purpose | Download Method |
|-------|------|---------|-----------------|
| **PaddleOCR v5** | ~170MB | Screen text extraction | Auto-downloads on first run |
| **FastVLM-0.5B** | ~1GB | Camera scene description | Auto-downloads on first run |
| **Vosk tiny.en** | ~40MB | Voice transcription | Included in repo |
| **GPT-4o-mini** | Cloud API | Context fusion + recommendations | Requires API key |

**Total disk space:** ~1.2GB

---

## 📚 Documentation

- **[CLAUDE.md](CLAUDE.md)** - Complete technical documentation, architecture, and API reference

---

## 🔮 Roadmap

### ✅ Current Status (Python Prototype - v1.0)
- Screen monitoring + OCR working
- Camera vision working (CPU-optimized)
- Voice transcription working
- Fusion server + dashboard working
- Real-time recommendations working

### ❌ Not Implemented
- **System audio capture** (device playback/loopback)
  - Requires WASAPI implementation
  - Deferred to future C++ version

### 🔨 Future Work (C++ Migration - v2.0)
- 2-5x performance improvement
- System audio capture via WASAPI
- Single executable deployment
- Reduced CPU usage (30-43% → 15-25%)
- Faster OCR (155-340ms → 50-100ms)
- Faster camera vision (5-10s → 1-3s)

---

## 📝 License

Proprietary - Nova Perception Engine Team

**Third-Party Libraries:**
- PaddleOCR: Apache 2.0
- FastVLM: Apple Research
- Vosk: Apache 2.0
- OpenAI API: Commercial

---

## 🤝 Contributing

For detailed technical documentation, architecture decisions, and implementation guides, see [CLAUDE.md](CLAUDE.md).

---

**Version:** 1.0.0 (Python Prototype for Windows)
**Last Updated:** 2025-10-08
**Platform:** Windows 10/11 (x64)
