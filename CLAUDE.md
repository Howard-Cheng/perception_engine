# CLAUDE.md

This file provides comprehensive guidance to Claude Code when working with code in this repository.

---

## Project Overview

**Nova Perception Engine** is a multi-modal perception system that fuses real-time data from three sources (screen activity + OCR, voice input, camera vision) to generate AI-powered contextual recommendations.

**Current Implementation:** Windows using Python for rapid prototyping
**Future Plan:** C++ migration for production deployment with 2-5x performance improvement

---

## Design Philosophy

- **CPU-first architecture** - All models optimized for CPU inference (no GPU required)
- **Local-first processing** - Privacy-focused, all perception runs on-device
- **Real-time streaming** - Continuous perception with minimal latency
- **Modular design** - Each perception source is an independent client
- **API-driven** - Clients POST to fusion server via HTTP

---

## System Architecture

### High-Level Overview

```
┌─────────────────────────────────────────────────────────┐
│                  Windows Perception Engine              │
│                     (Python Prototype)                  │
└─────────────────────────────────────────────────────────┘

   ┌──────────────┐  ┌───────────────┐  ┌──────────────┐
   │ Screen OCR   │  │ Camera Vision │  │  Audio ASR   │
   │ (PaddleOCR)  │  │  (FastVLM)    │  │   (Vosk)     │
   └──────┬───────┘  └───────┬───────┘  └──────┬───────┘
          │                  │                  │
          │    POST /update_context            │
          └──────────────────┼──────────────────┘
                             ▼
                  ┌────────────────────┐
                  │  FastAPI Server    │
                  │  (Fusion Engine)   │
                  │  GPT-4o-mini       │
                  └─────────┬──────────┘
                            ▼
                     [Web Dashboard]
               http://127.0.0.1:8000/dashboard
```

### Component Details

The system consists of **4 Python components**:

1. **FastAPI Fusion Server** ([server.py](server.py))
   - Central hub that receives perception data
   - Runs AI context fusion via GPT-4o-mini
   - Generates 3 actionable recommendations
   - Serves live dashboard at http://127.0.0.1:8000/dashboard

2. **Screen Monitor + OCR** ([win_screen_ocr.py](win_screen_ocr.py))
   - Monitors active window/app via Win32 API
   - Lists all running applications
   - Extracts screen text via PaddleOCR

3. **Camera Vision** ([win_camera_vision.py](win_camera_vision.py))
   - Analyzes physical environment via webcam
   - Generates scene descriptions using FastVLM-0.5B
   - Updates every 10 seconds

4. **Voice/Microphone** ([win_audio_asr.py](win_audio_asr.py))
   - Captures real-time speech transcription via Vosk
   - Continuous streaming audio processing

---

## Technology Stack

### Core Perception Models (CPU-Optimized)

| Component | Technology | Latency | Update Frequency |
|-----------|-----------|---------|------------------|
| **Screen Monitor** | Win32 API | <5ms | On window change |
| **Screen OCR** | PaddleOCR v5 (ONNX) | 155-340ms | On window change (~3s) |
| **Camera Vision** | FastVLM-0.5B (PyTorch) | 5-10s | Every 10 seconds |
| **Voice/Mic** | Vosk (offline ASR) | 100-200ms | Continuous stream |
| **AI Fusion** | OpenAI GPT-4o-mini | 300-500ms | On perception update |

### Python Dependencies

```txt
# Web Server
fastapi==0.115.5
uvicorn==0.32.1
jinja2==3.1.4

# AI/ML
openai==1.54.5
paddleocr==2.9.1
paddlepaddle==3.0.0b2
torch==2.5.1
transformers==4.46.3
vosk==0.3.45

# Media Processing
opencv-python==4.10.0.84
sounddevice==0.5.1
pillow==11.0.0

# Windows APIs
pywin32==308
psutil==6.1.0

# Utilities
requests==2.32.3
numpy==1.26.4
```

---

## File Structure

### Production Files (Windows Python Implementation)

```
perception_engine/
├── server.py                        # FastAPI fusion server
├── win_screen_ocr.py               # Screen monitoring + OCR client
├── win_camera_vision.py            # Camera vision client
├── win_audio_asr.py                # Voice/microphone client
├── templates/
│   └── dashboard.html              # Web dashboard UI
├── models/
│   └── vosk-model-small-en-us-0.15/  # Vosk ASR model (~40MB)
├── requirements_windows.txt        # Python dependencies
├── download_models_windows.py      # Model downloader script
├── start_windows_perception.bat    # Quick start script
├── .env.example                    # Environment variables template
├── .gitignore                      # Git ignore rules
├── README.md                       # User-facing documentation
└── CLAUDE.md                       # This file (technical guide)
```

### Archive (Reference Only)

```
archive/
├── mac_*.py                        # Original macOS Python clients
├── test_*.py                       # Test and debug scripts
└── old_docs/                       # Legacy documentation
```

---

## Data Flow & API Contracts

### Client → Server Data Format

All clients POST to `http://127.0.0.1:8000/update_context` with this structure:

```python
{
    "device": "device_name",  # "screen", "Voice", or "Camera"
    "data": {
        # Device-specific payload
    }
}
```

### Device-Specific Payloads

#### Screen Client ([win_screen_ocr.py](win_screen_ocr.py))

```python
payload = {
    "device": "screen",  # MUST be lowercase
    "data": {
        "active_app": {
            "name": "chrome.exe",
            "window": "Google - Chrome"
        },
        "apps": [
            {"name": "chrome.exe", "window": "Google - Chrome"},
            {"name": "Code.exe", "window": "VS Code"},
            # ... more apps
        ],
        "screen_text": "def main():\n    print('Hello')"  # OCR extracted text
    }
}
```

#### Voice Client ([win_audio_asr.py](win_audio_asr.py))

```python
payload = {
    "device": "Voice",  # MUST have capital V
    "data": {
        "text": "hey who are you I don't think so"
    }
}
```

#### Camera Client ([win_camera_vision.py](win_camera_vision.py))

```python
payload = {
    "device": "Camera",  # MUST have capital C
    "data": {
        "environment_caption": "A person sitting at desk with laptop",
        "latency_ms": 7200
    }
}
```

### Server Context Structure

The `global_context` dict in [server.py](server.py) maintains:

```python
{
    "screen": {
        "active_app": {"name": "...", "window": "..."},
        "apps": [{"name": "...", "window": "..."}, ...],
        "screen_text": "..."  # OCR extracted text
    },
    "voice": {
        "text": "..."  # Transcribed speech
    },
    "camera": {
        "environment_caption": "...",  # Scene description
        "latency_ms": 7500
    },
    "fusion_detailed": "...",     # Full breakdown for dashboard
    "fusion_summary": "...",      # One-liner banner
    "recommendations": [...],     # 3 AI-generated recommendations
    "latency": 0.0,
    "events_processed": 0,
    "last_updated": "..."
}
```

---

## Key Implementation Details

### 1. Screen Monitoring + OCR ([win_screen_ocr.py](win_screen_ocr.py))

**Technology:**
- Win32 API for window detection
- PaddleOCR v5 (ONNX Runtime) for text extraction

**Key Logic:**
```python
# Get active window
hwnd = win32gui.GetForegroundWindow()
window_title = win32gui.GetWindowText(hwnd)

# Capture screenshot
screenshot = ImageGrab.grab()

# Resize for OCR performance
max_width = 480  # Balance speed vs quality
screenshot.thumbnail((max_width, max_width))

# Run OCR
ocr = PaddleOCR(use_angle_cls=True, lang='en', use_gpu=False)
result = ocr.ocr(np.array(screenshot), cls=True)
```

**Performance Tuning:**
- Input resolution: 480px max width (340ms latency)
- For ultra-fast: 320px (155ms latency)
- For quality: 640px (615ms latency)
- Update frequency: Every 3 seconds (on window change)

**Important Notes:**
- PaddleOCR models auto-download on first run (~170MB)
- Use NO ONNX optimizations (baseline is fastest!)
- Win32 API requires `pywin32` package

---

### 2. Camera Vision ([win_camera_vision.py](win_camera_vision.py))

**Technology:**
- OpenCV for camera capture
- FastVLM-0.5B (Apple) for scene description
- PyTorch CPU inference

**Key Logic:**
```python
# Load model (happens once at startup)
model_id = "apple/FastVLM-0.5B"
processor = AutoProcessor.from_pretrained(model_id)
model = FastVLMForCausalLM.from_pretrained(model_id)

# Capture frame
cap = cv2.VideoCapture(0)  # Camera index 0
ret, frame = cap.read()
image = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))

# Resize for CPU performance
image = image.resize((224, 224))

# Generate caption
inputs = processor(text=prompt, images=image, return_tensors="pt")
output = model.generate(**inputs, max_new_tokens=50)
caption = processor.decode(output[0], skip_special_tokens=True)
```

**Performance Tuning:**
- Input size: 224x224 (CPU-optimized)
- Max tokens: 50 (short captions only)
- Greedy decoding (no sampling for speed)
- Update frequency: Every 10 seconds
- Latency: 5-10 seconds (acceptable for ambient context)

**Important Notes:**
- FastVLM auto-downloads on first run (~1GB)
- Model runs in float32 on CPU (no quantization yet)
- Camera index may vary (0, 1, 2...) - adjust if needed

---

### 3. Voice Transcription ([win_audio_asr.py](win_audio_asr.py))

**Technology:**
- Vosk (offline ASR, no internet required)
- sounddevice for microphone capture
- 16kHz sampling rate

**Key Logic:**
```python
# Load model
model = vosk.Model("models/vosk-model-small-en-us-0.15")
recognizer = vosk.KaldiRecognizer(model, 16000)

# Capture audio stream
def audio_callback(indata, frames, time, status):
    if recognizer.AcceptWaveform(bytes(indata)):
        result = json.loads(recognizer.Result())
        text = result.get("text", "")
        # Send to server
        send_to_server(text)

with sd.RawInputStream(samplerate=16000, callback=audio_callback):
    # Runs continuously
    pass
```

**Performance Tuning:**
- Sample rate: 16kHz (optimal for Vosk)
- Latency: 100-200ms (near real-time)
- Continuous streaming (no batching)

**Important Notes:**
- Vosk model MUST exist in `models/vosk-model-small-en-us-0.15/`
- Requires microphone permissions in Windows Settings
- Offline model (~40MB) - no internet required

---

### 4. Fusion Server ([server.py](server.py))

**Technology:**
- FastAPI for web server
- Uvicorn ASGI server
- OpenAI GPT-4o-mini for fusion
- Jinja2 templates for dashboard

**Key Endpoints:**

```python
@app.post("/update_context")
async def update_context(payload: dict):
    """Receive perception data from clients"""
    device = payload["device"]
    data = payload["data"]

    # Update global context
    if device == "screen":
        global_context["screen"] = data
    elif device == "Voice":
        global_context["voice"] = data
    elif device == "Camera":
        global_context["camera"] = data

    # Run fusion
    run_fusion_and_recommendations()

    return {"status": "ok"}

@app.get("/dashboard")
async def dashboard():
    """Serve web dashboard"""
    return templates.TemplateResponse("dashboard.html", {
        "request": request,
        "context": global_context
    })

@app.get("/get_context")
async def get_context():
    """API endpoint for debugging"""
    return global_context
```

**Fusion Logic ([server.py:82](server.py#L82)):**

```python
def run_fusion_and_recommendations():
    """Fuse all perception signals and generate recommendations"""

    # Build fusion prompt
    prompt = f"""
You are a context-aware AI assistant analyzing multi-modal perception data.

Screen: {global_context['screen']}
Voice: {global_context['voice']}
Camera: {global_context['camera']}

Generate:
1. A detailed context summary
2. A one-line summary banner
3. Exactly 3 numbered actionable recommendations

Be concise and specific.
"""

    # Call GPT-4o-mini
    response = client.chat.completions.create(
        model="gpt-4o-mini",
        messages=[{"role": "user", "content": prompt}],
        max_tokens=150,
        temperature=0.5
    )

    # Parse response
    text = response.choices[0].message.content

    # Extract recommendations (lines starting with numbers)
    recommendations = [
        line.strip() for line in text.split('\n')
        if line.strip() and line.strip()[0].isdigit()
    ][:3]

    # Update global context
    global_context["fusion_detailed"] = text
    global_context["fusion_summary"] = recommendations[0] if recommendations else "Processing..."
    global_context["recommendations"] = recommendations
```

**Configuration:**
- Model: `gpt-4o-mini` (fast, cheap)
- Max tokens: 150 (keep responses short)
- Temperature: 0.5 (balanced creativity)
- Timeout: 5 seconds
- Requires `OPENAI_API_KEY` environment variable

---

## Model Details & Performance

### PaddleOCR (Screen Text Extraction)

**Model Details:**
- Version: PaddleOCR v5
- Format: ONNX (quantized for CPU)
- Size: ~170MB (auto-downloaded)
- Input: Screenshot image (resized to 480x480 max)
- Output: Extracted text with bounding boxes

**Performance:**
| Resolution | Latency | Quality |
|-----------|---------|---------|
| 320x320 | ~155ms | Good |
| 480x480 | ~340ms | Better (default) |
| 640x640 | ~615ms | Best |

**Key Optimizations:**
- Use NO ONNX optimizations (baseline is fastest!)
- Resize input image before OCR
- CPU-only inference (GPU doesn't help much for small images)

---

### FastVLM-0.5B (Camera Environment Understanding)

**Model Details:**
- Model: Apple's FastVLM-0.5B
- Format: PyTorch (Transformers library)
- Size: ~1GB (auto-downloaded from HuggingFace)
- Input: Camera frame (224x224 optimized for CPU)
- Output: Natural language caption

**Performance:**
- Latency: 5-10 seconds (CPU)
- Resolution: 224x224 (balance speed/quality)
- Max tokens: 50 (short captions only)
- Decoding: Greedy (no sampling)

**Key Optimizations:**
- Low resolution (224x224)
- Short generation (50 tokens max)
- Greedy decoding (faster than sampling)
- Float32 on CPU (no quantization yet)

**Future Improvements:**
- ONNX export: 5-10s → 2-4s potential
- C++ implementation: 5-10s → 1-3s potential
- Quantization: Further 30-50% speedup

---

### Vosk (Voice Transcription)

**Model Details:**
- Model: vosk-model-small-en-us-0.15
- Size: ~40MB (included in repo)
- Format: Vosk native format
- Input: Microphone stream (16kHz PCM)
- Output: Real-time transcription

**Performance:**
- Latency: 100-200ms
- Streaming: Continuous audio processing
- Accuracy: Good for clear speech, decent for noisy environments

**Key Features:**
- Offline (no internet required)
- Privacy-focused (all processing on-device)
- Lightweight (40MB model)

---

### GPT-4o-mini (Context Fusion)

**Model Details:**
- Provider: OpenAI
- Model: gpt-4o-mini
- API Version: v1
- Cost: $0.15/1M input tokens, $0.60/1M output tokens

**Performance:**
- TTFT (Time to First Token): ~300ms
- Full response: ~500ms (150 tokens)
- Rate limit: 10,000 requests/min (Tier 2)

**Configuration:**
```python
response = client.chat.completions.create(
    model="gpt-4o-mini",
    messages=[{"role": "user", "content": prompt}],
    max_tokens=150,      # Short responses only
    temperature=0.5,     # Consistent recommendations
    timeout=5.0          # Fail fast
)
```

---

## Development Commands

### Setup

```bash
# Create virtual environment
python -m venv venv
venv\Scripts\activate

# Install dependencies
pip install -r requirements_windows.txt

# Download models (optional - auto-downloads on first run)
python download_models_windows.py

# Set OpenAI API key
set OPENAI_API_KEY=sk-your-key-here
```

### Running the System

**Start in 4 separate terminals (order matters):**

```bash
# Terminal 1: Start the FastAPI fusion server
python server.py

# Terminal 2: Start screen monitoring + OCR
python win_screen_ocr.py

# Terminal 3: Start camera vision
python win_camera_vision.py

# Terminal 4: Start voice/microphone transcription
python win_audio_asr.py
```

**Access dashboard:**
```
http://127.0.0.1:8000/dashboard
```

**Debug API:**
```
http://127.0.0.1:8000/get_context
```

### Testing Individual Components

```bash
# Test screen monitoring only
python win_screen_ocr.py

# Test camera vision only
python win_camera_vision.py

# Test voice transcription only
python win_audio_asr.py

# Server will log when clients connect
```

---

## Performance Benchmarks

### Windows (CPU-Only Laptop)

| Component | CPU Load | RAM Usage | Latency | Update Frequency |
|-----------|----------|-----------|---------|------------------|
| Screen monitoring | 2-3% | ~50MB | <5ms | On window change |
| Screen OCR | 3-5% | ~200MB | 155-340ms | On window change (~3s) |
| Camera vision | 15-20% | ~500MB | 5-10s | Every 10 seconds |
| Voice/Mic | 10-15% | ~100MB | 100-200ms | Continuous stream |
| Fusion server | 2-5% | ~100MB | 300-500ms | On perception update |
| **Total** | **32-48%** | **~950MB** | - | - |

**Hardware Tested:** Intel Core i5/i7 laptop (4+ cores, 8GB+ RAM, no GPU)

**Expected Improvements with C++ Migration:**
- Camera: 5-10s → 1-3s (ONNX Runtime + quantization)
- Screen OCR: 155-340ms → 50-100ms (native ONNX Runtime)
- Voice: 100-200ms → 50-100ms (whisper.cpp or native Vosk)
- **Total CPU load: 32-48% → 15-25%**
- **Total RAM: 950MB → 400MB**

---

## Common Modifications

### Change Update Frequency

**Screen:**
```python
# win_screen_ocr.py - line ~100
time.sleep(3)  # Update every 3 seconds (on window change)
```

**Camera:**
```python
# win_camera_vision.py - line ~163
time.sleep(10)  # Update every 10 seconds
```

**Dashboard:**
```html
<!-- templates/dashboard.html - line 8 -->
<meta http-equiv="refresh" content="3">  <!-- Refresh every 3 seconds -->
```

### Change Number of Recommendations

```python
# server.py - line ~155
recommendations = [
    line.strip() for line in text.split('\n')
    if line.strip() and line.strip()[0].isdigit()
][:3]  # Change to [:5] for 5 recommendations
```

**Also update prompt at [server.py:140](server.py#L140):**
```python
prompt = """
...
Generate exactly 5 numbered recommendations.  # Change from 3 to 5
...
"""
```

### Change AI Model

```python
# server.py - line ~140
model="gpt-4o-mini"  # Fast, cheap (default)
# model="gpt-4o"     # Slower, better quality
# model="gpt-4"      # Highest quality, expensive
```

### Add New Perception Source

1. **Create new client script** that POSTs to `/update_context`:
```python
import requests

payload = {
    "device": "new_device",  # Unique device name
    "data": {
        "key": "value"
    }
}

response = requests.post(
    "http://127.0.0.1:8000/update_context",
    json=payload
)
```

2. **Update [server.py](server.py) to handle new device:**
```python
# Add to global_context
global_context["new_device"] = {}

# Handle in update_context endpoint
if device == "new_device":
    global_context["new_device"] = data
```

3. **Update fusion logic** at [server.py:82](server.py#L82):
```python
prompt = f"""
...
New Device: {global_context['new_device']}
...
"""
```

4. **Add to dashboard** at [templates/dashboard.html](templates/dashboard.html):
```html
<div class="card">
    <h2>New Device</h2>
    <pre>{{ context.new_device }}</pre>
</div>
```

---

## Troubleshooting

### Server Won't Start

```bash
# Check if port 8000 is in use
netstat -ano | findstr :8000

# Kill process using port 8000
taskkill /PID <PID> /F

# Set OpenAI API key (required for recommendations)
set OPENAI_API_KEY=sk-your-key-here

# Check if API key is set
echo %OPENAI_API_KEY%
```

### Screen OCR Not Working

**Issue:** PaddleOCR models not found

```bash
# Re-run download script
python download_models_windows.py

# Or let PaddleOCR auto-download on first run (requires internet)
```

**Issue:** pywin32 not installed

```bash
pip install pywin32
```

**Issue:** OCR too slow

```python
# win_screen_ocr.py - line ~60
max_width = 320  # Ultra-fast (~155ms)
# max_width = 480  # Balanced (~340ms) [default]
# max_width = 640  # Quality (~615ms)
```

### Camera Not Detected

```python
# win_camera_vision.py - line ~150
cap = cv2.VideoCapture(0)  # Try different indices: 0, 1, 2...
```

**Check available cameras:**
```python
import cv2
for i in range(5):
    cap = cv2.VideoCapture(i)
    if cap.isOpened():
        print(f"Camera {i} available")
        cap.release()
```

**Check Windows permissions:**
- Go to Settings → Privacy → Camera
- Enable camera access for Python

### Voice Transcription Errors

**Issue:** Vosk model not found

```bash
# Check model exists
dir models\vosk-model-small-en-us-0.15

# If missing, download from:
# https://alphacephei.com/vosk/models
```

**Issue:** Microphone not accessible

- Go to Settings → Privacy → Microphone
- Enable microphone access for Python

**Issue:** No audio detected

```python
# win_audio_asr.py - list available devices
import sounddevice as sd
print(sd.query_devices())
```

### Dashboard Not Showing Updates

**Check all clients are running:**
```bash
# Should see 4 Python processes
tasklist | findstr python
```

**Check server logs:**
```bash
# Server should log when clients connect
INFO:     127.0.0.1:xxxxx - "POST /update_context HTTP/1.1" 200 OK
```

**Verify clients posting to correct URL:**
```python
# All clients should POST to:
http://127.0.0.1:8000/update_context
```

**Check network connectivity:**
```bash
curl http://127.0.0.1:8000/get_context
```

### High CPU Usage

**Reduce update frequencies:**
```python
# win_screen_ocr.py
time.sleep(5)  # Update every 5 seconds instead of 3

# win_camera_vision.py
time.sleep(15)  # Update every 15 seconds instead of 10
```

**Use smaller OCR resolution:**
```python
# win_screen_ocr.py
max_width = 320  # Ultra-fast mode
```

**Disable camera if not needed:**
```bash
# Don't run win_camera_vision.py
# Fusion will still work with screen + voice only
```

---

## Project Status & Roadmap

### ✅ Completed (Windows Python Prototype - v1.0)
- Screen monitoring + OCR (PaddleOCR)
- Camera vision (FastVLM)
- Voice/microphone transcription (Vosk)
- Fusion server + dashboard (FastAPI)
- Real-time AI recommendations (GPT-4o-mini)
- CPU-optimized (no GPU required)
- Local-first processing (privacy-focused)

### ❌ Not Implemented
- **System audio capture** (device playback/loopback)
  - Requires WASAPI implementation (Windows-specific)
  - Complex to implement in Python
  - Deferred to C++ migration

### 🔨 Future Work (C++ Migration - v2.0)

**Goals:**
- Port all components to C++ for production deployment
- 2-5x performance improvement
- Single executable deployment (no Python runtime)
- System audio capture via WASAPI

**Expected Improvements:**
| Component | Python (Current) | C++ (Target) | Improvement |
|-----------|-----------------|--------------|-------------|
| Screen OCR | 155-340ms | 50-100ms | 2-3x faster |
| Camera Vision | 5-10s | 1-3s | 3-5x faster |
| Voice ASR | 100-200ms | 50-100ms | 2x faster |
| CPU Usage | 32-48% | 15-25% | 2x reduction |
| RAM Usage | 950MB | 400MB | 2.4x reduction |

**Technologies for C++ Version:**
- ONNX Runtime for PaddleOCR + FastVLM
- whisper.cpp or native Vosk for voice
- WASAPI for system audio loopback
- Qt 6 or native Win32 for UI
- CMake + vcpkg for build system

**Timeline:**
- Research & prototyping: 2-3 weeks
- Core implementation: 4-6 weeks
- Testing & optimization: 2-3 weeks
- **Total: 8-12 weeks**

---

## Security & Privacy

### Data Handling
- **Local-first:** All perception processing happens on-device
- **No telemetry:** No usage data sent to external servers (except OpenAI API)
- **API keys:** Stored in environment variables (never committed to git)

### API Key Security

**NEVER commit API keys to git!**

```bash
# Use environment variables
set OPENAI_API_KEY=sk-your-key-here

# Or create .env file (gitignored)
echo OPENAI_API_KEY=sk-your-key-here > .env
```

**The `.gitignore` includes:**
```
.env
*.key
credentials.json
```

### OpenAI API Usage

**Data sent to OpenAI:**
- Screen: App names, window titles, OCR text
- Voice: Transcribed speech
- Camera: Scene descriptions (NOT raw images)

**Data retention:**
- OpenAI retains API data for 30 days (see https://openai.com/policies/usage-policies)
- Can opt-out of data retention for training

**Alternative: Local LLM**

To avoid sending data to OpenAI, use local LLM:

```python
# server.py - replace OpenAI with local model
from transformers import pipeline

fusion_model = pipeline(
    "text-generation",
    model="mistralai/Mistral-7B-Instruct-v0.2",
    device="cpu"
)

# Update run_fusion_and_recommendations()
response = fusion_model(prompt, max_new_tokens=150)
```

**Note:** Local LLM will be slower (5-10s vs 500ms) but fully private.

---

## License & Credits

**License:** Proprietary - Nova Perception Engine Team

**Third-Party Libraries:**
- **PaddleOCR:** Apache 2.0 License - https://github.com/PaddlePaddle/PaddleOCR
- **FastVLM:** Apple Research - https://github.com/apple/ml-fastvlm
- **Vosk:** Apache 2.0 License - https://alphacephei.com/vosk/
- **OpenAI API:** Commercial License - https://openai.com/policies/terms-of-use
- **FastAPI:** MIT License - https://fastapi.tiangolo.com/
- **PyTorch:** BSD License - https://pytorch.org/
- **OpenCV:** Apache 2.0 License - https://opencv.org/

---

## Contributing Guidelines

When modifying the perception system:

1. **Update [server.py](server.py):**
   - Add new device type to `global_context` if needed
   - Update `run_fusion_and_recommendations()` to include new signals
   - Maintain data format consistency

2. **Update [templates/dashboard.html](templates/dashboard.html):**
   - Add new card/section for perception source
   - Follow existing styling patterns (dark theme)

3. **Update this document (CLAUDE.md):**
   - Add new perception source to architecture
   - Document models/dependencies
   - Add troubleshooting tips
   - Update performance benchmarks

4. **Update [README.md](README.md):**
   - Add high-level user-facing documentation
   - Update quick start guide if needed

5. **Test thoroughly:**
   - Test each client individually
   - Test with fusion server
   - Verify dashboard displays correctly
   - Check performance benchmarks

---

**Last Updated:** 2025-10-08
**Version:** 1.0.0 (Python Prototype for Windows)
**Maintained By:** Nova Perception Engine Team
