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

# OpenAI API configuration
api_key = os.getenv("OPENAI_API_KEY", "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")
client = OpenAI(api_key=api_key)

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
        screen_active = global_context.get("screen", {}).get("active_app", {})
        screen_apps = global_context.get("screen", {}).get("apps", [])
        voice_text = global_context.get("voice", {}).get("text", "")
        camera_objs = global_context.get("camera", {}).get("objects", [])

        # ---------------- Detailed Fusion ----------------
        fusion_lines = []
        if screen_active:
            fusion_lines.append(f"Active App: {screen_active.get('name')} — {screen_active.get('window')}")
        else:
            fusion_lines.append("No active app detected")

        if screen_apps:
            other_names = [app['name'] for app in screen_apps if app['name'] != screen_active.get("name")]
            if other_names:
                fusion_lines.append("Other Apps: " + ", ".join(other_names))

        if voice_text:
            fusion_lines.append(f"Voice Context: \"{voice_text}\"")

        if camera_objs:
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
        if camera_objs:
            summary_parts.append(f"camera: {', '.join(camera_objs)}")

        if not summary_parts:
            fusion_summary = "Nova is perceiving your environment, but no active signals yet."
        else:
            fusion_summary = " | ".join(summary_parts)

        global_context["fusion_summary"] = fusion_summary

        # ---------------- Recommendations ----------------
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
