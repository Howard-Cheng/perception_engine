"""
Windows Screen Context using BLIP Image Captioning
Demo version to compare BLIP vs OCR approaches
"""

import os
import time
import requests
import win32gui
import win32process
import psutil
from PIL import ImageGrab
from transformers import AutoProcessor, BlipForConditionalGeneration

SERVER_URL = "http://127.0.0.1:8000/update_context"

# Create log directory for screenshots
LOG_DIR = "./blip_log"
os.makedirs(LOG_DIR, exist_ok=True)
screenshot_counter = 0

print("🔹 Loading BLIP model (Salesforce/blip-image-captioning-base)...")
MODEL_ID = "Salesforce/blip-image-captioning-base"

processor = AutoProcessor.from_pretrained(MODEL_ID)
model = BlipForConditionalGeneration.from_pretrained(MODEL_ID)
print("✅ BLIP model loaded\n")

def get_active_app():
    """Get the currently active window and its process name (Windows)."""
    try:
        hwnd = win32gui.GetForegroundWindow()
        window_title = win32gui.GetWindowText(hwnd)

        _, pid = win32process.GetWindowThreadProcessId(hwnd)
        process = psutil.Process(pid)
        app_name = process.name()

        return {
            "name": app_name,
            "window": window_title
        }
    except Exception as e:
        print(f"⚠️ Error fetching active app: {e}")
        return {}

def get_all_apps():
    """Get list of all visible applications."""
    apps = []
    seen = set()

    def enum_handler(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            try:
                _, pid = win32process.GetWindowThreadProcessId(hwnd)
                process = psutil.Process(pid)
                app_name = process.name()

                if app_name not in seen and app_name != "":
                    seen.add(app_name)
                    apps.append({"name": app_name})
            except:
                pass

    win32gui.EnumWindows(enum_handler, None)
    return apps[:10]  # Limit to 10 apps

def describe_screen(active_app):
    """Capture screenshot and generate BLIP caption."""
    global screenshot_counter
    try:
        start_time = time.time()

        # Capture screen
        img = ImageGrab.grab()

        # Save screenshot for debugging
        screenshot_counter += 1
        app_name = active_app.get("name", "unknown").replace(".exe", "")
        timestamp = time.strftime("%H%M%S")
        filename = f"{screenshot_counter:03d}_{timestamp}_{app_name}.png"
        filepath = os.path.join(LOG_DIR, filename)
        img.save(filepath)

        # Run BLIP captioning
        inputs = processor(images=img, return_tensors="pt")
        out = model.generate(**inputs, max_new_tokens=30)
        caption = processor.decode(out[0], skip_special_tokens=True)

        latency = (time.time() - start_time) * 1000

        # Also save caption to text file
        caption_file = filepath.replace(".png", ".txt")
        with open(caption_file, "w") as f:
            f.write(f"App: {active_app.get('name')}\n")
            f.write(f"Window: {active_app.get('window')}\n")
            f.write(f"Caption: {caption}\n")
            f.write(f"Latency: {latency:.0f}ms\n")

        print(f"💾 Saved: {filename}")

        return caption, latency
    except Exception as e:
        print(f"⚠️ Error describing screen: {e}")
        return "", 0.0

def post_context(active_app, all_apps, caption, latency):
    """Post screen context to fusion server."""
    payload = {
        "device": "Screen",
        "data": {
            "active_app": active_app,
            "apps": all_apps,
            "screen_caption_blip": caption,  # BLIP caption
            "blip_latency_ms": latency
        }
    }
    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=5)
        if resp.status_code == 200:
            print(f"✅ [{time.strftime('%H:%M:%S')}] BLIP: \"{caption}\" ({latency:.0f}ms)")
        else:
            print(f"❌ Server error {resp.status_code}")
    except Exception as e:
        print(f"⚠️ Error posting: {e}")

def main():
    print("🖼️ Starting Windows Screen Monitor with BLIP Captioning")
    print("=" * 60)
    print("This demo uses BLIP to generate captions of your screen.")
    print("Compare output with win_screen_ocr.py to see the difference.")
    print("=" * 60 + "\n")

    print(f"📂 Screenshots will be saved to: {os.path.abspath(LOG_DIR)}\n")

    while True:
        try:
            active_app = get_active_app()
            all_apps = get_all_apps()
            caption, latency = describe_screen(active_app)

            post_context(active_app, all_apps, caption, latency)

            time.sleep(5)  # Update every 5 seconds

        except KeyboardInterrupt:
            print("\n👋 Shutting down...")
            break
        except Exception as e:
            print(f"⚠️ Error in main loop: {e}")
            time.sleep(5)

if __name__ == "__main__":
    main()
