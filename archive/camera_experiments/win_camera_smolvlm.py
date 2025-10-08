"""
Windows Camera Vision Client with SmolVLM-256M

Uses HuggingFace's SmolVLM - a tiny 256M parameter vision-language model
optimized for fast inference on CPU.

Requirements:
- pip install torch transformers pillow opencv-python requests
"""

import sys
import os
# Fix Windows console encoding for emojis
if sys.platform == "win32":
    os.system("chcp 65001 > nul")
    sys.stdout.reconfigure(encoding='utf-8')

import time
import requests
import cv2
import torch
from PIL import Image
from transformers import AutoProcessor, AutoModelForImageTextToText

SERVER_URL = "http://127.0.0.1:8000/update_context"


class SmolVLMEngine:
    """Camera vision using SmolVLM-256M for fast scene understanding."""

    def __init__(self):
        """Initialize SmolVLM model."""
        print("🔧 Loading SmolVLM-256M-Instruct model...")
        print("   This is a tiny model (256M params) optimized for speed!")

        model_id = "HuggingFaceTB/SmolVLM-256M-Instruct"

        # Load processor and model
        self.processor = AutoProcessor.from_pretrained(model_id)

        # Use float32 for CPU
        self.model = AutoModelForImageTextToText.from_pretrained(
            model_id,
            dtype=torch.float32,  # CPU
            device_map="auto",
        )

        self.device = self.model.device

        print(f"✅ SmolVLM loaded successfully")
        print(f"   Device: {self.device}")
        print(f"   Model size: 256M parameters (very small!)")

    def describe_scene(self, frame):
        """Generate natural language description of camera frame."""
        start_time = time.time()

        try:
            # Convert BGR to RGB and create PIL Image
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame_rgb)

            # Build chat message with image placeholder
            # SmolVLM expects <image> token in the text to mark where image should be
            messages = [
                {
                    "role": "user",
                    "content": [
                        {"type": "image"},
                        {"type": "text", "text": "Describe this scene briefly in one sentence."}
                    ]
                },
            ]

            # Process inputs
            prompt = self.processor.apply_chat_template(
                messages,
                add_generation_prompt=True
            )

            inputs = self.processor(
                images=img,
                text=prompt,
                return_tensors="pt"
            ).to(self.device)

            # Generate description
            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=30,
                    temperature=0.3,
                    do_sample=True,
                )

            # Decode only the new tokens (skip input prompt)
            input_length = inputs["input_ids"].shape[-1]
            new_tokens = outputs[0][input_length:]
            description = self.processor.decode(new_tokens, skip_special_tokens=True).strip()

            # Clean up response
            # Remove common artifacts
            description = description.replace("Assistant:", "").strip()
            description = description.replace("assistant:", "").strip()

            # Take first sentence only
            for delimiter in ['\n', '. ', '? ', '! ']:
                if delimiter in description:
                    description = description.split(delimiter)[0].strip()
                    break

            # Remove trailing punctuation for cleaner display
            description = description.rstrip('.!?')

            # Ensure it's not empty
            if not description or len(description) < 5:
                description = "Scene captured"

            # Capitalize first letter
            if description:
                description = description[0].upper() + description[1:]

            latency = (time.time() - start_time) * 1000

            return description, latency

        except Exception as e:
            print(f"❌ Vision error: {e}")
            import traceback
            traceback.print_exc()
            return "Error generating description", 0


def main():
    print("="*60)
    print("📷 WINDOWS CAMERA VISION - SmolVLM")
    print("="*60)

    # Initialize SmolVLM first (this takes time)
    try:
        print("\n⏳ Loading SmolVLM model (this may take 1-2 minutes on first run)...")
        vision_engine = SmolVLMEngine()
    except Exception as e:
        print(f"❌ Failed to initialize SmolVLM: {e}")
        import traceback
        traceback.print_exc()
        print(f"\nMake sure you have installed:")
        print(f"  pip install torch transformers pillow accelerate")
        return

    # Initialize camera after model is loaded
    print("\n📷 Initializing camera...")
    cap = cv2.VideoCapture(0)

    if not cap.isOpened():
        print("❌ Failed to open camera")
        return

    # Set resolution
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    print("✅ Camera initialized (640x480)")

    print(f"\n🚀 Starting camera monitoring...")
    print(f"   Server: {SERVER_URL}")
    print(f"   Update frequency: Every 5 seconds")
    print(f"   Expected latency: ~1-3 seconds on CPU")
    print(f"   Press Ctrl+C to stop\n")

    frame_count = 0

    try:
        while True:
            ret, frame = cap.read()

            if ret:
                frame_count += 1
                print(f"\n📸 Frame {frame_count}")

                # Generate scene description
                description, latency = vision_engine.describe_scene(frame)
                print(f"   Scene: {description}")
                print(f"   ⚡ Latency: {latency:.0f}ms ({latency/1000:.1f}s)")

                # Post to server (use 'objects' field for dashboard compatibility)
                payload = {
                    "device": "Camera",
                    "data": {
                        "objects": [description]
                    }
                }

                try:
                    resp = requests.post(SERVER_URL, json=payload, timeout=3)
                    if resp.status_code == 200:
                        print("✅ Posted to server")
                    else:
                        print(f"❌ Server error {resp.status_code}")
                except requests.exceptions.ConnectionError:
                    print("⚠️ Server not reachable")
                except Exception as e:
                    print(f"⚠️ Post error: {e}")

            # Wait 5 seconds between captures
            time.sleep(5)

    except KeyboardInterrupt:
        print("\n\n👋 Stopping camera vision client...")
    finally:
        cap.release()
        print("✅ Camera released")


if __name__ == "__main__":
    main()
