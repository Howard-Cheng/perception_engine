import time
from Quartz import CGWindowListCopyWindowInfo, kCGWindowListOptionOnScreenOnly, kCGNullWindowID

def get_frontmost_app():
    # Get all windows
    window_list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID)
    for window in window_list:
        if window.get('kCGWindowLayer') == 0:  # layer 0 = frontmost window
            app_name = window.get('kCGWindowOwnerName', 'Unknown')
            window_title = window.get('kCGWindowName', '')
            return app_name, window_title
    return "Unknown", ""

# Loop and print active app/title every 2s
while True:
    app, title = get_frontmost_app()
    print("Active App:", app, "| Title:", title)
    time.sleep(2)
