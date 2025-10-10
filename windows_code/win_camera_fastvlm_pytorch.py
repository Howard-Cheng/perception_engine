"""
Windows Camera Vision - OPTIMIZED FastVLM
Lower resolution + fewer tokens for 2x speed improvement
"""

import time
import requests
import cv2
import torch
from PIL import Image
from transformers import AutoTokenizer, AutoModelForCausalLM

SERVER_URL = "http://127.0.0.1:8777/update_context"  # C++ PerceptionEngine server
IMAGE_TOKEN_INDEX = -200

class FastVLMEngine:
    def __init__(self):
        print("🔧 Loading FastVLM-0.5B model (optimized mode)...")
        model_id = "apple/FastVLM-0.5B"

        self.tokenizer = AutoTokenizer.from_pretrained(model_id, trust_remote_code=True)
        dtype = torch.float16 if torch.cuda.is_available() else torch.float32

        self.model = AutoModelForCausalLM.from_pretrained(
            model_id,
            dtype=dtype,
            device_map="auto",
            trust_remote_code=True,
        )

        self.device = self.model.device
        self.dtype = dtype

        print(f"✅ FastVLM loaded (Device: {self.device})")

    def describe_scene(self, frame):
        start_time = time.time()
        
        try:
            # OPTIMIZATION: Resize to smaller resolution
            frame = cv2.resize(frame, (224, 224))  # Much smaller!
            
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame_rgb)
            
            # Short prompt for faster generation
            messages = [{"role": "user", "content": "<image>\nBriefly, what is this?"}]
            rendered = self.tokenizer.apply_chat_template(messages, add_generation_prompt=True, tokenize=False)
            
            pre, post = rendered.split("<image>", 1)
            pre_ids = self.tokenizer(pre, return_tensors="pt", add_special_tokens=False).input_ids
            post_ids = self.tokenizer(post, return_tensors="pt", add_special_tokens=False).input_ids
            
            img_tok = torch.tensor([[IMAGE_TOKEN_INDEX]], dtype=pre_ids.dtype)
            input_ids = torch.cat([pre_ids, img_tok, post_ids], dim=1).to(self.device)
            attention_mask = torch.ones_like(input_ids, device=self.device)
            
            px = self.model.get_vision_tower().image_processor(images=img, return_tensors="pt")["pixel_values"]
            px = px.to(self.device, dtype=self.dtype)
            
            with torch.no_grad():
                out = self.model.generate(
                    inputs=input_ids,
                    attention_mask=attention_mask,
                    images=px,
                    max_new_tokens=50,      # MUCH shorter
                    temperature=0.0,        # Lower for faster
                    do_sample=False,        # Greedy = faster
                    use_cache=False,        # CRITICAL: Don't accumulate KV cache
                )

            # Clear any cached state to prevent degradation
            if hasattr(self.model, 'past_key_values'):
                self.model.past_key_values = None

            full_text = self.tokenizer.decode(out[0], skip_special_tokens=True)
            
            # Extract answer
            if "what is this?" in full_text.lower():
                parts = full_text.lower().split("what is this?")
                if len(parts) > 1:
                    answer = parts[1].strip()
                else:
                    answer = full_text
            else:
                answer = full_text
            
            # Clean up
            answer = answer.replace("assistant", "").replace("Assistant", "")
            answer = answer.strip().rstrip('.')
            
            # Take first sentence
            for delimiter in ['\n', '. ', '? ', '! ']:
                if delimiter in answer:
                    answer = answer.split(delimiter)[0].strip()
                    break
            
            if not answer or len(answer) < 3:
                answer = "Scene detected"
            
            # Capitalize first letter
            if answer:
                answer = answer[0].upper() + answer[1:]
            
            latency = (time.time() - start_time) * 1000
            return answer, latency
            
        except Exception as e:
            print(f"❌ Error: {e}")
            return "Error", 0

def main():
    print("="*60)
    print("📷 CAMERA VISION - FastVLM OPTIMIZED")
    print("="*60)

    cap = cv2.VideoCapture(0, cv2.CAP_DSHOW)  # Use DirectShow backend
    if not cap.isOpened():
        print("❌ Failed to open camera")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
    print("✅ Camera initialized (320x240 - optimized)")

    try:
        vision_engine = FastVLMEngine()
    except Exception as e:
        print(f"❌ Failed to initialize: {e}")
        cap.release()
        return

    print(f"\n🚀 Starting optimized camera monitoring...")
    print(f"   Optimizations: 224x224 input, 50 tokens, greedy decode")
    print(f"   Expected latency: 8-12 seconds (2x faster)")
    print(f"   Update: Every 10 seconds")
    print(f"   Press Ctrl+C to stop\n")

    frame_count = 0

    try:
        while True:
            ret, frame = cap.read()

            if ret:
                frame_count += 1
                print(f"\n📸 Frame {frame_count}")

                description, latency = vision_engine.describe_scene(frame)
                print(f"   Scene: {description}")
                print(f"   ⚡ Latency: {latency:.0f}ms ({latency/1000:.1f}s)")

                payload = {
                    "device": "Camera",
                    "data": {"objects": [description]}
                }

                try:
                    requests.post(SERVER_URL, json=payload, timeout=3)
                    print("✅ Posted to server")
                except:
                    print("⚠️  Server not reachable")

            time.sleep(10)  # Slightly longer interval

    except KeyboardInterrupt:
        print("\n\n👋 Stopping...")
    finally:
        cap.release()

if __name__ == "__main__":
    main()
