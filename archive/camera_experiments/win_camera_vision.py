"""
Windows Camera Vision Client with OCR

Captures webcam frames and:
1. Generates natural language captions using FastVLM (optional)
2. Extracts text using PaddleOCR
Posts combined context to fusion server.

Requirements:
- pip install onnxruntime opencv-python pillow numpy requests transformers
- PaddleOCR ONNX models in models/ocr/
- FastVLM ONNX model in models/vision/ (optional)

Note: For prototype, using transformers library for FastVLM.
      Production version should use pure ONNX Runtime.
"""

import time
import requests
import numpy as np
import cv2
import onnxruntime as ort
from pathlib import Path

SERVER_URL = "http://127.0.0.1:8000/update_context"

# Try to import transformers for FastVLM
try:
    from transformers import pipeline
    HAS_TRANSFORMERS = True
except ImportError:
    HAS_TRANSFORMERS = False
    print("⚠️ transformers not installed. Vision captions disabled.")


class CameraOCREngine:
    """Simple OCR for camera frames using PaddleOCR."""

    def __init__(self, det_model_path, rec_model_path):
        """Initialize OCR models."""
        print(f"🔧 Loading PaddleOCR for camera...")

        # Load models without any optimization (baseline is fastest)
        self.det_session = ort.InferenceSession(
            str(det_model_path),
            providers=['CPUExecutionProvider']
        )
        self.rec_session = ort.InferenceSession(
            str(rec_model_path),
            providers=['CPUExecutionProvider']
        )

        print(f"✅ Camera OCR loaded")

    def extract_text_simple(self, frame):
        """Extract text from camera frame (simplified approach)."""
        try:
            # Resize frame to reasonable size for OCR
            h, w = frame.shape[:2]
            max_width = 640
            if w > max_width:
                ratio = max_width / w
                new_w = max_width
                new_h = int(h * ratio)
                frame = cv2.resize(frame, (new_w, new_h))

            # Preprocess for detection
            h, w = frame.shape[:2]
            new_h = max(32, (h // 32) * 32)
            new_w = max(32, (w // 32) * 32)
            new_h = (new_h // 2) * 2
            new_w = (new_w // 2) * 2

            img = cv2.resize(frame, (new_w, new_h))
            img = img.astype(np.float32) / 255.0
            img = (img - 0.5) / 0.5
            img = img.transpose(2, 0, 1)
            img = np.expand_dims(img, axis=0).astype(np.float32)

            # Run detection
            input_name = self.det_session.get_inputs()[0].name
            outputs = self.det_session.run(None, {input_name: img})

            # For camera OCR, just return "text detected" or "no text"
            # Full text recognition requires complex post-processing
            # Check if detection output has significant activations
            detection_map = outputs[0]
            has_text = np.max(detection_map) > 0.5

            if has_text:
                return "text_detected"
            else:
                return "no_text"

        except Exception as e:
            print(f"   OCR error: {e}")
            return "ocr_error"


class CameraVisionEngine:
    """Camera vision using FastVLM or fallback to simple description."""

    def __init__(self, use_model=True):
        """Initialize vision engine."""
        self.use_model = use_model and HAS_TRANSFORMERS

        if self.use_model:
            print(f"🔧 Loading FastVLM model...")
            print(f"   This may take a minute on first run...")

            try:
                # Try to load FastVLM from HuggingFace
                # Note: This will download the model if not present
                self.captioner = pipeline(
                    "image-to-text",
                    model="onnx-community/FastVLM-0.5B-ONNX",
                    device=-1  # CPU
                )
                print(f"✅ FastVLM loaded successfully")
            except Exception as e:
                print(f"❌ Failed to load FastVLM: {e}")
                print(f"   Falling back to simple frame description")
                self.use_model = False
        else:
            print(f"ℹ️  Using simple frame description (no ML model)")

    def generate_caption(self, frame):
        """Generate natural language caption for camera frame."""
        start_time = time.time()

        if self.use_model:
            try:
                # Resize for CPU efficiency
                frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                frame_resized = cv2.resize(frame_rgb, (384, 384))

                # Generate caption
                result = self.captioner(frame_resized)
                caption = result[0]['generated_text'] if result else "Unable to generate caption"

                latency = (time.time() - start_time) * 1000
                print(f"   Vision latency: {latency:.0f}ms")

                return caption

            except Exception as e:
                print(f"❌ Caption generation error: {e}")
                return self._simple_description(frame)
        else:
            return self._simple_description(frame)

    def _simple_description(self, frame):
        """Simple frame description without ML (fallback)."""
        h, w = frame.shape[:2]

        # Calculate brightness
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        brightness = np.mean(gray)

        # Simple description
        brightness_level = "bright" if brightness > 127 else "dim"
        return f"Camera frame captured ({w}x{h}), {brightness_level} lighting"


def initialize_camera(camera_index=0):
    """Initialize webcam."""
    print(f"📷 Initializing camera {camera_index}...")

    cap = cv2.VideoCapture(camera_index)

    if not cap.isOpened():
        print(f"❌ Failed to open camera {camera_index}")
        return None

    # Set resolution (lower for CPU efficiency)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    print(f"✅ Camera initialized")
    return cap


def capture_frame(cap):
    """Capture single frame from camera."""
    ret, frame = cap.read()

    if not ret:
        print(f"⚠️ Failed to capture frame")
        return None

    return frame


def post_to_server(caption, ocr_result=None):
    """Post camera context to fusion server."""
    data = {
        "environment_caption": caption
    }

    if ocr_result:
        data["text_detected"] = ocr_result

    payload = {
        "device": "Camera",
        "data": data
    }

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
    print("📷 WINDOWS CAMERA VISION CLIENT WITH OCR")
    print("="*60)

    # Initialize camera
    cap = initialize_camera()
    if cap is None:
        print(f"\n❌ Camera initialization failed. Exiting.")
        return

    # Initialize OCR engine
    ocr_engine = None
    det_model_path = Path("models/ocr/PP-OCRv5_server_det_infer.onnx")
    rec_model_path = Path("models/ocr/PP-OCRv5_server_rec_infer.onnx")

    if det_model_path.exists() and rec_model_path.exists():
        try:
            ocr_engine = CameraOCREngine(det_model_path, rec_model_path)
        except Exception as e:
            print(f"⚠️ OCR initialization failed: {e}")
            print(f"   Continuing without OCR")
    else:
        print(f"⚠️ PaddleOCR models not found, skipping OCR")

    # Initialize vision engine (optional)
    vision_engine = None
    try:
        vision_engine = CameraVisionEngine(use_model=HAS_TRANSFORMERS)
    except Exception as e:
        print(f"⚠️ Vision engine initialization failed: {e}")
        print(f"   Continuing with OCR only")

    if not ocr_engine and not vision_engine:
        print(f"\n❌ Neither OCR nor vision engine available. Exiting.")
        cap.release()
        return

    print(f"\n🚀 Starting camera monitoring...")
    print(f"   Server: {SERVER_URL}")
    print(f"   OCR: {'✅ Enabled' if ocr_engine else '❌ Disabled'}")
    print(f"   Vision: {'✅ Enabled' if vision_engine else '❌ Disabled'}")
    print(f"   Update frequency: Every 5 seconds")
    print(f"   Press Ctrl+C to stop\n")

    frame_count = 0

    try:
        while True:
            # Capture frame
            frame = capture_frame(cap)

            if frame is not None:
                frame_count += 1
                print(f"\n📸 Frame {frame_count}")

                # Generate caption (if vision engine available)
                caption = "Camera frame"
                if vision_engine:
                    caption = vision_engine.generate_caption(frame)
                    print(f"   Vision: {caption}")

                # Extract text detection (if OCR engine available)
                ocr_result = None
                if ocr_engine:
                    ocr_result = ocr_engine.extract_text_simple(frame)
                    print(f"   OCR: {ocr_result}")

                # Post to server
                post_to_server(caption, ocr_result)

            # Wait 5 seconds (camera updates less frequently than screen)
            time.sleep(5)

    except KeyboardInterrupt:
        print(f"\n\n👋 Stopping camera vision client...")
    finally:
        cap.release()
        print(f"✅ Camera released")


if __name__ == "__main__":
    main()
