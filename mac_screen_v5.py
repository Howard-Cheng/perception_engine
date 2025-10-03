import subprocess
import time
import requests
import re

SERVER_URL = "http://127.0.0.1:8000/update_context"

def get_active_app():
    """Get the currently frontmost app + its window title."""
    try:
        script = '''
        tell application "System Events"
            set frontApp to first application process whose frontmost is true
            set appName to name of frontApp
            if exists (window 1 of frontApp) then
                set winName to name of window 1 of frontApp
            else
                set winName to ""
            end if
        end tell
        return appName & "||" & winName
        '''
        result = subprocess.check_output(["osascript", "-e", script]).decode("utf-8").strip()
        if "||" in result:
            name, window = result.split("||", 1)
            return {"name": name.strip(), "window": window.strip()}
    except Exception as e:
        print(f"⚠️ Error fetching active app: {e}")
    return {}

def get_all_apps():
    """Get all open apps + their first window name."""
    try:
        script = '''
        set output to ""
        tell application "System Events"
            set app_list to (name of processes whose background only is false)
            repeat with app_name in app_list
                tell process app_name
                    if exists (window 1) then
                        set win_name to name of window 1
                    else
                        set win_name to ""
                    end if
                end tell
                set output to output & app_name & "||" & win_name & ";;"
            end repeat
        end tell
        return output
        '''
        result = subprocess.check_output(["osascript", "-e", script]).decode("utf-8").strip()
        apps = []
        for entry in result.split(";;"):
            if "||" in entry:
                name, window = entry.split("||", 1)
                apps.append({"name": name.strip(), "window": window.strip()})
        return apps
    except Exception as e:
        print(f"⚠️ Error fetching apps: {e}")
        return []

def extract_keywords(active_app, apps):
    """Generate keywords from active app and background apps."""
    text = ""
    if active_app:
        text += f"{active_app['name']} {active_app['window']} "
    text += " ".join([f"{a['name']} {a['window']}" for a in apps])
    words = re.findall(r"\w+", text.lower())
    common = [w for w in words if len(w) > 2]
    return list(set(common))[:30]

def post_context(active_app, apps, keywords):
    payload = {
        "device": "screen",   # 👈 must match server.py
        "data": {
            "active_app": active_app,
            "apps": apps,
            "keywords": keywords
        }
    }
    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=2)
        if resp.status_code == 200:
            print(f"✅ Posted: {payload}")
        else:
            print(f"❌ Server error {resp.status_code}: {resp.text}")
    except Exception as e:
        print(f"⚠️ Error posting: {e}")

def main():
    print("🖥️ Starting mac_screen_v5.py ...")
    while True:
        active_app = get_active_app()
        apps = get_all_apps()
        keywords = extract_keywords(active_app, apps)
        post_context(active_app, apps, keywords)
        time.sleep(2)

if __name__ == "__main__":
    main()
