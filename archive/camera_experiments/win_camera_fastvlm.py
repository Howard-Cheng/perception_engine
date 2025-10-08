import time
import requests
import cv2
import torch
from PIL import Image
from transformers import AutoTokenizer, AutoModelForCausalLM

SERVER_URL = "http://127.0.0.1:8000/update_context"
IMAGE_TOKEN_INDEX = -200

class FastVLMEngine:
    def __init__(self):
        print("🔧 Loading FastVLM-0.5B model...")
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
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame_rgb)
            
            # Simple direct prompt - just ask what's in the image
            messages = [{"role": "user", "content": "<image>\nWhat do you see?"}]
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
                    max_new_tokens=50,
                    temperature=0.3,
                    do_sample=True,
                )
            
            # Get full decoded text
            full_text = self.tokenizer.decode(out[0], skip_special_tokens=True)
            
            # Extract answer after "What do you see?"
            if "What do you see?" in full_text:
                # Split and get the part after the question
                parts = full_text.split("What do you see?")
                if len(parts) > 1:
                    answer = parts[1].strip()
                else:
                    answer = full_text
            else:
                answer = full_text
            
            # Clean up common artifacts
            answer = answer.replace("assistant", "").replace("Assistant", "")
            answer = answer.replace("<|im_end|>", "").replace("<|endoftext|>", "")
            
            # Take first sentence only
            for delimiter in ['\n', '. ', '? ', '! ']:
                if delimiter in answer:
                    answer = answer.split(delimiter)[0].strip()
                    break
            
            # Final cleanup
            answer = answer.strip().rstrip('.')
            if len(answer) > 120:
                answer = answer[:117] + "..."
            
            if not answer or len(answer) < 5:
                answer = "Unable to describe scene"
            
            latency = (time.time() - start_time) * 1000
            return answer, latency
            
        except Exception as e:
            print(f"❌ Vision error: {e}")
            import traceback
            traceback.print_exc()
            return "Error generating description", 0

def main():
    print("="*60)
    print("📷 WINDOWS CAMERA VISION - FastVLM")
    print("="*60)
    
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("❌ Failed to open camera")
        return
    
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    print("✅ Camera initialized (640x480)")
    
    try:
        vision_engine = FastVLMEngine()
    except Exception as e:
        print(f"❌ Failed to initialize FastVLM: {e}")
        cap.release()
        return
    
    print(f"\n🚀 Starting camera monitoring...")
    print(f"   Update frequency: Every 8 seconds")
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
                print(f"   Latency: {latency:.0f}ms")
                
                payload = {
                    "device": "Camera",
                    "data": {"objects": [description]}
                }
                
                try:
                    resp = requests.post(SERVER_URL, json=payload, timeout=3)
                    if resp.status_code == 200:
                        print("✅ Posted to server")
                except:
                    print("⚠️ Server not reachable")
            
            time.sleep(8)
    
    except KeyboardInterrupt:
        print("\n\n👋 Stopping camera vision client...")
    finally:
        cap.release()
        print("✅ Camera released")

if __name__ == "__main__":
    main()
