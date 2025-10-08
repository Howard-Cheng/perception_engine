"""
Windows Audio ASR Client (Dual-Channel)

Captures audio from:
1. Microphone (user speech)
2. System audio loopback (device playback)

Transcribes using Whisper and posts to fusion server.

Requirements:
- pip install sounddevice numpy requests vosk
- Whisper model in models/whisper/ (or use Vosk for prototype)
- For system audio: requires additional setup (see notes below)

Note: System audio loopback on Windows is complex.
      For prototype, starting with microphone only.
      Full implementation requires WASAPI in C++.
"""

import time
import requests
import numpy as np
import sounddevice as sd
import queue
from pathlib import Path

SERVER_URL = "http://127.0.0.1:8000/update_context"

# Try to import Vosk (easier to set up than whisper.cpp for Python)
try:
    from vosk import Model, KaldiRecognizer
    import json
    HAS_VOSK = True
except ImportError:
    HAS_VOSK = False
    print("⚠️ Vosk not installed. Run: pip install vosk")


class AudioASREngine:
    """Audio transcription using Vosk (prototype) or Whisper (future)."""

    def __init__(self, model_path):
        """Initialize ASR engine."""
        print(f"🔧 Loading ASR model...")

        if HAS_VOSK:
            if not Path(model_path).exists():
                print(f"❌ Model not found: {model_path}")
                print(f"   For Vosk: Download from https://alphacephei.com/vosk/models")
                print(f"   Recommended: vosk-model-small-en-us-0.15")
                raise FileNotFoundError(f"Model not found: {model_path}")

            self.model = Model(model_path)
            self.recognizer = KaldiRecognizer(self.model, 16000)
            self.recognizer.SetWords(True)
            print(f"✅ Vosk model loaded")
        else:
            raise ImportError("Vosk is required. Run: pip install vosk")

    def transcribe(self, audio_data):
        """Transcribe audio data to text."""
        if self.recognizer.AcceptWaveform(audio_data.tobytes()):
            result = json.loads(self.recognizer.Result())
            text = result.get("text", "").strip()
            return text if text else None
        else:
            # Partial result
            partial = json.loads(self.recognizer.PartialResult()).get("partial", "")
            return None  # Wait for final result


class MicrophoneMonitor:
    """Microphone audio capture and transcription."""

    def __init__(self, asr_engine):
        """Initialize microphone monitor."""
        self.asr_engine = asr_engine
        self.audio_queue = queue.Queue()
        self.running = False

    def audio_callback(self, indata, frames, time_info, status):
        """Callback for audio stream."""
        if status:
            print(f"⚠️ Audio status: {status}")
        self.audio_queue.put(bytes(indata))

    def start_streaming(self):
        """Start continuous audio capture and transcription."""
        print(f"🎤 Starting microphone streaming...")
        print(f"   Sample rate: 16000 Hz")
        print(f"   Speak into your microphone...")

        self.running = True

        with sd.RawInputStream(
            samplerate=16000,
            blocksize=8000,
            dtype='int16',
            channels=1,
            callback=self.audio_callback
        ):
            while self.running:
                try:
                    # Get audio chunk
                    data = self.audio_queue.get(timeout=1)

                    # Transcribe
                    text = self.asr_engine.transcribe(np.frombuffer(data, dtype=np.int16))

                    if text:
                        print(f"\n🎤 User speech: {text}")
                        post_to_server(user_speech=text, device_audio=None)

                except queue.Empty:
                    continue
                except KeyboardInterrupt:
                    break

    def stop(self):
        """Stop streaming."""
        self.running = False


def post_to_server(user_speech=None, device_audio=None):
    """Post audio context to fusion server."""
    # Server expects "text" field for voice data (see server.py line 87)
    payload = {
        "device": "Voice",
        "data": {
            "text": user_speech or ""  # Changed from "user_speech" to "text"
        }
    }

    if device_audio:
        payload["data"]["device_audio"] = device_audio

    try:
        resp = requests.post(SERVER_URL, json=payload, timeout=3)
        if resp.status_code == 200:
            print(f"   ✅ Posted to server")
        else:
            print(f"   ❌ Server error {resp.status_code}: {resp.text}")
    except requests.exceptions.ConnectionError:
        print(f"   ⚠️ Server not reachable (is server.py running?)")
    except Exception as e:
        print(f"   ⚠️ Error posting: {e}")


def list_audio_devices():
    """List available audio devices."""
    print("\n📋 Available audio devices:")
    devices = sd.query_devices()
    for idx, device in enumerate(devices):
        device_type = "INPUT" if device['max_input_channels'] > 0 else "OUTPUT"
        print(f"   [{idx}] {device['name']} ({device_type})")


def main():
    print("="*60)
    print("🎤 WINDOWS AUDIO ASR CLIENT")
    print("="*60)

    # List audio devices
    list_audio_devices()

    # Check for Vosk model (using existing macOS model for prototype)
    vosk_model_path = "models/vosk-model-small-en-us-0.15"

    if not Path(vosk_model_path).exists():
        print(f"\n❌ Vosk model not found: {vosk_model_path}")
        print(f"\n   For prototype, we'll use the existing macOS Vosk model.")
        print(f"   Download from: https://alphacephei.com/vosk/models")
        print(f"   Extract to: {vosk_model_path}")
        return

    # Initialize ASR engine
    try:
        asr_engine = AudioASREngine(vosk_model_path)
    except Exception as e:
        print(f"❌ Failed to initialize ASR: {e}")
        return

    # Initialize microphone monitor
    mic_monitor = MicrophoneMonitor(asr_engine)

    print(f"\n🚀 Starting audio monitoring...")
    print(f"   Server: {SERVER_URL}")
    print(f"   Press Ctrl+C to stop\n")

    print(f"ℹ️  Note: This prototype only captures microphone audio.")
    print(f"   System audio loopback requires WASAPI (C++ implementation).")
    print(f"   Full dual-channel support coming in C++ version.\n")

    try:
        mic_monitor.start_streaming()
    except KeyboardInterrupt:
        print(f"\n\n👋 Stopping audio ASR client...")
        mic_monitor.stop()


if __name__ == "__main__":
    main()
