import os
import time
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from openai import OpenAI

# -----------------------------
# CONFIG
# -----------------------------

# OpenAI API configuration - load from environment variable
api_key = os.getenv("OPENAI_API_KEY")
if not api_key:
    print("⚠️  WARNING: OPENAI_API_KEY environment variable not set!")
    print("   Fusion engine will not work without it.")
    print("   Set it with: $env:OPENAI_API_KEY='your-key-here'")
    client = None
else:
    try:
        client = OpenAI(api_key=api_key)
        print("✅ OpenAI client initialized successfully")
    except Exception as e:
        print(f"❌ Failed to initialize OpenAI client: {e}")
        print("   Try upgrading: pip install --upgrade openai httpx")
        client = None

app = FastAPI()

# Static folder (for Ferrari logo etc.)
static_dir = os.path.join(os.path.dirname(__file__), "static")
if not os.path.exists(static_dir):
    os.makedirs(static_dir)

app.mount("/static", StaticFiles(directory=static_dir), name="static")

base_dir = os.path.dirname(os.path.abspath(__file__))
templates = Jinja2Templates(directory=os.path.join(base_dir, "templates"))

# -----------------------------
# GLOBAL CONTEXT
# -----------------------------

global_context = {
    "screen": {},
    "voice": {},
    "camera": {},
    "fusion_detailed": "",     # full breakdown for Fusion card
    "fusion_summary": "",      # one-liner for banner
    "recommendations": [],
    "latency": 0.0,
    "events_processed": 0,
    "last_updated": None,
}

# -----------------------------
# UPDATE CONTEXT ENDPOINT
# -----------------------------

@app.post("/update_context")
async def update_context(request: Request):
    data = await request.json()
    device = data.get("device")
    payload = data.get("data", {})

    global_context[device.lower()] = payload
    global_context["events_processed"] += 1

    # Run fusion + recommendations each update
    start = time.time()
    run_fusion_and_recommendations()
    global_context["latency"] = time.time() - start

    return {"status": "ok", "fused_context": global_context["fusion_detailed"]}

# -----------------------------
# FUSION ENGINE + RECOMMENDATIONS
# -----------------------------

def run_fusion_and_recommendations():
    """Fuse perception signals into a crisp context + actionable recs."""
    try:
        screen_data = global_context.get("screen", {})
        screen_active = screen_data.get("active_app", {})
        screen_apps = screen_data.get("apps", [])
        screen_blip = screen_data.get("screen_caption_blip", "")  # BLIP caption (demo)
        screen_text = screen_data.get("text_detected", "")  # OCR text
        voice_text = global_context.get("voice", {}).get("text", "")

        # Camera context - support both old (objects) and new (caption + OCR) formats
        camera_data = global_context.get("camera", {})
        camera_objs = camera_data.get("objects", [])  # Old format (YOLO)
        camera_caption = camera_data.get("environment_caption", "")  # New format (vision)
        camera_ocr = camera_data.get("text_detected", "")  # New format (OCR detection)

        # ---------------- Detailed Fusion ----------------
        fusion_lines = []
        if screen_active:
            fusion_lines.append(f"Active App: {screen_active.get('name')} — {screen_active.get('window')}")
        else:
            fusion_lines.append("No active app detected")

        if screen_apps:
            other_names = [app['name'] for app in screen_apps if app['name'] != screen_active.get("name")]
            if other_names:
                fusion_lines.append("Other Apps: " + ", ".join(other_names[:5]))  # Limit to 5

        # Screen content (BLIP or OCR)
        if screen_blip:
            fusion_lines.append(f"Screen BLIP Caption: {screen_blip}")
        if screen_text:
            fusion_lines.append(f"Screen Text (OCR): {screen_text[:200]}")  # Limit to 200 chars

        if voice_text:
            fusion_lines.append(f"Voice Context: \"{voice_text}\"")

        # Camera perception (new or old format)
        if camera_caption:
            ocr_note = f" (text: {camera_ocr})" if camera_ocr and camera_ocr != "no_text" else ""
            fusion_lines.append(f"Camera: {camera_caption}{ocr_note}")
        elif camera_objs:
            fusion_lines.append("Camera sees: " + ", ".join(camera_objs))

        if not fusion_lines:
            fusion_lines.append("No strong perception context available.")

        global_context["fusion_detailed"] = "\n".join(fusion_lines)

        # ---------------- Fusion Summary (Banner) ----------------
        summary_parts = []
        if screen_active:
            summary_parts.append(f"Focused on {screen_active.get('name')}")
        if voice_text:
            summary_parts.append(f"heard: \"{voice_text}\"")
        if camera_caption:
            summary_parts.append(f"camera: {camera_caption}")
        elif camera_objs:
            summary_parts.append(f"camera: {', '.join(camera_objs)}")

        if not summary_parts:
            fusion_summary = "Nova is perceiving your environment, but no active signals yet."
        else:
            fusion_summary = " | ".join(summary_parts)

        global_context["fusion_summary"] = fusion_summary

        # ---------------- Recommendations ----------------
        if client is not None:
            user_prompt = (
                "You are the Nova Recommendation Engine.\n"
                "Generate exactly 3 crisp, executive-level actionable recommendations.\n"
                "Do not explain — only output numbered bullet points.\n\n"
                f"Fusion Context:\n{global_context['fusion_detailed']}\n"
            )

            resp = client.chat.completions.create(
                model="gpt-4o-mini",
                messages=[{"role": "system", "content": user_prompt}],
                max_tokens=150,
                temperature=0.5,
            )

            recs_text = resp.choices[0].message.content.strip()
            recs = [line.strip(" -") for line in recs_text.split("\n") if line.strip()]

            global_context["recommendations"] = recs[:3]
        else:
            # Fallback when OpenAI client not available
            global_context["recommendations"] = [
                "OpenAI API not configured - set OPENAI_API_KEY",
                "Fusion working, but recommendations require API key",
                "See server logs for setup instructions"
            ]

        global_context["last_updated"] = time.strftime("%Y-%m-%d %H:%M:%S")

    except Exception as e:
        global_context["fusion_detailed"] = f"Fusion error: {e}"
        global_context["fusion_summary"] = "Fusion error"
        global_context["recommendations"] = []


# -----------------------------
# DASHBOARD
# -----------------------------

@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request):
    return templates.TemplateResponse(
        "dashboard.html",
        {
            "request": request,
            "context": global_context,
        },
    )


# -----------------------------
# DEBUG API
# -----------------------------

@app.get("/get_context")
async def get_context():
    return JSONResponse(content=global_context, indent=2)


# -----------------------------
# RUN SERVER
# -----------------------------

if __name__ == "__main__":
    import uvicorn
    print("\n" + "="*60)
    print("🚀 NOVA PERCEPTION ENGINE - FUSION SERVER")
    print("="*60)
    print(f"📊 Dashboard: http://127.0.0.1:8000/dashboard")
    print(f"🔍 Debug API: http://127.0.0.1:8000/get_context")
    print("="*60 + "\n")

    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="info")
