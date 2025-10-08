"""
Windows Screen OCR Client

Captures active window screenshots and extracts text using PaddleOCR.
Posts to fusion server with screen context.

Requirements:
- pip install onnxruntime opencv-python pillow numpy requests pywin32
- PaddleOCR models in models/ocr/
"""

import os
import time
import requests
import numpy as np
import cv2
from PIL import Image, ImageGrab
import win32gui
import win32process
import psutil
from pathlib import Path
import onnxruntime as ort

SERVER_URL = "http://127.0.0.1:8000/update_context"

# Create log directory for screenshots
LOG_DIR = "./ocr_log"
os.makedirs(LOG_DIR, exist_ok=True)
screenshot_counter = 0

class PaddleOCREngine:
    """PaddleOCR ONNX inference engine."""

    def __init__(self, det_model_path, rec_model_path, max_width=640):
        """Initialize OCR models."""
        print(f"🔧 Loading PaddleOCR models...")
        print(f"   Detection: {det_model_path}")
        print(f"   Recognition: {rec_model_path}")
        print(f"   Max input width: {max_width}px (for performance)")

        # NO session options - baseline is fastest on this model!
        # Testing showed threading/graph optimization makes it 2-4x SLOWER
        self.det_session = ort.InferenceSession(
            det_model_path,
            providers=['CPUExecutionProvider']
        )
        self.rec_session = ort.InferenceSession(
            rec_model_path,
            providers=['CPUExecutionProvider']
        )

        self.max_width = max_width
        print(f"✅ OCR models loaded successfully")
        print(f"   Using baseline ONNX config (fastest for this model)")

    def preprocess_image_for_detection(self, image):
        """Preprocess image for text detection."""
        h, w = image.shape[:2]

        # Resize to max_width to reduce computation (critical for performance)
        if w > self.max_width:
            ratio = self.max_width / w
            new_w = self.max_width
            new_h = int(h * ratio)
        else:
            new_w, new_h = w, h

        # Make dimensions divisible by 32 (required by model)
        new_h = max(32, (new_h // 32) * 32)
        new_w = max(32, (new_w // 32) * 32)

        # Additional safety: ensure both dimensions are even (some ONNX ops require this)
        new_h = (new_h // 2) * 2
        new_w = (new_w // 2) * 2

        img = cv2.resize(image, (new_w, new_h))

        # Normalize
        img = img.astype(np.float32) / 255.0
        img = (img - 0.5) / 0.5

        # HWC to CHW
        img = img.transpose(2, 0, 1)

        # Add batch dimension
        img = np.expand_dims(img, axis=0).astype(np.float32)

        return img

    def detect_text_regions(self, image):
        """Detect text regions in image."""
        try:
            # Preprocess
            input_data = self.preprocess_image_for_detection(image)

            # Run detection
            input_name = self.det_session.get_inputs()[0].name
            outputs = self.det_session.run(None, {input_name: input_data})

            # Post-process to get bounding boxes
            # Note: This is simplified - full PaddleOCR has complex post-processing
            # For prototype, we'll extract text from entire image
            return [(0, 0, image.shape[1], image.shape[0])]  # Full image

        except Exception as e:
            print(f"❌ Detection error: {e}")
            return []

    def recognize_text(self, image_region):
        """Recognize text in a cropped region."""
        try:
            # Resize to fixed height (48 pixels as expected by PaddleOCR v5 server model)
            h, w = image_region.shape[:2]
            ratio = 48.0 / h
            new_w = int(w * ratio)

            # Ensure width is divisible by 8 and at least 48
            new_w = max(48, (new_w // 8) * 8)

            img = cv2.resize(image_region, (new_w, 48))

            # Normalize
            img = img.astype(np.float32) / 255.0
            img = (img - 0.5) / 0.5

            # HWC to CHW
            img = img.transpose(2, 0, 1)

            # Add batch dimension
            img = np.expand_dims(img, axis=0).astype(np.float32)

            # Run recognition
            input_name = self.rec_session.get_inputs()[0].name
            outputs = self.rec_session.run(None, {input_name: img})

            # Decode output to text (simplified)
            # Note: Full PaddleOCR has character mapping
            # For prototype, return placeholder
            return "Extracted text from region"

        except Exception as e:
            print(f"❌ Recognition error: {e}")
            return ""

    def extract_text(self, image):
        """Extract all text from image."""
        start_time = time.time()

        # Detect text regions
        regions = self.detect_text_regions(image)

        # Recognize text in each region
        texts = []
        for x1, y1, x2, y2 in regions:
            region = image[y1:y2, x1:x2]
            if region.size > 0:
                text = self.recognize_text(region)
                if text:
                    texts.append(text)

        latency = (time.time() - start_time) * 1000
        print(f"   OCR latency: {latency:.0f}ms")

        return " ".join(texts) if texts else ""


def get_active_window_info():
    """Get active window title and process name."""
    try:
        hwnd = win32gui.GetForegroundWindow()
        if hwnd == 0:
            return None, None

        # Get window title
        title = win32gui.GetWindowText(hwnd)

        # Get process name
        _, pid = win32process.GetWindowThreadProcessId(hwnd)
        try:
            process = psutil.Process(pid)
            process_name = process.name()
        except:
            process_name = "Unknown"

        return title, process_name

    except Exception as e:
        print(f"⚠️ Error getting window info: {e}")
        return None, None


def get_all_apps():
    """Get all running GUI applications with windows."""
    apps = []
    seen = set()

    def enum_windows_callback(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd)
            if title:  # Only include windows with titles
                try:
                    _, pid = win32process.GetWindowThreadProcessId(hwnd)
                    process = psutil.Process(pid)
                    process_name = process.name()

                    # Avoid duplicates (same process name)
                    if process_name not in seen:
                        seen.add(process_name)
                        apps.append({
                            "name": process_name,
                            "window": title
                        })
                except:
                    pass

    try:
        win32gui.EnumWindows(enum_windows_callback, None)
    except Exception as e:
        print(f"⚠️ Error enumerating windows: {e}")

    return apps[:20]  # Limit to top 20 to avoid clutter


def capture_active_window(save_log=False, app_name="", window_title=""):
    """Capture screenshot of active window."""
    global screenshot_counter
    try:
        # For prototype, capture entire screen
        # TODO: Capture only active window for efficiency
        screenshot = ImageGrab.grab()
        screenshot_np = np.array(screenshot)
        screenshot_cv = cv2.cvtColor(screenshot_np, cv2.COLOR_RGB2BGR)

        # Save to log if requested
        if save_log:
            screenshot_counter += 1
            clean_app = app_name.replace(".exe", "")
            timestamp = time.strftime("%H%M%S")
            filename = f"{screenshot_counter:03d}_{timestamp}_{clean_app}.png"
            filepath = os.path.join(LOG_DIR, filename)

            # Save screenshot
            cv2.imwrite(filepath, screenshot_cv)
            print(f"💾 Saved: {filename}")

        return screenshot_cv

    except Exception as e:
        print(f"❌ Screenshot error: {e}")
        return None


def post_to_server(window_title, process_name, extracted_text, all_apps=None, save_metadata=False):
    """Post screen context to fusion server."""
    # Extract simple keywords from window titles
    import re
    keywords = []
    text = f"{process_name} {window_title}"
    if all_apps:
        text += " " + " ".join([f"{a['name']} {a['window']}" for a in all_apps[:5]])
    words = re.findall(r"\w+", text.lower())
    keywords = list(set([w for w in words if len(w) > 3]))[:20]  # Unique words, length > 3

    payload = {
        "device": "screen",
        "data": {
            "active_app": {
                "name": process_name or "Unknown",
                "window": window_title or "Unknown"
            },
            "screen_text": extracted_text or "",
            "apps": all_apps or [],
            "keywords": keywords
        }
    }

    # Save metadata to log if requested
    if save_metadata:
        clean_app = process_name.replace(".exe", "")
        timestamp = time.strftime("%H%M%S")
        filename = f"{screenshot_counter:03d}_{timestamp}_{clean_app}.txt"
        filepath = os.path.join(LOG_DIR, filename)

        with open(filepath, "w", encoding="utf-8") as f:
            f.write(f"App: {process_name}\n")
            f.write(f"Window: {window_title}\n")
            f.write(f"Extracted Text: {extracted_text}\n")
            f.write(f"\nAll Apps ({len(all_apps) if all_apps else 0}):\n")
            if all_apps:
                for app in all_apps[:10]:
                    f.write(f"  - {app['name']}: {app['window']}\n")
            f.write(f"\nKeywords: {', '.join(keywords)}\n")

    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=3)
        if resp.status_code == 200:
            print(f"✅ Posted to server")
        else:
            print(f"❌ Server error {resp.status_code}: {resp.text}")
    except requests.exceptions.ConnectionError:
        print(f"⚠️ Server not reachable (is server.py running?)")
    except Exception as e:
        print(f"⚠️ Error posting: {e}")


def main():
    print("="*60)
    print("🖥️  WINDOWS SCREEN OCR CLIENT")
    print("="*60)

    # Check if models exist
    det_model_path = "models/ocr/PP-OCRv5_server_det_infer.onnx"
    rec_model_path = "models/ocr/PP-OCRv5_server_rec_infer.onnx"

    if not Path(det_model_path).exists() or not Path(rec_model_path).exists():
        print(f"❌ Models not found!")
        print(f"   Expected: {det_model_path}")
        print(f"   Expected: {rec_model_path}")
        print(f"\n   Run: python download_models_windows.py")
        return

    # Initialize OCR engine
    try:
        ocr_engine = PaddleOCREngine(det_model_path, rec_model_path)
    except Exception as e:
        print(f"❌ Failed to initialize OCR: {e}")
        return

    print(f"\n🚀 Starting screen monitoring...")
    print(f"   Server: {SERVER_URL}")
    print(f"   Update frequency: Every 3 seconds")
    print(f"   Press Ctrl+C to stop")
    print(f"📂 Screenshots will be saved to: {os.path.abspath(LOG_DIR)}\n")

    last_window = None

    try:
        while True:
            # Get active window info
            window_title, process_name = get_active_window_info()

            if window_title:
                # Only capture & OCR if window changed (optimization)
                current_window = f"{process_name}:{window_title}"

                if current_window != last_window:
                    print(f"\n🔄 Window changed: {process_name} - {window_title}")

                    # Get all open apps
                    all_apps = get_all_apps()
                    print(f"   📋 Total apps running: {len(all_apps)}")

                    # Capture screenshot with logging enabled
                    screenshot = capture_active_window(
                        save_log=True,
                        app_name=process_name,
                        window_title=window_title
                    )

                    # For now, skip full OCR and just send window metadata
                    # TODO: Implement proper PaddleOCR text detection + recognition pipeline
                    extracted_text = f"Active window: {window_title}"

                    # Optional: Try to extract text if you want (currently simplified)
                    # if screenshot is not None:
                    #     extracted_text = ocr_engine.extract_text(screenshot)

                    print(f"   📝 Context: {extracted_text[:100]}...")

                    # Post to server with all apps (with metadata logging)
                    post_to_server(window_title, process_name, extracted_text, all_apps, save_metadata=True)

                    last_window = current_window
                else:
                    print(f"⏭️  Same window, skipping")

            time.sleep(3)  # Update every 3 seconds

    except KeyboardInterrupt:
        print(f"\n\n👋 Stopping screen OCR client...")


if __name__ == "__main__":
    main()
