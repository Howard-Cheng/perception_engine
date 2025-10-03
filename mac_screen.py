import time
import requests
from Quartz import CGWindowListCopyWindowInfo, kCGWindowListOptionOnScreenOnly, kCGNullWindowID

SERVER_URL = "http://127.0.0.1:8000/update_context"

def get_active_window():
    # Get list of all visible windows
    window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID)
    # Frontmost window = layer 0
    for window in window_list:
        if window.get('kCGWindowLayer') == 0:
            app_name = window.get('kCGWindowOwnerName', 'Unknown')
            window_title = window.get('kCGWindowName', '')
            return app_name, window_title
    return "Unknown", ""

def main():
    while True:
        try:
            app_name, title = get_active_window()
            payload = {
                "device": "Mac",
                "data": {"active_app": app_name, "window_title": title}
            }
            requests.post(SERVER_URL, json=payload)
            print("Posted:", payload)
        except Exception as e:
            print("Error:", e)
        time.sleep(3)

if __name__ == "__main__":
    main()
