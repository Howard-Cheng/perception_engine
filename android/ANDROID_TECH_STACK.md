# Android Perception Engine - Tech Stack

**Version:** 1.0
**Target:** Android 10+ (API 29+)
**Architecture:** Single-device perception with local fusion
**Last Updated:** 2025-10-07

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Technology Stack](#technology-stack)
4. [Perception Components](#perception-components)
5. [Models & Libraries](#models--libraries)
6. [Project Structure](#project-structure)
7. [Performance Targets](#performance-targets)
8. [Implementation Roadmap](#implementation-roadmap)

---

## Overview

### **Design Philosophy**

- ✅ **On-device processing** - All perception and fusion happens locally
- ✅ **Battery-efficient** - Optimized for mobile battery life
- ✅ **Privacy-first** - No data leaves the device
- ✅ **CPU-optimized** - Works on phones without dedicated NPU
- ✅ **Modular design** - Easy to enable/disable perception sources

### **Core Capabilities**

The Android Perception Engine collects context from 5 sources and generates AI-powered recommendations:

1. **Screen Activity** - Active app, screen content (via Accessibility Service)
2. **Screen OCR** - Extract text from screen using Google ML Kit
3. **Camera Vision** - Environment understanding via vision-language model
4. **Microphone** - User speech transcription
5. **System Sensors** - Battery, network, location, device state

---

## Architecture

### **High-Level Architecture**

```
┌─────────────────────────────────────────────────────────────┐
│          Android Perception Engine (Single Device)          │
└─────────────────────────────────────────────────────────────┘

┌────────────────────────┐     ┌─────────────────────────────┐
│  Perception Layer      │────▶│  Fusion Layer               │
│  (Context Collection)  │     │  (Local Processing)         │
└────────────────────────┘     └──────────────┬──────────────┘
         │                                     │
         │                                     ▼
         │                      ┌──────────────────────────────┐
         │                      │  AI Recommendation Engine    │
         │                      │  (GPT-4o-mini via API)       │
         │                      └──────────────┬───────────────┘
         │                                     │
         ▼                                     ▼
┌────────────────────────────────────────────────────────────┐
│              Local Context Store (Room Database)            │
└────────────────────────────────────────────────────────────┘
         │
         ▼
┌────────────────────────────────────────────────────────────┐
│                 UI Layer (Jetpack Compose)                  │
│  - Real-time context display                                │
│  - AI recommendations                                       │
│  - Settings & controls                                      │
└────────────────────────────────────────────────────────────┘
```

### **Data Flow**

```
Screen Activity ──┐
Screen OCR ───────┤
Camera Vision ────┼──▶ Context Collector ──▶ Local Fusion ──▶ OpenAI API
Microphone ───────┤                                │
System Sensors ───┘                                │
                                                   ▼
                                        AI Recommendations
                                                   │
                                                   ▼
                                           Display in UI
```

---

## Technology Stack

### **Core Framework**

| Component | Technology | Version | Purpose |
|-----------|-----------|---------|---------|
| **Language** | Kotlin | 2.0+ | Primary development language |
| **Build System** | Gradle | 8.5+ | Build automation |
| **Min SDK** | Android 10 | API 29 | Minimum supported version |
| **Target SDK** | Android 14 | API 34 | Target version |
| **Architecture** | arm64-v8a | 64-bit | Target CPU architecture |

### **Android Libraries**

| Library | Purpose | Implementation |
|---------|---------|----------------|
| **Jetpack Compose** | Modern UI framework | `androidx.compose.ui:ui:1.6.0` |
| **Room Database** | Local context storage | `androidx.room:room-runtime:2.6.0` |
| **WorkManager** | Background tasks | `androidx.work:work-runtime-ktx:2.9.0` |
| **CameraX** | Camera API | `androidx.camera:camera-camera2:1.3.0` |
| **Hilt** | Dependency injection | `com.google.dagger:hilt-android:2.48` |
| **Kotlin Coroutines** | Async operations | `org.jetbrains.kotlinx:kotlinx-coroutines:1.7.3` |

### **ML & Inference**

| Component | Library | Purpose |
|-----------|---------|---------|
| **TensorFlow Lite** | `org.tensorflow:tensorflow-lite:2.14.0` | ML inference engine |
| **Google ML Kit** | `com.google.mlkit:text-recognition:16.0.0` | On-device OCR |
| **ONNX Runtime Mobile** | `com.microsoft.onnxruntime:onnxruntime-android:1.17.0` | Alternative ML runtime |

### **Networking**

| Component | Library | Purpose |
|-----------|---------|---------|
| **Ktor Client** | `io.ktor:ktor-client-android:2.3.7` | HTTP client for OpenAI API |
| **Kotlinx Serialization** | `org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.2` | JSON serialization |

---

## Perception Components

### **1. Screen Activity Monitor**

**Technology:** Android Accessibility Service

**Purpose:** Monitor active app and screen state changes

**Implementation:**
```kotlin
class ScreenMonitorService : AccessibilityService() {
    override fun onAccessibilityEvent(event: AccessibilityEvent) {
        when (event.eventType) {
            AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED -> {
                val packageName = event.packageName?.toString()
                val className = event.className?.toString()
                val windowTitle = event.text.firstOrNull()?.toString()

                // Collect context
                contextCollector.updateScreenContext(
                    packageName = packageName,
                    className = className,
                    windowTitle = windowTitle
                )
            }
        }
    }
}
```

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.BIND_ACCESSIBILITY_SERVICE" />
```

**Latency:** ~5ms (event-driven)

---

### **2. Screen OCR**

**Technology:** Google ML Kit Text Recognition

**Purpose:** Extract text from active screen for content understanding

**Implementation:**
```kotlin
class ScreenOCREngine(private val context: Context) {
    private val recognizer = TextRecognition.getClient(
        TextRecognizerOptions.DEFAULT_OPTIONS
    )

    suspend fun extractScreenText(): String = suspendCoroutine { continuation ->
        // Requires MediaProjection API to capture screen
        val screenshot = captureScreen() // Bitmap

        val image = InputImage.fromBitmap(screenshot, 0)
        recognizer.process(image)
            .addOnSuccessListener { visionText ->
                val text = visionText.textBlocks
                    .joinToString("\n") { it.text }
                continuation.resume(text)
            }
            .addOnFailureListener { e ->
                continuation.resumeWithException(e)
            }
    }
}
```

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />
```

**Model:** Google ML Kit (on-device, no download needed)
**Latency:** 100-150ms
**CPU Usage:** Low (optimized by Google)

---

### **3. Camera Vision**

**Technology:** CameraX + TensorFlow Lite (MobileViT or similar lightweight VLM)

**Purpose:** Generate natural language descriptions of environment

**Model Options:**

| Model | Size | Latency (CPU) | Quality |
|-------|------|---------------|---------|
| **MobileViT-Small** | 25MB | 300-500ms | Good |
| **Phi-3 Vision (quantized)** | 150MB | 800-1200ms | Better |
| **Cloud API fallback** | - | 500-1000ms | Best |

**Recommended:** MobileViT-Small for on-device, with optional cloud fallback

**Implementation:**
```kotlin
class CameraVisionEngine(private val context: Context) {
    private lateinit var interpreter: Interpreter

    init {
        // Load TFLite model
        val model = loadModelFile("mobilevit_vision.tflite")
        interpreter = Interpreter(model)
    }

    fun generateCaption(bitmap: Bitmap): String {
        // Preprocess image
        val input = preprocessImage(bitmap, inputSize = 224)

        // Run inference
        val output = Array(1) { FloatArray(vocabSize) }
        interpreter.run(input, output)

        // Decode to text
        return decodeOutput(output[0])
    }
}
```

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.CAMERA" />
```

**Update Frequency:** Every 5-10 seconds (battery optimization)

---

### **4. Microphone (Speech Recognition)**

**Technology:** Android SpeechRecognizer (system STT) or Vosk (offline)

**Purpose:** Capture user speech for intent understanding

**Option A: System SpeechRecognizer (Recommended)**
```kotlin
class MicrophoneMonitor(private val context: Context) {
    private val speechRecognizer = SpeechRecognizer.createSpeechRecognizer(context)

    fun startListening() {
        val intent = Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
            putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                    RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
            putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
        }

        speechRecognizer.setRecognitionListener(object : RecognitionListener {
            override fun onResults(results: Bundle) {
                val matches = results.getStringArrayList(
                    SpeechRecognizer.RESULTS_RECOGNITION
                )
                val transcript = matches?.firstOrNull() ?: ""

                // Post to context collector
                contextCollector.updateVoiceContext(transcript)
            }
        })

        speechRecognizer.startListening(intent)
    }
}
```

**Option B: Vosk (Offline, Privacy-focused)**
- Model: vosk-model-small-en-us-0.15 (40MB)
- Fully offline, no internet required
- Slightly lower quality than system STT

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.RECORD_AUDIO" />
```

**Latency:** 150-300ms
**Recommended:** System SpeechRecognizer (uses device's built-in STT)

---

### **5. System Sensors**

**Technology:** Android System APIs

**Purpose:** Device state context (battery, network, location)

**Implementation:**
```kotlin
class SystemSensorCollector(private val context: Context) {
    fun collectSystemContext(): SystemContext {
        return SystemContext(
            battery = getBatteryLevel(),
            isCharging = isCharging(),
            networkType = getNetworkType(),
            location = getLocation(),
            timeOfDay = getTimeOfDay(),
            deviceState = getDeviceState() // locked, unlocked, screen on/off
        )
    }

    private fun getBatteryLevel(): Int {
        val batteryManager = context.getSystemService(Context.BATTERY_SERVICE)
            as BatteryManager
        return batteryManager.getIntProperty(
            BatteryManager.BATTERY_PROPERTY_CAPACITY
        )
    }

    private fun getNetworkType(): String {
        val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE)
            as ConnectivityManager
        val network = cm.activeNetwork ?: return "None"
        val capabilities = cm.getNetworkCapabilities(network) ?: return "Unknown"

        return when {
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "WiFi"
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "Cellular"
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> "Ethernet"
            else -> "Unknown"
        }
    }
}
```

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
```

**Latency:** <5ms (cached system state)

---

## Models & Libraries

### **Required Models**

| Model | Purpose | Size | Download |
|-------|---------|------|----------|
| **Google ML Kit Text Recognition** | Screen OCR | Built-in | Automatic |
| **MobileViT-Small (TFLite)** | Camera vision | 25MB | Manual download |
| **Vosk Model (optional)** | Offline ASR | 40MB | https://alphacephei.com/vosk/models |

### **Model Conversion (if needed)**

**Convert PyTorch to TFLite:**
```python
# Example: Convert vision model to TFLite
import torch
import tensorflow as tf

# 1. Export to ONNX
torch_model.eval()
torch.onnx.export(torch_model, dummy_input, "model.onnx")

# 2. Convert ONNX to TFLite
import onnx
from onnx_tf.backend import prepare

onnx_model = onnx.load("model.onnx")
tf_rep = prepare(onnx_model)
tf_rep.export_graph("model_tf")

# 3. Quantize to INT8
converter = tf.lite.TFLiteConverter.from_saved_model("model_tf")
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()

with open("model_quantized.tflite", "wb") as f:
    f.write(tflite_model)
```

---

## Project Structure

```
android/
├── app/
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── kotlin/com/nova/perception/
│   │   │   ├── MainActivity.kt
│   │   │   │
│   │   │   ├── perception/              # Perception layer
│   │   │   │   ├── screen/
│   │   │   │   │   ├── ScreenMonitorService.kt
│   │   │   │   │   └── ScreenOCREngine.kt
│   │   │   │   ├── camera/
│   │   │   │   │   ├── CameraVisionEngine.kt
│   │   │   │   │   └── CameraManager.kt
│   │   │   │   ├── audio/
│   │   │   │   │   ├── MicrophoneMonitor.kt
│   │   │   │   │   └── VoskEngine.kt (optional)
│   │   │   │   └── sensors/
│   │   │   │       └── SystemSensorCollector.kt
│   │   │   │
│   │   │   ├── fusion/                  # Fusion layer
│   │   │   │   ├── ContextCollector.kt
│   │   │   │   ├── LocalFusionEngine.kt
│   │   │   │   └── SemanticExtractor.kt
│   │   │   │
│   │   │   ├── ai/                      # AI layer
│   │   │   │   ├── OpenAIClient.kt
│   │   │   │   └── RecommendationEngine.kt
│   │   │   │
│   │   │   ├── data/                    # Data layer
│   │   │   │   ├── local/
│   │   │   │   │   ├── ContextDatabase.kt
│   │   │   │   │   ├── ContextDao.kt
│   │   │   │   │   └── entities/
│   │   │   │   └── models/
│   │   │   │       ├── PerceptionContext.kt
│   │   │   │       └── Recommendation.kt
│   │   │   │
│   │   │   ├── ui/                      # UI layer
│   │   │   │   ├── home/
│   │   │   │   │   ├── HomeScreen.kt
│   │   │   │   │   └── HomeViewModel.kt
│   │   │   │   ├── components/
│   │   │   │   │   ├── ContextCard.kt
│   │   │   │   │   └── RecommendationCard.kt
│   │   │   │   └── theme/
│   │   │   │       └── Theme.kt
│   │   │   │
│   │   │   └── di/                      # Dependency injection
│   │   │       ├── AppModule.kt
│   │   │       └── PerceptionModule.kt
│   │   │
│   │   ├── res/
│   │   │   ├── xml/
│   │   │   │   └── accessibility_service_config.xml
│   │   │   └── values/
│   │   │       └── strings.xml
│   │   │
│   │   └── assets/
│   │       └── models/
│   │           ├── mobilevit_vision.tflite
│   │           └── vosk-model/ (optional)
│   │
│   └── build.gradle.kts
│
├── gradle/
│   └── libs.versions.toml
├── build.gradle.kts
└── settings.gradle.kts
```

---

## Performance Targets

### **Latency Goals**

| Component | Target | Measurement |
|-----------|--------|-------------|
| **Screen Monitor** | < 10ms | Event callback → context updated |
| **Screen OCR** | < 150ms | Capture → text extracted |
| **Camera Vision** | < 500ms | Frame → caption generated |
| **Audio ASR** | < 300ms | Speech → transcript |
| **Local Fusion** | < 50ms | Raw contexts → fused context |
| **AI Recommendations** | < 2s | API call → response |
| **End-to-End** | < 3s | Event → recommendations displayed |

### **Resource Limits**

| Metric | Target | Critical Threshold |
|--------|--------|--------------------|
| **Battery Drain** | < 5% per hour | > 10% per hour |
| **CPU Usage (Idle)** | < 5% | > 15% |
| **CPU Usage (Active)** | < 25% | > 50% |
| **Memory (Base)** | < 100MB | > 200MB |
| **Memory (Peak)** | < 250MB | > 500MB |
| **Network (AI API)** | < 10KB per request | - |

### **Battery Optimization Strategies**

1. **Adaptive Sampling**
   - Screen: Event-driven (no polling)
   - Camera: Only when app is visible (5-10s intervals)
   - Audio: On-demand activation (not continuous)
   - Sensors: Batch updates every 30s

2. **WorkManager for Background Tasks**
   - Schedule periodic context collection
   - Use constraints (charging, WiFi, etc.)
   - Respect Doze and App Standby

3. **Model Optimization**
   - Use INT8 quantized models
   - Batch inference when possible
   - Cache results to avoid re-computation

---

## Implementation Roadmap

### **Phase 1: Core Infrastructure (Week 1-2)**

**Goal:** Set up project structure and basic perception

**Tasks:**
- ✅ Create Android project with Compose + Hilt
- ✅ Implement Screen Monitor (Accessibility Service)
- ✅ Implement System Sensor Collector
- ✅ Create Room database for context storage
- ✅ Build basic UI with Compose

**Deliverable:** App can monitor active app and collect system state

---

### **Phase 2: Screen OCR (Week 2-3)**

**Goal:** Extract text from screen using ML Kit

**Tasks:**
- ✅ Integrate Google ML Kit Text Recognition
- ✅ Implement MediaProjection for screen capture
- ✅ Add OCR result to context collector
- ✅ Display OCR results in UI

**Deliverable:** App can extract and display screen text

---

### **Phase 3: Camera Vision (Week 3-4)**

**Goal:** Generate environment captions

**Tasks:**
- ✅ Integrate CameraX
- ✅ Download and integrate MobileViT TFLite model
- ✅ Implement vision inference pipeline
- ✅ Add camera captions to context

**Deliverable:** App generates natural language environment descriptions

---

### **Phase 4: Audio (Week 4-5)**

**Goal:** Transcribe user speech

**Tasks:**
- ✅ Integrate Android SpeechRecognizer
- ✅ (Optional) Integrate Vosk for offline ASR
- ✅ Implement audio capture + transcription
- ✅ Add transcripts to context

**Deliverable:** App transcribes user speech in real-time

---

### **Phase 5: Fusion & AI (Week 5-6)**

**Goal:** Combine contexts and generate recommendations

**Tasks:**
- ✅ Implement local fusion engine
- ✅ Integrate OpenAI API client (Ktor)
- ✅ Build recommendation engine
- ✅ Display recommendations in UI

**Deliverable:** Full end-to-end perception → recommendations pipeline

---

### **Phase 6: Polish & Optimization (Week 6-7)**

**Goal:** Battery optimization and UX refinement

**Tasks:**
- ✅ Implement battery optimization strategies
- ✅ Add settings for enable/disable perception sources
- ✅ Implement WorkManager for background tasks
- ✅ Add error handling and retry logic
- ✅ Performance profiling and optimization

**Deliverable:** Production-ready Android app

---

## Development Setup

### **Prerequisites**

- Android Studio Hedgehog (2023.1.1) or later
- JDK 17
- Android SDK 34
- Git

### **Initial Setup**

```bash
# Clone repository
git clone https://github.com/your-org/perception_engine.git
cd perception_engine/android

# Open in Android Studio
# File → Open → Select android/ directory

# Sync Gradle
# Build → Make Project

# Run on device/emulator
# Run → Run 'app'
```

### **API Keys**

Add to `local.properties`:
```properties
OPENAI_API_KEY=sk-...
```

Access in code:
```kotlin
val apiKey = BuildConfig.OPENAI_API_KEY
```

---

## Testing Strategy

### **Unit Tests**
- Test individual perception engines
- Mock Android APIs
- Validate fusion logic

### **Instrumented Tests**
- Test Accessibility Service
- Test camera and audio capture
- Test Room database operations

### **Performance Tests**
- Measure latency for each component
- Profile memory usage
- Battery drain tests (use Battery Historian)

---

## Security & Privacy

### **Privacy Principles**

1. **On-device processing** - All perception happens locally
2. **User control** - Easy to disable any perception source
3. **Minimal API calls** - Only send fused context to OpenAI (not raw data)
4. **No persistent storage of sensitive data** - Clear context after use
5. **Permissions transparency** - Explain why each permission is needed

### **Required Permissions**

```xml
<!-- Screen monitoring -->
<uses-permission android:name="android.permission.BIND_ACCESSIBILITY_SERVICE" />

<!-- Screen capture (for OCR) -->
<uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />

<!-- Camera -->
<uses-permission android:name="android.permission.CAMERA" />

<!-- Microphone -->
<uses-permission android:name="android.permission.RECORD_AUDIO" />

<!-- Network (for AI API) -->
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />

<!-- Location (optional) -->
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

<!-- Foreground service (for background perception) -->
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
```

---

## FAQ

### **Q: Why not use Whisper for audio?**
**A:** Android's built-in SpeechRecognizer is optimized for mobile and uses device's native STT (Google, Samsung, etc.). It's more battery-efficient than running Whisper on-device. Vosk is recommended only if offline/privacy is critical.

### **Q: Can we use larger vision models?**
**A:** Yes, but be mindful of battery drain. Models > 100MB may be too slow for mobile CPUs. Consider cloud API fallback for better quality.

### **Q: How to handle permissions?**
**A:** Request permissions at runtime with clear explanations. Allow users to selectively enable/disable perception sources in settings.

### **Q: What about Android 15+?**
**A:** Architecture is forward-compatible. Some APIs may change (e.g., screen capture restrictions), but core design remains valid.

### **Q: Can this work offline?**
**A:** Mostly yes - all perception is on-device. Only AI recommendations require internet (OpenAI API). You can implement local LLM as fallback.

---

## Resources

### **Documentation**
- [Android Accessibility Service Guide](https://developer.android.com/guide/topics/ui/accessibility/service)
- [Google ML Kit Text Recognition](https://developers.google.com/ml-kit/vision/text-recognition)
- [CameraX Documentation](https://developer.android.com/training/camerax)
- [TensorFlow Lite Android](https://www.tensorflow.org/lite/android)

### **Sample Code**
- [Android Accessibility Service Sample](https://github.com/android/accessibility-samples)
- [ML Kit Samples](https://github.com/googlesamples/mlkit)
- [CameraX Samples](https://github.com/android/camera-samples)

### **Models**
- [TensorFlow Lite Model Zoo](https://www.tensorflow.org/lite/models)
- [Vosk Models](https://alphacephei.com/vosk/models)
- [ONNX Model Zoo](https://github.com/onnx/models)

---

**Questions?** Reach out to the team or create an issue.

**License:** Proprietary

**Maintained by:** Nova Perception Engine Team
