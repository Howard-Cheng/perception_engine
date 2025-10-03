# Nova Perception Engine - Production Tech Stack

**Version:** 2.0 (Production Architecture - Performance Optimized)
**Target Platforms:** Windows Desktop, Android Mobile & Tablet
**Last Updated:** October 2025
**Performance Target:** < 1700ms end-to-end latency

---

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Client Applications](#client-applications)
3. [Sync Infrastructure](#sync-infrastructure)
4. [Fusion Engine](#fusion-engine)
5. [AI/ML Stack](#aiml-stack)
6. [Development Tools](#development-tools)
7. [Deployment](#deployment)
8. [Performance Targets](#performance-targets)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     Cloud Sync Hub (AWS)                        │
│  • WebSocket Server (Ktor + Netty)                              │
│  • Device State Management                                      │
│  • Global Context Fusion                                        │
│  • Message Routing & Broadcast                                  │
└──────────────────┬──────────────────────────────────────────────┘
                   │ WebSocket (200ms)
          ┌────────┴────────┐
          │                 │
┌─────────▼──────┐  ┌──────▼─────────┐
│ Windows Client │  │ Android Client │
│ (Native C#)    │  │ (Native Kotlin)│
└────────────────┘  └────────────────┘
     │                    │
     │ P2P Optimization   │
     │ (LAN: 20ms)        │
     └────────────────────┘
```

**Key Design Principles:**
- **Local-first:** Context collection and local fusion happen on-device
- **Cloud-augmented:** Cross-device fusion and AI recommendations via cloud
- **Hybrid sync:** P2P when possible, cloud as fallback
- **Event-driven:** Real-time context updates, not polling

---

## Client Applications

### Windows Desktop Client

**Language & Runtime:**
- **Primary Language:** Modern C++20
- **Compiler:** MSVC 19.38+ (Visual Studio 2022)
- **UI Framework:** Qt 6.6 (cross-platform) OR native Win32/WinUI 3
- **Target:** Windows 10 22H2+, Windows 11 (x64)
- **Build System:** CMake 3.27 + vcpkg

**Why C++:** Maximum performance (55ms context collection vs 59ms C#), zero GC pauses, tight memory control

---

#### **Perception Components**

| Component | Technology | Model/Library | Latency | Purpose |
|-----------|-----------|---------------|---------|---------|
| **Screen Monitoring** | Win32 API (native) | SetWinEventHook | 2μs | Active window change detection |
| **Screen OCR** | ONNX Runtime + DirectML | PaddleOCR v4 | 50ms | Extract text from screen |
| **Camera** | DirectShow API | Native capture | 15ms | Camera frame acquisition |
| **Object Detection** | ONNX Runtime | YOLOv8n (optional) | 30ms | Camera feed analysis |
| **Voice Recognition** | Azure Speech SDK | Streaming STT | 100ms | Real-time speech-to-text |
| **Networking** | Boost.Beast | WebSocket++ | 50ms | Cloud sync |
| **Serialization** | RapidJSON | - | 2ms | JSON encode/decode |
| **Logging** | spdlog | - | <1ms | High-perf logging |

#### **1. Screen Monitoring (Event-Driven)**

```cpp
#include <windows.h>

class ScreenMonitor {
private:
    HWINEVENTHOOK m_hookHandle;

    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hook, DWORD event, HWND hwnd,
        LONG idObject, LONG idChild,
        DWORD idEventThread, DWORD dwmsEventTime
    ) {
        // Fires immediately on window change (2μs latency)
        HWND foreground = GetForegroundWindow();

        // Get window title
        wchar_t title[256];
        GetWindowTextW(foreground, title, 256);

        // Get process name
        DWORD processId;
        GetWindowThreadProcessId(foreground, &processId);

        // Trigger OCR capture
        OnScreenChanged(title, processId);
    }

public:
    void Initialize() {
        m_hookHandle = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND,  // Window focus change
            EVENT_SYSTEM_FOREGROUND,
            NULL,
            WinEventProc,
            0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
        );
    }
};
```

**Performance:** 2μs event callback → 10ms screen capture → 50ms OCR = **60ms total**

---

#### **2. Screen OCR (PaddleOCR via ONNX Runtime)**

```cpp
#include <onnxruntime/onnxruntime_cxx_api.h>

class OCREngine {
private:
    Ort::Env m_env;
    Ort::Session* m_detSession;
    Ort::Session* m_recSession;

public:
    OCREngine() {
        Ort::SessionOptions sessionOpts;
        sessionOpts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);

        // Use DirectML for GPU acceleration
        Ort::ThrowOnError(
            OrtSessionOptionsAppendExecutionProvider_DML(sessionOpts, 0)
        );

        m_detSession = new Ort::Session(m_env, L"paddle_det.onnx", sessionOpts);
        m_recSession = new Ort::Session(m_env, L"paddle_rec.onnx", sessionOpts);
    }

    std::string ExtractText(const cv::Mat& screenshot) {
        // 1. Detect text regions (30ms)
        auto regions = DetectTextRegions(screenshot);

        // 2. Recognize text in each region (20ms)
        std::vector<std::string> texts;
        for (const auto& region : regions) {
            texts.push_back(RecognizeText(region));
        }

        return JoinText(texts);
    }
};
```

**Models:**
- Detection: `paddle_det.onnx` (10MB)
- Recognition: `paddle_rec.onnx` (8MB)
- Download: https://github.com/PaddlePaddle/PaddleOCR/tree/release/2.7/deploy/cpp_infer

---

#### **3. Camera (DirectShow)**

```cpp
#include <dshow.h>

class CameraMonitor {
private:
    IGraphBuilder* m_graphBuilder;
    ICaptureGraphBuilder2* m_captureGraph;
    IBaseFilter* m_cameraFilter;

public:
    cv::Mat CaptureFrame() {
        // Grab latest frame from camera (15ms)
        ISampleGrabber* grabber;
        long bufferSize;
        grabber->GetCurrentBuffer(&bufferSize, nullptr);

        std::vector<BYTE> buffer(bufferSize);
        grabber->GetCurrentBuffer(&bufferSize, buffer.data());

        // Convert to OpenCV Mat
        return cv::Mat(height, width, CV_8UC3, buffer.data());
    }
};
```

---

#### **4. Voice Recognition (Azure Speech SDK)**

```cpp
#include <speechapi_cxx.h>

using namespace Microsoft::CognitiveServices::Speech;

class VoiceMonitor {
private:
    std::shared_ptr<SpeechRecognizer> m_recognizer;

public:
    void StartContinuousRecognition() {
        auto config = SpeechConfig::FromSubscription(
            "YOUR_KEY", "YOUR_REGION"
        );

        m_recognizer = SpeechRecognizer::FromConfig(config);

        // Stream recognition with minimal latency
        m_recognizer->Recognizing.Connect([](const SpeechRecognitionEventArgs& e) {
            // Partial results (low latency)
            std::cout << "Partial: " << e.Result->Text << std::endl;
        });

        m_recognizer->Recognized.Connect([](const SpeechRecognitionEventArgs& e) {
            // Final result
            OnVoiceDetected(e.Result->Text);
        });

        m_recognizer->StartContinuousRecognitionAsync().get();
    }
};
```

**Latency:** 100ms streaming recognition (real-time)

---

#### **Project Structure (C++)**

```
NovaWindows/
├── CMakeLists.txt                  # Build configuration
├── vcpkg.json                      # Dependencies manifest
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── perception/
│   │   ├── screen_monitor.cpp     # Win32 event hooks
│   │   ├── screen_monitor.h
│   │   ├── ocr_engine.cpp         # PaddleOCR ONNX
│   │   ├── ocr_engine.h
│   │   ├── camera_monitor.cpp     # DirectShow capture
│   │   ├── camera_monitor.h
│   │   ├── voice_monitor.cpp      # Azure Speech SDK
│   │   ├── voice_monitor.h
│   │   └── context_collector.cpp  # Aggregates all sensors
│   ├── fusion/
│   │   ├── local_fusion.cpp       # Device-level fusion
│   │   ├── semantic_extractor.cpp # Extract intent from raw data
│   │   └── confidence_scorer.cpp  # Fusion quality metrics
│   ├── sync/
│   │   ├── cloud_sync.cpp         # WebSocket to cloud hub
│   │   ├── p2p_client.cpp         # LAN peer discovery
│   │   └── hybrid_sync.cpp        # Smart routing logic
│   ├── ui/
│   │   ├── main_window.cpp        # Qt main window
│   │   ├── recommendations_view.cpp
│   │   └── context_view.cpp
│   └── models/
│       ├── device_context.h       # Data structures
│       └── global_context.h
├── models/
│   ├── paddle_det.onnx            # OCR detection model
│   ├── paddle_rec.onnx            # OCR recognition model
│   └── yolov8n.onnx               # Optional object detection
└── third_party/                    # External dependencies (via vcpkg)
```

**Dependencies (vcpkg.json):**
```json
{
  "dependencies": [
    "onnxruntime-gpu",
    "opencv4",
    "boost-beast",
    "websocketpp",
    "rapidjson",
    "spdlog",
    "qt6-base",
    "azure-speech-sdk"
  ]
}
```

---

### Android Mobile Client

**Language & Runtime:**
- **Primary Language:** Kotlin 2.0 + selective C++ (NDK)
- **Build System:** Gradle 8.5 + AGP 8.3 + CMake 3.22
- **Min SDK:** Android 10 (API 29)
- **Target SDK:** Android 14 (API 34)
- **Architecture:** arm64-v8a (64-bit only for performance)

**Why Kotlin + Native Libs:** System APIs (AccessibilityService) are Java-based, but core libraries (ML Kit, CameraX) use native code underneath. Best of both worlds.

---

#### **Perception Components**

| Component | Technology | Model/Library | Latency | Purpose |
|-----------|-----------|---------------|---------|---------|
| **Screen Monitoring** | AccessibilityService | Native Android | 5ms | App change detection |
| **Screen OCR** | Google ML Kit | On-device model | 100ms | Extract screen text |
| **Camera** | CameraX + Camera2 | Native API | 20ms | Camera frame acquisition |
| **Object Detection** | TensorFlow Lite | YOLOv8n (TFLite) | 50ms | Camera feed analysis |
| **Voice Recognition** | SpeechRecognizer | Native Android | 150ms | Speech-to-text |
| **Networking** | Ktor Client | HTTP/2 + WebSocket | 50ms | Cloud sync |
| **Serialization** | Kotlinx Serialization | JSON codec | 3ms | Data serialization |
| **DI** | Hilt | Dependency injection | - | Clean architecture |

**Key Android APIs:**
```kotlin
// AccessibilityService for screen monitoring
class ScreenMonitorService : AccessibilityService() {
    override fun onAccessibilityEvent(event: AccessibilityEvent) {
        when (event.eventType) {
            AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED -> {
                val packageName = event.packageName
                val className = event.className
                // Extract app context
            }
        }
    }
}

// CameraX for camera feed
val cameraProvider = ProcessCameraProvider.getInstance(context)
cameraProvider.bindToLifecycle(
    lifecycleOwner,
    CameraSelector.DEFAULT_BACK_CAMERA,
    imageAnalysis
)

// SpeechRecognizer for voice
val recognizer = SpeechRecognizer.createSpeechRecognizer(context)
recognizer.setRecognitionListener(object : RecognitionListener {
    override fun onResults(results: Bundle) {
        val matches = results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
        // Process speech
    }
})
```

**Project Structure:**
```
android/
├── app/src/main/
│   ├── AndroidManifest.xml        # Permissions, services
│   ├── kotlin/com/nova/perception/
│   │   ├── MainActivity.kt        # App entry point
│   │   ├── services/
│   │   │   ├── ScreenMonitorService.kt      # AccessibilityService
│   │   │   ├── CameraMonitor.kt             # CameraX integration
│   │   │   ├── VoiceMonitor.kt              # SpeechRecognizer
│   │   │   └── ContextCollectorService.kt   # Background service
│   │   ├── fusion/
│   │   │   ├── LocalFusionEngine.kt         # Device fusion
│   │   │   ├── SemanticExtractor.kt         # Intent extraction
│   │   │   └── ConfidenceScorer.kt          # Quality metrics
│   │   ├── sync/
│   │   │   ├── CloudSyncClient.kt           # WebSocket client
│   │   │   ├── P2PClient.kt                 # LAN discovery
│   │   │   └── HybridSyncEngine.kt          # Smart routing
│   │   ├── ml/
│   │   │   ├── YoloDetector.kt              # TFLite YOLOv8
│   │   │   └── ModelLoader.kt               # Model management
│   │   ├── ui/
│   │   │   ├── HomeScreen.kt                # Main UI
│   │   │   ├── RecommendationsCard.kt       # Show AI recs
│   │   │   └── ContextCard.kt               # Display context
│   │   └── data/
│   │       ├── DeviceContext.kt             # Local context model
│   │       └── GlobalContext.kt             # Cross-device model
│   └── res/
│       ├── xml/accessibility_service_config.xml
│       └── values/strings.xml
└── build.gradle.kts
```

**Required Permissions:**
```xml
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
```

---

## Sync Infrastructure

### Cloud Sync Hub

**Deployment Platform:** AWS (Primary), GCP (Backup)

**Technology Stack:**

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **Server Framework** | Ktor 2.3 (Kotlin) | WebSocket server |
| **Runtime** | JVM (Corretto 21) | Server runtime |
| **WebSocket** | Netty Engine | High-performance I/O |
| **State Storage** | Redis 7.2 | In-memory device state |
| **Message Queue** | Amazon SQS | Async message processing |
| **Database** | PostgreSQL 16 | Context history, analytics |
| **CDN** | CloudFront | Static asset delivery |
| **Load Balancer** | ALB | Distribute connections |
| **Monitoring** | CloudWatch + Grafana | Metrics, alerts |

**Server Code (Ktor):**
```kotlin
fun Application.configureSyncHub() {
    install(WebSockets) {
        pingPeriod = Duration.ofSeconds(30)
        timeout = Duration.ofSeconds(60)
        maxFrameSize = Long.MAX_VALUE
        masking = false
    }

    routing {
        webSocket("/sync/{deviceId}") {
            val deviceId = call.parameters["deviceId"]!!

            // Add to connection pool
            connectionPool.add(deviceId, this)

            try {
                // Send recent history
                sendContextHistory(deviceId)

                // Listen for updates
                for (frame in incoming) {
                    val message = frame.readText()
                    val context = json.decodeFromString<SyncMessage>(message)

                    // Store in Redis
                    redis.set("context:$deviceId", context, ex = 3600)

                    // Broadcast to other devices in same user account
                    broadcastToUserDevices(context, exclude = deviceId)
                }
            } finally {
                connectionPool.remove(deviceId)
            }
        }
    }
}
```

**Infrastructure (Terraform):**
```hcl
# AWS ECS Fargate for Ktor server
resource "aws_ecs_service" "sync_hub" {
  name            = "nova-sync-hub"
  cluster         = aws_ecs_cluster.main.id
  task_definition = aws_ecs_task_definition.sync_hub.arn
  desired_count   = 3
  launch_type     = "FARGATE"

  network_configuration {
    subnets         = aws_subnet.private.*.id
    security_groups = [aws_security_group.sync_hub.id]
  }

  load_balancer {
    target_group_arn = aws_lb_target_group.sync_hub.arn
    container_name   = "sync-hub"
    container_port   = 8080
  }
}

# ElastiCache Redis for state
resource "aws_elasticache_cluster" "redis" {
  cluster_id           = "nova-state"
  engine               = "redis"
  node_type            = "cache.t3.micro"
  num_cache_nodes      = 1
  parameter_group_name = "default.redis7"
  port                 = 6379
}

# RDS PostgreSQL for history
resource "aws_db_instance" "postgres" {
  identifier        = "nova-history"
  engine            = "postgres"
  engine_version    = "16.1"
  instance_class    = "db.t3.small"
  allocated_storage = 20
  username          = var.db_username
  password          = var.db_password
}
```

---

## Fusion Engine

### Local Fusion (On-Device)

**Architecture:**
```
Context Collection → Feature Extraction → Weighted Fusion → Confidence Scoring
      (10ms)              (5ms)                (5ms)             (2ms)
```

**Components:**

1. **Semantic Extractor**
   - Maps raw data to semantic features
   - Intent classification (coding, browsing, meeting, etc.)
   - Entity recognition (apps, documents, people)

2. **Temporal Context Manager**
   - Rolling window of last 10 contexts
   - Pattern detection (multitasking, deep work, workflow)
   - Focus duration tracking

3. **Confidence Scorer**
   - Data freshness (< 30s = high confidence)
   - Modality completeness (all 3 sources = 100%)
   - Conflict detection (contradictory signals = lower confidence)

**Fusion Weights:**
```kotlin
val fusionWeights = mapOf(
    "screen" to 0.40,    // Primary intent signal
    "voice" to 0.35,     // Explicit user intent
    "camera" to 0.15,    // Environmental context
    "temporal" to 0.10   // Past behavior
)
```

---

### Global Fusion (Cross-Device)

**Architecture:**
```
Device Contexts → Workflow Detection → Intent Resolution → Scope Determination
      (20ms)            (10ms)              (5ms)               (5ms)
```

**Workflow Patterns:**
```kotlin
val crossDeviceWorkflows = mapOf(
    ("coding", "browsing") to "development_research",
    ("meeting", "note_taking") to "hybrid_meeting",
    ("coding", "communication") to "async_collaboration",
    ("video_watching", "browsing") to "leisure_multitasking"
)
```

**Intent Resolution Strategy:**
```kotlin
val intentPriority = mapOf(
    "meeting" to 10,      // Highest priority
    "coding" to 8,
    "email" to 6,
    "communication" to 5,
    "browsing" to 4,
    "entertainment" to 2
)
```

---

## AI/ML Stack

### LLM Integration

**Primary Provider:** OpenAI
**Model:** GPT-4o-mini
**API Version:** v1

**Configuration:**
```kotlin
val openAiClient = OpenAI {
    apiKey = System.getenv("OPENAI_API_KEY")
    timeout = 5000  // 5 second timeout
    retry = 2       // Max 2 retries
}

// Optimized for latency
val recommendations = openAiClient.chat.completions.create(
    model = "gpt-4o-mini",
    messages = listOf(
        ChatMessage(role = "system", content = fusionPrompt)
    ),
    maxTokens = 100,      // Short responses only
    temperature = 0.3,    // Consistent recommendations
    stream = true         // Stream for faster TTFT
)
```

**Prompt Engineering:**
```kotlin
fun buildFusionPrompt(globalContext: GlobalContext): String {
    return """
You are Nova, a context-aware AI assistant.

# User Context:
Primary Device: ${globalContext.primaryDevice}
Primary Intent: ${globalContext.primaryIntent}
Workflow: ${globalContext.workflowType}

## Devices:
${globalContext.devices.joinToString("\n") { device ->
    "- ${device.type}: ${device.screen.app} (${device.screen.contentType})"
}}

# Task:
Generate 3 actionable recommendations for this ${globalContext.workflowType} workflow.
Focus on: ${globalContext.recommendationsScope}

Requirements:
1. Prioritize ${globalContext.primaryDevice} actions
2. Consider cross-device workflows
3. Max 15 words per recommendation

Format: Numbered list only.
""".trimIndent()
}
```

---

### Computer Vision

**Primary: OCR for Screen Content Understanding**

| Component | Windows | Android | Purpose |
|-----------|---------|---------|---------|
| **OCR** | PaddleOCR (ONNX) | Google ML Kit | Extract text from screen |
| **Object Detection** | YOLOv8n (ONNX) | YOLOv8n (TFLite) | Camera feed analysis (optional) |
| **Runtime** | ONNX Runtime + DirectML | TFLite + NNAPI | GPU/NPU acceleration |

**Why OCR over Object Detection:**
- Screen text provides **semantic context** (what user is reading/writing)
- Object detection only gives generic labels ("person", "laptop")
- OCR enables **content-aware recommendations**
- Example: Reading email → "Draft a reply to [sender]"

---

#### **Windows OCR Stack (PaddleOCR)**

**Model:** PaddleOCR v4 (ONNX format)
**Performance:** 50ms per screen capture (GPU)

```csharp
using Microsoft.ML.OnnxRuntime;

public class ScreenOCREngine {
    private InferenceSession detModel;   // Text detection
    private InferenceSession recModel;   // Text recognition

    public ScreenOCREngine() {
        var sessionOptions = new SessionOptions();
        sessionOptions.AppendExecutionProvider_DML(0);  // DirectML GPU

        detModel = new InferenceSession("paddle_det.onnx", sessionOptions);
        recModel = new InferenceSession("paddle_rec.onnx", sessionOptions);
    }

    public async Task<string> CaptureScreenText() {
        // 1. Capture active window (10ms)
        var screenshot = CaptureActiveWindow();

        // 2. Detect text regions (30ms)
        var regions = await DetectTextRegions(screenshot);

        // 3. Recognize text in each region (20ms)
        var texts = await RecognizeTextInRegions(regions);

        return string.Join("\n", texts);
    }
}
```

**Model Files:**
- `paddle_det.onnx` (10MB) - Text detection
- `paddle_rec.onnx` (8MB) - Text recognition
- Download: https://github.com/PaddlePaddle/PaddleOCR

---

#### **Android OCR Stack (Google ML Kit)**

**Model:** Google ML Kit Text Recognition
**Performance:** 100ms per capture (on-device)

```kotlin
import com.google.mlkit.vision.text.TextRecognition
import com.google.mlkit.vision.text.latin.TextRecognizerOptions

class ScreenOCREngine {
    private val recognizer = TextRecognition.getClient(
        TextRecognizerOptions.DEFAULT_OPTIONS
    )

    suspend fun captureScreenText(): String = suspendCoroutine { continuation ->
        // Requires MediaProjection permission
        val screenshot = mediaProjection.captureScreen()

        val image = InputImage.fromBitmap(screenshot, 0)
        recognizer.process(image)
            .addOnSuccessListener { visionText ->
                val fullText = visionText.textBlocks
                    .joinToString("\n") { it.text }
                continuation.resume(fullText)
            }
    }
}
```

**Permissions Required:**
```xml
<uses-permission android:name="android.permission.SYSTEM_ALERT_WINDOW" />
```

---

#### **Optional: Object Detection (Camera Only)**

**Model:** YOLOv8n (Nano)
**Use Case:** Ambient environment context (camera feed, not screen)

**Model Conversion:**
```python
from ultralytics import YOLO

model = YOLO("yolov8n.pt")

# For Windows (ONNX)
model.export(format="onnx", dynamic=True, simplify=True)

# For Android (TFLite)
model.export(format="tflite", int8=True, nms=True)
```

**Performance:**
- Windows (GPU): ~30ms per frame
- Android (NPU): ~50ms per frame
- Input size: 640x640
- Classes: 80 COCO classes

**Note:** Object detection is **secondary** to OCR. Use sparingly for camera feed analysis only.

---

## Development Tools

### Build & CI/CD

**Windows:**
```yaml
# .github/workflows/windows-build.yml
name: Windows Build

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-dotnet@v4
        with:
          dotnet-version: '8.0.x'
      - run: dotnet restore
      - run: dotnet build --configuration Release
      - run: dotnet test
      - run: dotnet publish -c Release -r win-x64 --self-contained
```

**Android:**
```yaml
# .github/workflows/android-build.yml
name: Android Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-java@v4
        with:
          java-version: '17'
          distribution: 'temurin'
      - run: ./gradlew assembleRelease
      - run: ./gradlew testReleaseUnitTest
```

---

### Code Quality

| Tool | Purpose | Config |
|------|---------|--------|
| **SonarQube** | Code quality, security | `sonar-project.properties` |
| **Detekt** (Kotlin) | Static analysis | `.detekt.yml` |
| **StyleCop** (C#) | Code style | `.editorconfig` |
| **ktlint** (Kotlin) | Formatting | `.editorconfig` |

---

## Deployment

### Windows Distribution

**Package Format:** MSIX (Windows App SDK)
**Distribution:** Microsoft Store + Direct download
**Auto-Update:** MSIX package updates

**Build Command:**
```bash
dotnet publish -c Release -r win-x64 \
    --self-contained \
    -p:PublishSingleFile=true \
    -p:IncludeNativeLibrariesForSelfExtract=true \
    -p:GenerateAppxPackageOnBuild=true
```

---

### Android Distribution

**Package Format:** AAB (Android App Bundle)
**Distribution:** Google Play Store
**Auto-Update:** Play Store managed

**Build Command:**
```bash
./gradlew bundleRelease
```

---

### Cloud Infrastructure

**Deployment Strategy:** Blue-Green Deployment
**Container Registry:** Amazon ECR
**Orchestration:** ECS Fargate

**Deployment Pipeline:**
```yaml
# Deploy to staging
terraform apply -var-file=staging.tfvars

# Health checks
curl -f https://staging.nova-sync.com/health || exit 1

# Deploy to production
terraform apply -var-file=production.tfvars
```

---

## Performance Targets

### Latency Goals

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Context Collection** | < 10ms | Win32 event → data ready |
| **Local Fusion** | < 25ms | Raw data → fused context |
| **Cloud Sync (LAN)** | < 50ms | Device → Cloud → Device |
| **Cloud Sync (WAN)** | < 200ms | Device → Cloud → Device |
| **LLM TTFT** | < 300ms | Request → first token |
| **LLM Full Response** | < 1500ms | Request → all tokens |
| **UI Update** | < 16ms | 60 FPS rendering |
| **End-to-End** | < 400ms | Event → first rec shown |
| **End-to-End (full)** | < 1600ms | Event → all recs shown |

---

### Resource Limits

**Windows Client:**
- CPU: < 2% idle, < 10% active
- RAM: < 150MB base, < 300MB peak
- GPU: < 100MB VRAM (OpenCV + ONNX)
- Network: < 50KB/s average

**Android Client:**
- CPU: < 3% idle, < 15% active
- RAM: < 100MB base, < 200MB peak
- Battery: < 5% drain per hour
- Network: < 30KB/s average

**Cloud Infrastructure:**
- WebSocket connections: 10,000 concurrent/instance
- Message throughput: 100,000 messages/second/instance
- P99 latency: < 200ms
- Uptime: 99.9% SLA

---

## Security & Privacy

### Data Encryption
- **In Transit:** TLS 1.3 (WebSocket Secure)
- **At Rest:** AES-256 (Redis, PostgreSQL)
- **End-to-End:** Optional E2EE for sensitive data

### Authentication
- **User Auth:** OAuth 2.0 + JWT
- **Device Auth:** Device-specific certificates
- **API Keys:** Rotating secrets (AWS Secrets Manager)

### Privacy Controls
- **Data Retention:** 30 days (configurable)
- **Data Deletion:** User-triggered purge
- **Local-only Mode:** No cloud sync (offline mode)
- **Consent:** Granular permissions per sensor

---

## Monitoring & Observability

### Metrics (Prometheus + Grafana)
- Context collection frequency
- Fusion latency (p50, p95, p99)
- LLM API latency
- WebSocket connection count
- Error rates

### Logging (ELK Stack)
- Structured logs (JSON)
- Correlation IDs for distributed tracing
- Log levels: DEBUG, INFO, WARN, ERROR

### Alerts (PagerDuty)
- High error rate (> 1%)
- High latency (p99 > 2s)
- Service down
- Low device sync rate

---

## License & Attribution

**License:** Proprietary
**Third-Party Libraries:** See `THIRD_PARTY_LICENSES.md`

---

**Last Updated:** October 2025
**Maintained By:** Nova Engineering Team
