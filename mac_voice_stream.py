import sounddevice as sd
import queue
import requests
import json
from vosk import Model, KaldiRecognizer

SERVER_URL = "http://127.0.0.1:8000/update_context"
MODEL_PATH = "models/vosk-model-small-en-us-0.15"

q = queue.Queue()

def callback(indata, frames, time, status):
    if status:
        print(status)
    q.put(bytes(indata))

def main():
    model = Model(MODEL_PATH)
    rec = KaldiRecognizer(model, 16000)
    rec.SetWords(True)

    with sd.RawInputStream(samplerate=16000, blocksize=8000, dtype='int16',
                           channels=1, callback=callback):
        print("🎤 Streaming... speak into the mic.")
        while True:
            data = q.get()
            if rec.AcceptWaveform(data):
                result = json.loads(rec.Result())
                text = result.get("text", "").strip()
                if text:
                    payload = {"device": "Voice", "data": {"text": text}}  # ✅ match dashboard
                    try:
                        requests.post(SERVER_URL, json=payload, timeout=2)
                        print("✅ Posted:", payload)
                    except Exception as e:
                        print("⚠️ Error posting:", e)
            else:
                partial = json.loads(rec.PartialResult()).get("partial", "")
                if partial:
                    print("Partial:", partial)

if __name__ == "__main__":
    main()
