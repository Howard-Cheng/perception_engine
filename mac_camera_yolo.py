import cv2
import requests
import time
from ultralytics import YOLO

SERVER_URL = "http://127.0.0.1:8000/update_context"

def post_context(objects):
    payload = {
        "device": "Camera",   # ✅ match server + dashboard
        "data": {"objects": objects}
    }
    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=2)  # increased timeout
        if resp.status_code == 200:
            print(f"✅ Posted: {payload}")
        else:
            print(f"❌ Server error {resp.status_code}: {resp.text}")
    except requests.exceptions.Timeout:
        print("⏳ Timeout: server took too long to respond")
    except Exception as e:
        print(f"⚠️ Error posting: {e}")

def main():
    print("📷 Starting mac_camera_yolo.py ...")
    cap = cv2.VideoCapture(0)  # use MacBook webcam
    if not cap.isOpened():
        print("❌ Camera not available")
        return

    print("✅ Camera initialized successfully (YOLOv8).")
    model = YOLO("yolov8n.pt")  # lightweight YOLOv8 model

    while True:
        ret, frame = cap.read()
        if not ret:
            print("⚠️ Failed to grab frame")
            break

        results = model(frame, verbose=False)
        objects = []
        for r in results:
            for box in r.boxes:
                cls_id = int(box.cls[0])
                cls_name = model.names[cls_id]
                objects.append(cls_name)

        objects = list(set(objects))  # unique objects
        if objects:
            post_context(objects)

        time.sleep(2)  # send updates every 2s

    cap.release()

if __name__ == "__main__":
    main()
