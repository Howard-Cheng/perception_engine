# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nova Perception Engine** is a multi-modal perception system that fuses real-time data from three sources (screen activity, voice input, camera vision) to generate AI-powered contextual recommendations. It runs on macOS and uses GPT-4o-mini for fusion analysis.

### Architecture

The system consists of:
1. **FastAPI Server** (`server.py`) - Central fusion engine that receives perception data, runs AI-powered context fusion, and serves a live dashboard
2. **Three Perception Clients** - Independent processes that stream data to the server:
   - `mac_screen_v5.py` - Monitors active app and window titles via AppleScript
   - `mac_voice_stream.py` - Real-time speech recognition using Vosk
   - `mac_camera_yolo.py` - Object detection using YOLOv8
3. **Dashboard** (`templates/dashboard.html`) - Web UI that auto-refreshes every 3 seconds to display fused context and recommendations

**Data Flow:**
- Each perception client POSTs to `/update_context` endpoint with device-specific data
- Server stores latest state in `global_context` dict
- On each update, `run_fusion_and_recommendations()` fuses all signals and generates 3 actionable recommendations via GPT-4o-mini
- Dashboard renders the fused context at `/dashboard`

## Development Commands

### Setup
```bash
# Install dependencies
pip install -r requirements.txt

# Download YOLOv8 model (should already be present as yolov8n.pt)
# Download Vosk model (should be in models/vosk-model-small-en-us-0.15/)
```

### Running the System
**Start in separate terminals (order matters):**
```bash
# Terminal 1: Start the FastAPI server
python server.py

# Terminal 2: Start screen monitoring
python mac_screen_v5.py

# Terminal 3: Start voice streaming
python mac_voice_stream.py

# Terminal 4: Start camera detection
python mac_camera_yolo.py
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
# Test screen capture only
python mac_screen_v5.py

# Test voice recognition only
python mac_voice_stream.py

# Test camera vision only
python mac_camera_yolo.py
```

## Key Implementation Details

### Server Context Structure
The `global_context` dict in `server.py` maintains:
```python
{
    "screen": {"active_app": {name, window}, "apps": [...], "keywords": [...]},
    "voice": {"text": "..."},
    "camera": {"objects": [...]},
    "fusion_detailed": "...",  # Full breakdown for dashboard
    "fusion_summary": "...",   # One-liner banner
    "recommendations": [...],  # 3 AI-generated recommendations
    "latency": 0.0,
    "events_processed": 0,
    "last_updated": "..."
}
```

### Device Names
Client scripts must POST with exact device names:
- Screen: `"device": "screen"` (lowercase)
- Voice: `"device": "Voice"`
- Camera: `"device": "Camera"`

### Fusion Engine
`run_fusion_and_recommendations()` in `server.py:69`:
- Fuses screen, voice, and camera signals into coherent context
- Calls GPT-4o-mini with max_tokens=150, temp=0.5
- Generates exactly 3 numbered recommendations
- Updates `fusion_detailed`, `fusion_summary`, and `recommendations`

### macOS-Specific Components
- Screen monitoring uses AppleScript (`osascript`) to query System Events
- Voice streaming requires microphone permissions
- Camera requires webcam access via OpenCV

### Models Directory
- `models/camera/` - Legacy MobileNet-SSD Caffe implementation (not actively used)
- `models/vosk-model-small-en-us-0.15/` - Vosk speech recognition model
- `yolov8n.pt` - YOLOv8 nano model for object detection (root directory)

## Common Modifications

**To change update frequency:**
- Modify `time.sleep(2)` in perception clients (currently 2 seconds)
- Modify `<meta http-equiv="refresh" content="3">` in dashboard.html (currently 3 seconds)

**To change number of recommendations:**
- Update prompt in `server.py:119` and adjust slicing in `server.py:134`

**To add new perception source:**
1. Create new client script that POSTs to `/update_context` with unique device name
2. Update `global_context` structure in `server.py`
3. Update fusion logic in `run_fusion_and_recommendations()`
4. Add new card to `dashboard.html`

**To change AI model:**
- Modify `model="gpt-4o-mini"` in `server.py:126`
- Adjust OpenAI client initialization at `server.py:14`
