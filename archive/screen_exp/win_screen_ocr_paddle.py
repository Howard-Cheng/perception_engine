import os
import time
import requests
import win32gui
import win32process
import psutil
from PIL import ImageGrab
from paddleocr import PaddleOCR

SERVER_URL = "http://127.0.0.1:8000/update_context"

# Create log directory for screenshots
LOG_DIR = "./ocr_log"
os.makedirs(LOG_DIR, exist_ok=True)
screenshot_counter = 0

# Initialize PaddleOCR instance
print("🔧 Initializing PaddleOCR...")
print("   Language: English")
print("   This may take a moment on first run to download models...\n")

ocr = PaddleOCR(
    use_angle_cls=True,
    lang='en',
)

print("✅ PaddleOCR initialized successfully\n")


def get_active_window_info():
    """Get active window title and process name."""
    try:
        hwnd = win32gui.GetForegroundWindow()
        if hwnd == 0:
            return None, None

        title = win32gui.GetWindowText(hwnd)
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
    """Get all running GUI applications."""
    apps = []
    seen = set()

    def enum_windows_callback(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd)
            if title:
                try:
                    _, pid = win32process.GetWindowThreadProcessId(hwnd)
                    process = psutil.Process(pid)
                    process_name = process.name()

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

    return apps[:20]


def capture_and_extract_text(app_name, window_title):
    """Capture screenshot and extract text using PaddleOCR."""
    global screenshot_counter

    try:
        start_time = time.time()

        # Capture full screen
        screenshot = ImageGrab.grab()

        # Save screenshot
        screenshot_counter += 1
        clean_app = app_name.replace(".exe", "")
        timestamp = time.strftime("%H%M%S")
        filename = f"{screenshot_counter:03d}_{timestamp}_{clean_app}.png"
        filepath = os.path.join(LOG_DIR, filename)
        screenshot.save(filepath)

        print(f"   🔍 Running OCR on screenshot...")
        ocr_start = time.time()

        # Run OCR using PaddleOCR
        result = ocr.ocr(filepath, cls=True)

        # Extract text from results
        # PaddleOCR returns: [[[bbox], (text, confidence)], ...]
        extracted_texts = []
        detailed_results = []

        if result and result[0]:
            for line in result[0]:
                bbox, (text, confidence) = line
                extracted_texts.append(text)
                detailed_results.append({
                    'text': text,
                    'confidence': confidence,
                    'bbox': bbox
                })

        full_text = " ".join(extracted_texts)
        ocr_latency = (time.time() - ocr_start) * 1000
        total_latency = (time.time() - start_time) * 1000

        print(f"   ⚡ OCR latency: {ocr_latency:.0f}ms")
        print(f"   📝 Extracted {len(extracted_texts)} text lines ({len(full_text)} chars)")
        print(f"💾 Saved: {filename}")

        # Save metadata
        txt_filepath = filepath.replace(".png", ".txt")
        with open(txt_filepath, "w", encoding="utf-8") as f:
            f.write(f"App: {app_name}\n")
            f.write(f"Window: {window_title}\n")
            f.write(f"OCR Latency: {ocr_latency:.0f}ms\n")
            f.write(f"Total Latency: {total_latency:.0f}ms\n")
            f.write(f"Text Lines Found: {len(extracted_texts)}\n")
            f.write(f"\n--- EXTRACTED TEXT ({len(full_text)} chars) ---\n")
            f.write(full_text)
            f.write(f"\n\n--- DETAILED RESULTS ---\n")
            for i, res in enumerate(detailed_results):
                f.write(f"{i+1}. [{res['confidence']:.2f}] {res['text']}\n")

        return full_text, ocr_latency

    except Exception as e:
        print(f"❌ OCR error: {e}")
        import traceback
        traceback.print_exc()
        return "", 0.0


def post_to_server(window_title, process_name, extracted_text, all_apps, ocr_latency):
    """Post screen context to fusion server."""
    import re

    # Extract keywords
    text = f"{process_name} {window_title} {extracted_text[:500]}"
    words = re.findall(r"\w+", text.lower())
    keywords = list(set([w for w in words if len(w) > 3]))[:30]

    payload = {
        "device": "screen",
        "data": {
            "active_app": {
                "name": process_name or "Unknown",
                "window": window_title or "Unknown"
            },
            "text_detected": extracted_text or "",
            "apps": all_apps or [],
            "keywords": keywords,
            "ocr_latency_ms": ocr_latency
        }
    }

    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=5)
        if resp.status_code == 200:
            print(f"✅ Posted to server\n")
        else:
            print(f"❌ Server error {resp.status_code}\n")
    except requests.exceptions.ConnectionError:
        print(f"⚠️ Server not reachable (is server.py running?)\n")
    except Exception as e:
        print(f"⚠️ Error posting: {e}\n")


def main():
    print("="*60)
    print("🖥️  WINDOWS SCREEN OCR CLIENT (PaddleOCR)")
    print("="*60)
    print(f"🚀 Starting screen monitoring with REAL OCR...")
    print(f"   Server: {SERVER_URL}")
    print(f"   Update frequency: Every 5 seconds")
    print(f"📂 Screenshots: {os.path.abspath(LOG_DIR)}")
    print(f"   Press Ctrl+C to stop\n")

    last_window = None

    try:
        while True:
            window_title, process_name = get_active_window_info()

            if window_title:
                current_window = f"{process_name}:{window_title}"

                if current_window != last_window:
                    print(f"🔄 Window changed: {process_name} - {window_title}")

                    # Get all apps
                    all_apps = get_all_apps()
                    print(f"   📋 Total apps running: {len(all_apps)}")

                    # Capture screenshot and extract text with REAL OCR
                    extracted_text, ocr_latency = capture_and_extract_text(
                        process_name,
                        window_title
                    )

                    # Show preview
                    preview = extracted_text[:150] + "..." if len(extracted_text) > 150 else extracted_text
                    print(f"   💬 Preview: {preview}")

                    # Post to server
                    post_to_server(window_title, process_name, extracted_text, all_apps, ocr_latency)

                    last_window = current_window
                else:
                    print(f"⏭️  Same window, skipping")

            time.sleep(5)  # Update every 5 seconds

    except KeyboardInterrupt:
        print(f"\n\n👋 Stopping screen OCR client...")


if __name__ == "__main__":
    main()