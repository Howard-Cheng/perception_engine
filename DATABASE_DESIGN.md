# Database Design Documentation

## Overview

The Nova Perception Engine uses a **multi-layered database architecture** designed for efficient capture, compression, and analysis of cross-device context data. The system balances high-frequency writes (real-time perception events) with analytical queries (session analysis, engagement tracking) through a three-tier storage hierarchy.

**Design Philosophy:**
- **Layer 0**: High-speed write buffer for raw events (SQLite/Elasticsearch)
- **Layer 1**: Compressed analytical storage (DuckDB)
- **Layer 2**: Long-term aggregated summaries (DuckDB) - Planned

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    Perception Engine (C++)                       │
│                  Real-time Context Capture                       │
│         (Screen + Audio + Camera + System Metrics)               │
└────────────────────────────┬────────────────────────────────────┘
                             │ HTTP API (Port 8777)
                             │ GET /context (500ms polling)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Data Collector                              │
│               (perception_data_collector)                        │
│         Polls API → Parses JSON → Ingests Events                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                         LAYER 0                                  │
│                   Raw Event Storage                              │
│                                                                  │
│  ┌──────────────────┐              ┌──────────────────┐        │
│  │     SQLite       │      OR      │  Elasticsearch   │        │
│  │  (Embedded DB)   │              │ (Distributed)    │        │
│  │  raw_events.db   │              │ Port 9200        │        │
│  └──────────────────┘              └──────────────────┘        │
│                                                                  │
│  Retention: 24 hours                                            │
│  Volume: 2-5 GB/day/device                                      │
│  Write: ~200 events/hour                                        │
└────────────────────────────┬────────────────────────────────────┘
                             │ Compression Pipeline
                             │ (Processes uncompressed events)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                         LAYER 1                                  │
│                  Compressed Sessions                             │
│                                                                  │
│                    ┌──────────────────┐                         │
│                    │     DuckDB       │                         │
│                    │  (OLAP/Analytics)│                         │
│                    │compressed_sessions│                        │
│                    └──────────────────┘                         │
│                                                                  │
│  Retention: 7 days                                              │
│  Volume: 200-500 MB/day (90% compression)                       │
│  Features: Session detection, engagement scoring, LLM summaries │
└────────────────────────────┬────────────────────────────────────┘
                             │ Aggregation Pipeline (Planned)
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                         LAYER 2                                  │
│                    Aggregated Sessions                           │
│                                                                  │
│                    ┌──────────────────┐                         │
│                    │     DuckDB       │                         │
│                    │ work_sessions/   │                         │
│                    │  day_sessions    │                         │
│                    └──────────────────┘                         │
│                                                                  │
│  Retention: 30 days                                             │
│  Volume: Aggregated data                                        │
│  Features: Project tracking, daily summaries, entity linking    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Layer 0: Raw Event Storage

### Purpose
Layer 0 serves as a **high-frequency write buffer** that captures every perception event in real-time. It prioritizes write performance and complete data retention before compression.

### Storage Options

#### Option A: SQLite (Default)
**When to use:** Single-device deployment, embedded systems, development/testing

**Advantages:**
- Zero configuration (embedded database)
- ACID transactions
- Low memory footprint (~50MB)
- File-based (easy backup/restore)

**Disadvantages:**
- Single-writer limitation
- Limited to ~200 concurrent writes/sec
- Not suitable for multi-device deployments

#### Option B: Elasticsearch
**When to use:** Multi-device deployment, distributed systems, production at scale

**Advantages:**
- Distributed architecture (horizontal scaling)
- Full-text search capabilities
- Built-in replication and failover
- REST API (language-agnostic)

**Disadvantages:**
- Requires separate server deployment
- Higher memory requirements (~2GB minimum)
- More complex setup and maintenance

### Schema: `raw_events` Table

```sql
CREATE TABLE raw_events (
    -- Primary identification
    event_id TEXT PRIMARY KEY,           -- SHA-256 hash of (timestamp + device_id + app_name)
    timestamp TIMESTAMP NOT NULL,        -- Event capture time (ISO 8601)
    device_id TEXT NOT NULL,             -- Unique device identifier

    -- Application context
    app_name TEXT NOT NULL,              -- Active application (e.g., "chrome.exe", "code.exe")
    window_title TEXT,                   -- Window title text
    url TEXT,                            -- Browser URL (if applicable)

    -- Content capture
    screen_content TEXT,                 -- Full screen text/OCR result
    screen_content_hash TEXT,            -- SHA-256 hash for deduplication

    -- Interaction signals
    mouse_events TEXT,                   -- JSON array of mouse events
    interaction_count INTEGER DEFAULT 0, -- Number of clicks/keypresses
    dwell_time_seconds INTEGER DEFAULT 0,-- Time spent in this window

    -- Multimodal inputs
    voice_transcription TEXT,            -- Whisper.cpp transcription
    camera_description TEXT,             -- FastVLM scene description

    -- System information
    battery_percent INTEGER,             -- 0-100
    is_charging BOOLEAN DEFAULT 0,       -- Charging state
    network_type TEXT,                   -- "WiFi", "Ethernet", "Cellular", "Offline"
    location_lat REAL,                   -- GPS latitude (optional)
    location_lon REAL,                   -- GPS longitude (optional)
    cpu_usage REAL,                      -- Percentage (0-100)
    memory_usage REAL,                   -- Percentage (0-100)

    -- Classification (populated during compression)
    content_type TEXT,                   -- "EMAIL", "CHAT", "CODE", "DOCUMENT", etc.
    domain TEXT,                         -- "WORK", "ENTERTAINMENT", "LIFE", etc.

    -- Session linking
    session_id TEXT,                     -- Assigned during compression

    -- Processing status
    compressed BOOLEAN DEFAULT 0,        -- Has this been processed by Layer 1?
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for performance
CREATE INDEX idx_timestamp ON raw_events(timestamp);
CREATE INDEX idx_compressed ON raw_events(compressed);
CREATE INDEX idx_session ON raw_events(session_id);
CREATE INDEX idx_device ON raw_events(device_id);
CREATE INDEX idx_app_name ON raw_events(app_name);
CREATE INDEX idx_content_hash ON raw_events(screen_content_hash);
```

### Data Types

**RawEvent Structure (C++):**
```cpp
class RawEvent {
public:
    EventId eventId;                        // std::string (SHA-256 hash)
    Timestamp timestamp;                    // std::chrono::system_clock::time_point
    DeviceId deviceId;                      // std::string

    // App context
    std::string appName;
    std::optional<std::string> windowTitle;
    std::optional<std::string> url;

    // Content
    std::optional<std::string> screenContent;
    std::optional<std::string> screenContentHash;

    // Interaction signals
    std::vector<MouseEvent> mouseEvents;    // Array of mouse/keyboard events
    int interactionCount;
    int dwellTimeSeconds;

    // Audio/Camera
    std::optional<std::string> voiceTranscription;
    std::optional<std::string> cameraDescription;

    // System info
    SystemInfo systemInfo;

    // Classification (optional until compression)
    std::optional<ContentType> contentType;
    std::optional<Domain> domain;

    // Session linking
    std::optional<SessionId> sessionId;

    // Status
    bool compressed;
    Timestamp createdAt;
};
```

**MouseEvent Structure:**
```cpp
struct MouseEvent {
    Timestamp timestamp;
    std::string eventType;      // "LeftClick", "RightClick", "Copy", "TextSelection", "Scroll"
    int posX;
    int posY;
    std::string content;        // Selected/copied text content
    std::string elementType;    // "Button", "Text", "Link", "Input", etc.
};
```

**SystemInfo Structure:**
```cpp
struct SystemInfo {
    std::optional<int> batteryPercent;
    bool isCharging;
    std::string networkType;
    std::optional<double> locationLat;
    std::optional<double> locationLon;
    std::optional<double> cpuUsage;
    std::optional<double> memoryUsage;
};
```

### Data Ingestion Flow

1. **Perception Engine** captures real-time context (screen, audio, camera, system)
2. **HTTP API** serves context at `http://localhost:8777/context` (500ms poll interval)
3. **Data Collector** polls API, parses JSON response
4. **Event Creation** transforms JSON → RawEvent object
5. **Ingestion** stores event in Layer 0 (SQLite or Elasticsearch)

**Code Location:**
- `database_cpp/database_cpp/src/layer0/DataIngestion.cpp` - SQLite ingestion
- `database_cpp/database_cpp/src/layer0/ElasticsearchClient.cpp` - Elasticsearch ingestion
- `database_cpp/database_cpp/src/collector/DataCollector.cpp` - API polling and ingestion loop

### Performance Characteristics

| Metric | SQLite | Elasticsearch |
|--------|--------|---------------|
| Write throughput | ~200 events/sec | ~5,000 events/sec (bulk) |
| Latency (p99) | <10ms | <50ms |
| Storage overhead | ~2KB/event | ~3KB/event (with indexing) |
| Memory usage | ~50MB | ~2GB (minimum) |
| Concurrent writers | 1 | Unlimited (sharded) |

### Data Retention

**Automatic cleanup policy:**
- Events older than 24 hours are **automatically deleted** after Layer 1 compression completes
- Compressed events (where `compressed = 1`) are retained for an additional 6 hours for debugging
- Manual cleanup: `DELETE FROM raw_events WHERE timestamp < datetime('now', '-24 hours')`

**Implementation:** Scheduled cleanup job runs every hour (`database_cpp/database_cpp/src/layer0/DataIngestion.cpp:cleanupOldEvents()`)

---

## Layer 1: Compressed Sessions

### Purpose
Layer 1 performs **intelligent compression** of raw events into meaningful sessions with engagement metrics, content summaries, and high-attention extracts. This reduces storage by ~90% while preserving critical information for retrieval and analysis.

### Storage Technology: DuckDB

**Why DuckDB?**
- Optimized for **analytical queries** (OLAP workload)
- Columnar storage (efficient compression)
- Embedded like SQLite but with PostgreSQL-compatible SQL
- Excellent performance for aggregations and time-series queries
- Native support for complex types (arrays, structs, JSON)

### Schema: `compressed_sessions` Table

```sql
CREATE TABLE compressed_sessions (
    -- Identification
    session_id VARCHAR PRIMARY KEY,      -- UUID v4
    device_id VARCHAR NOT NULL,

    -- Time boundaries
    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP NOT NULL,
    duration_seconds INTEGER,            -- Computed: end_time - start_time

    -- Classification
    content_type VARCHAR,                -- "EMAIL", "CHAT", "CODE", "DOCUMENT", "WEB_ARTICLE", etc.
    domain VARCHAR,                      -- "WORK", "ENTERTAINMENT", "LIFE", "INTERACTION"

    -- Application context
    app_name VARCHAR NOT NULL,
    window_title VARCHAR,
    url VARCHAR,

    -- Engagement metrics
    engagement_score DOUBLE,             -- 0.0 to 1.0 (weighted calculation)
    interaction_count INTEGER,           -- Total clicks/keypresses in session
    total_dwell_time INTEGER,            -- Total time (seconds)
    has_copied BOOLEAN,                  -- User copied text?
    has_selected BOOLEAN,                -- User selected text?

    -- Compressed content (LLM-generated)
    compressed_summary VARCHAR,          -- LLM summary (target: 100-200 tokens)
    key_points VARCHAR,                  -- JSON array of key points
    summary_token_count INTEGER,         -- Token count for compression ratio tracking

    -- High-attention content (preserved for retrieval)
    copied_content VARCHAR,              -- JSON array of copied text
    selected_text VARCHAR,               -- JSON array of selected text
    numbers VARCHAR,                     -- JSON array of important numbers
    dates VARCHAR,                       -- JSON array of dates
    urls VARCHAR,                        -- JSON array of URLs
    emails VARCHAR,                      -- JSON array of email addresses

    -- Metadata
    metadata_json VARCHAR,               -- Content-type-specific metadata (JSON)

    -- Averaged system metrics
    avg_cpu_usage DOUBLE,
    avg_memory_usage DOUBLE,
    avg_battery_percent INTEGER,

    -- Timestamps
    created_at TIMESTAMP,                -- Session start time
    compressed_at TIMESTAMP              -- When compression occurred
);

-- Indexes for analytical queries
CREATE INDEX idx_device_time ON compressed_sessions(device_id, start_time);
CREATE INDEX idx_content_type ON compressed_sessions(content_type);
CREATE INDEX idx_engagement ON compressed_sessions(engagement_score);
CREATE INDEX idx_domain ON compressed_sessions(domain);
```

### Compression Pipeline

**Pipeline Stages:**

```
┌─────────────────────────────────────────────────────────────────┐
│              1. Session Detection                                │
│  Input: Uncompressed raw events (compressed = 0)                │
│  Algorithm: Idle-time threshold + context switch detection      │
│  Output: Groups of events belonging to same session              │
└─────────────────────────────┬───────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              2. Engagement Calculation                           │
│  Metrics:                                                        │
│   - Interaction count (clicks, keypresses)                      │
│   - Dwell time                                                  │
│   - Copy/selection events (high attention)                      │
│  Formula: engagement_score = weighted_sum([metrics...])         │
└─────────────────────────────┬───────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              3. Content Classification                           │
│  Classifiers:                                                    │
│   - ContentType: EMAIL, CHAT, CODE, DOCUMENT, etc.              │
│   - Domain: WORK, ENTERTAINMENT, LIFE, INTERACTION              │
│  Method: Heuristic rules + pattern matching                     │
└─────────────────────────────┬───────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              4. Content Extraction                               │
│  Extractors:                                                     │
│   - High-attention content (copied/selected text)               │
│   - Entities (numbers, dates, URLs, emails)                     │
│   - Metadata (sender, subject, file path, etc.)                 │
└─────────────────────────────┬───────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              5. LLM Compression (TODO)                           │
│  Input: Full screen content from session events                 │
│  Process: LLM summarization (target: 10% of original)           │
│  Output: compressed_summary + key_points                        │
└─────────────────────────────┬───────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              6. DuckDB Storage                                   │
│  Action: INSERT compressed_session row                          │
│  Update: Mark raw events as compressed (compressed = 1)         │
└─────────────────────────────────────────────────────────────────┘
```

### Session Detection Algorithm

**Triggers for session boundary:**

1. **Idle threshold exceeded:** Gap between events > 5 minutes (configurable)
2. **Domain change:** `domain` changes (WORK → ENTERTAINMENT)
3. **App type change:** Major app category switch (Browser → IDE)
4. **Content type change:** `content_type` changes (EMAIL → CODE)

**Implementation:** `database_cpp/database_cpp/src/layer1/SessionDetector.cpp`

**Example:**
```
Event 1: chrome.exe, GitHub, 10:00:00 → Session A starts
Event 2: chrome.exe, GitHub, 10:05:00 → Session A continues
Event 3: chrome.exe, Gmail, 10:10:00  → Content change: Session B starts
Event 4: code.exe, main.cpp, 10:20:00 → App change: Session C starts
```

### Engagement Score Calculation

**Formula:**
```cpp
engagement_score = (
    normalized_interaction_count * 0.3 +
    normalized_dwell_time * 0.2 +
    has_copied * 0.25 +
    has_selected * 0.15 +
    normalized_selection_count * 0.1
)
```

**Normalization:**
- `normalized_interaction_count = min(1.0, interaction_count / 50)`
- `normalized_dwell_time = min(1.0, dwell_time_seconds / 300)`
- `normalized_selection_count = min(1.0, selection_count / 10)`

**Implementation:** `database_cpp/database_cpp/src/layer1/EngagementCalculator.cpp`

### Content Classification

**ContentType Enum:**
- `EMAIL` - Email clients (Outlook, Gmail, Thunderbird)
- `CHAT` - Messaging apps (Slack, Teams, Discord, WhatsApp)
- `WEB_ARTICLE` - Long-form content (Medium, blogs, news articles)
- `WEB_PAGE` - General web browsing
- `CODE` - IDEs and code editors (VS Code, Visual Studio, IntelliJ)
- `DOCUMENT` - Document editors (Word, Google Docs, Notion)
- `MEETING` - Video conferencing (Zoom, Teams, Meet)
- `VIDEO` - Video streaming (YouTube, Netflix)
- `SOCIAL` - Social media (Twitter, Facebook, LinkedIn)
- `RESEARCH_PAPER` - Academic papers (arXiv, Google Scholar)
- `UNKNOWN` - Unclassified

**Domain Enum:**
- `SYSTEM` - System utilities, settings
- `WORK` - Productivity, development, email, meetings
- `ENTERTAINMENT` - Videos, music, gaming
- `LIFE` - Shopping, banking, health, travel
- `INTERACTION` - Chat, social media, communication

**Classification Logic:**
```cpp
// Example: Email classification
if (app_name == "outlook.exe" || url.contains("mail.google.com")) {
    content_type = ContentType::EMAIL;
    domain = Domain::WORK;
}

// Example: Code classification
if (app_name == "code.exe" || window_title.contains(".cpp") || window_title.contains(".py")) {
    content_type = ContentType::CODE;
    domain = Domain::WORK;
}
```

**Implementation:** `database_cpp/database_cpp/src/layer1/ContentClassifier.cpp`

### Compression Ratio

**Target:** 90% reduction from Layer 0 to Layer 1

**Calculation:**
```
compression_ratio = 1 - (compressed_size / original_size)

Where:
- original_size = sum(length(screen_content) for all events in session)
- compressed_size = length(compressed_summary) + length(high_attention_content)
```

**Example:**
- Session: 20 events, average screen_content = 5,000 characters
- Original size: 20 * 5,000 = 100,000 characters
- Compressed summary: 500 characters (LLM summary)
- High-attention extracts: 2,000 characters (copied/selected text)
- Compressed size: 2,500 characters
- Compression ratio: 1 - (2,500 / 100,000) = **97.5%**

### Data Retention

**Policy:** 7-day retention for compressed sessions

**Cleanup:** Automatic deletion of sessions older than 7 days
- Runs daily at 2:00 AM (configurable)
- Query: `DELETE FROM compressed_sessions WHERE start_time < current_timestamp - INTERVAL '7 days'`

**Implementation:** `database_cpp/database_cpp/src/layer1/DuckDBManager.cpp:deleteSessionsOlderThan()`

---

## Layer 2: Aggregated Sessions (Planned)

### Purpose
Layer 2 aggregates Layer 1 sessions into higher-level abstractions for long-term analysis and cross-session intelligence.

**Status:** ⚠️ Not yet implemented (planned for future release)

### Planned Features

#### Work Sessions
Group related interaction sessions into project-level work sessions.

**Example:**
- Multiple CODE sessions + DOCUMENT sessions + MEETING sessions → "Project X Development" work session

**Schema (planned):**
```sql
CREATE TABLE work_sessions (
    work_session_id VARCHAR PRIMARY KEY,
    device_id VARCHAR NOT NULL,
    project_name VARCHAR,              -- Inferred project name
    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP NOT NULL,
    interaction_sessions VARCHAR[],     -- Array of session IDs
    total_engagement DOUBLE,
    primary_domain VARCHAR,
    key_entities VARCHAR,              -- JSON: extracted entities across sessions
    work_summary VARCHAR,              -- LLM-generated work session summary
    created_at TIMESTAMP
);
```

#### Day Sessions
Daily rollups of all user activity.

**Schema (planned):**
```sql
CREATE TABLE day_sessions (
    day_session_id VARCHAR PRIMARY KEY,
    device_id VARCHAR NOT NULL,
    date DATE NOT NULL,
    work_duration_seconds INTEGER,
    entertainment_duration_seconds INTEGER,
    total_sessions INTEGER,
    top_apps VARCHAR[],                -- Most used apps
    daily_summary VARCHAR,             -- LLM-generated daily summary
    created_at TIMESTAMP
);
```

#### Entity Tracking
Track entities (people, projects, URLs, documents) across sessions.

**Schema (planned):**
```sql
CREATE TABLE entity_occurrences (
    entity_id VARCHAR PRIMARY KEY,
    entity_type VARCHAR,               -- "person", "project", "url", "document"
    entity_value VARCHAR,              -- Actual value (e.g., "Alice", "github.com/foo/bar")
    first_seen TIMESTAMP,
    last_seen TIMESTAMP,
    occurrence_count INTEGER,
    session_ids VARCHAR[]              -- Sessions where entity appeared
);
```

---

## Integration with Perception Engine

### Data Flow: Perception Engine → Database

```
┌──────────────────────────────────────────────────────────────┐
│  Perception Engine (C++ Service - Port 8777)                 │
│                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Screen       │  │ Audio        │  │ Camera       │      │
│  │ Monitor      │  │ Pipeline     │  │ (Python)     │      │
│  │ (Win32 API)  │  │ (Whisper)    │  │ (FastVLM)    │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │               │
│         └─────────────────┼─────────────────┘               │
│                           ▼                                 │
│         ┌────────────────────────────────┐                  │
│         │   ContextCollector             │                  │
│         │   • Aggregates all sources     │                  │
│         │   • Thread-safe updates        │                  │
│         │   • Returns JSON context       │                  │
│         └────────────────────────────────┘                  │
└──────────────────────────┬───────────────────────────────────┘
                           │ HTTP API
                           │ GET /context (every 500ms)
                           ▼
┌──────────────────────────────────────────────────────────────┐
│  Data Collector (perception_data_collector)                  │
│                                                               │
│  1. Poll API: GET http://localhost:8777/context              │
│  2. Parse JSON response                                      │
│  3. Create RawEvent object                                   │
│  4. Ingest to Layer 0 (SQLite or Elasticsearch)              │
│                                                               │
│  Configuration:                                              │
│   - Poll interval: 5 seconds (configurable)                  │
│   - Retry logic: 3 attempts with exponential backoff         │
│   - Storage backend: SQLITE or ELASTICSEARCH                 │
└──────────────────────────────────────────────────────────────┘
```

### API Response Format

**Endpoint:** `GET http://localhost:8777/context`

**Response JSON:**
```json
{
  "activeApp": "chrome.exe",
  "windowTitle": "GitHub - perception_engine",
  "url": "https://github.com/Howard-Cheng/perception_engine",
  "cpuUsage": 25.3,
  "memoryUsage": 65.2,
  "battery": 85,
  "voiceTranscription": "hello world",
  "voiceLatency": 180.5,
  "cameraDescription": "A person sitting at desk working on laptop",
  "cameraLatency": 9200,
  "contextUpdateLatency": 28.3,
  "RecentPeriodActiveApps": [
    {
      "appName": "chrome.exe",
      "lastSwitchTime": "2025-10-10T10:30:45.123+08:00",
      "duration": 120
    }
  ],
  "fusedContext": "Active: chrome.exe | Said: \"hello world\" | Scene: person at desk",
  "timestamp": "2025-10-10T10:30:45.123+08:00"
}
```

### Mapping: API JSON → RawEvent

```cpp
// Pseudo-code for data collector transformation
RawEvent event;
event.timestamp = parseISO8601(json["timestamp"]);
event.deviceId = config.deviceId;
event.appName = json["activeApp"];
event.windowTitle = json["windowTitle"];
event.url = json["url"];
event.voiceTranscription = json["voiceTranscription"];
event.cameraDescription = json["cameraDescription"];

// System info
event.systemInfo.cpuUsage = json["cpuUsage"];
event.systemInfo.memoryUsage = json["memoryUsage"];
event.systemInfo.batteryPercent = json["battery"];

// Interaction signals (derived from RecentPeriodActiveApps)
event.dwellTimeSeconds = json["RecentPeriodActiveApps"][0]["duration"];
event.interactionCount = 0; // Not directly available from API

// Screen content (not available from /context endpoint)
// Would require separate API endpoint or browser content extraction
event.screenContent = std::nullopt;
```

**Note:** Current `/context` API does not provide full screen content or mouse events. For complete database functionality, consider:
1. Adding `/context/full` endpoint with screen content
2. Implementing browser content extraction (see `windows_code/BrowserContentExtractor.cpp`)
3. Logging mouse/keyboard events at PerceptionEngine level

---

## Code Organization

### Directory Structure

```
perception_engine/
├── database_cpp/
│   └── database_cpp/
│       ├── include/                     # Header files
│       │   ├── common/                  # Common utilities
│       │   │   ├── Types.h              # Core type definitions
│       │   │   ├── DatabaseConfig.h     # Configuration structures
│       │   │   ├── Logger.h             # Logging system
│       │   │   └── Utils.h              # Helper functions
│       │   │
│       │   ├── layer0/                  # Layer 0 - Raw Events
│       │   │   ├── DataIngestion.h      # SQLite event ingestion
│       │   │   ├── SchemaManager.h      # Database schema management
│       │   │   └── ElasticsearchClient.h # Elasticsearch ingestion
│       │   │
│       │   ├── layer1/                  # Layer 1 - Compression
│       │   │   ├── DuckDBManager.h      # DuckDB interface
│       │   │   ├── SessionDetector.h    # Session boundary detection
│       │   │   ├── EngagementCalculator.h # Engagement metrics
│       │   │   ├── ContentExtractor.h   # High-attention content extraction
│       │   │   ├── ContentClassifier.h  # Content type classification
│       │   │   └── CompressionPipeline.h # Main compression orchestrator
│       │   │
│       │   └── collector/               # Data Collector
│       │       └── DataCollector.h      # API polling and ingestion
│       │
│       ├── src/                         # Implementation files
│       │   ├── common/
│       │   │   ├── Types.cpp
│       │   │   ├── DatabaseConfig.cpp
│       │   │   ├── Logger.cpp
│       │   │   └── Utils.cpp
│       │   │
│       │   ├── layer0/
│       │   │   ├── DataIngestion.cpp
│       │   │   ├── SchemaManager.cpp
│       │   │   └── ElasticsearchClient.cpp
│       │   │
│       │   ├── layer1/
│       │   │   ├── DuckDBManager.cpp
│       │   │   ├── SessionDetector.cpp
│       │   │   ├── EngagementCalculator.cpp
│       │   │   ├── ContentExtractor.cpp
│       │   │   ├── ContentClassifier.cpp
│       │   │   └── CompressionPipeline.cpp
│       │   │
│       │   ├── collector/
│       │   │   ├── main.cpp             # Data collector executable
│       │   │   └── DataCollector.cpp
│       │   │
│       │   └── main.cpp                 # Database service main
│       │
│       ├── examples/                    # Usage examples
│       │   ├── basic_ingestion.cpp      # SQLite ingestion example
│       │   ├── elasticsearch_ingestion.cpp # Elasticsearch example
│       │   └── layer1_integration_test.cpp # Compression pipeline test
│       │
│       ├── elasticsearch_client_dll/    # Elasticsearch C++ client library
│       │   ├── include/
│       │   │   ├── ElasticsearchClient.h
│       │   │   ├── ElasticsearchClientAPI.h
│       │   │   └── ElasticsearchTypes.h
│       │   ├── src/
│       │   │   ├── ElasticsearchClient.cpp
│       │   │   └── ElasticsearchClientAPI.cpp
│       │   ├── docker/                  # Elasticsearch deployment scripts
│       │   │   ├── docker-compose.yml
│       │   │   ├── Deploy-Elasticsearch.ps1
│       │   │   └── elasticsearch-manager.bat
│       │   └── docs/                    # Elasticsearch documentation
│       │
│       └── CMakeLists.txt               # Build configuration
│
└── windows_code/                        # Perception Engine (Windows)
    ├── PerceptionEngine.cpp             # Main entry point
    ├── ContextCollector.cpp             # Context aggregation
    ├── HttpServer.cpp                   # HTTP API server
    ├── AudioCaptureEngine.cpp           # Audio pipeline
    ├── WindowsAPIs.cpp                  # System monitoring
    ├── win_camera_fastvlm_pytorch.py    # Camera vision (Python)
    └── elasticsearch_client/            # Elasticsearch integration
        ├── src/
        │   ├── ElasticsearchClient.cpp
        │   └── ElasticsearchClientAPI.cpp
        └── include/
            └── ElasticsearchClient.h
```

---

## Build and Deployment

### Building the Database Components

#### Prerequisites
- CMake 3.15+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- SQLite3 development libraries
- DuckDB development libraries
- libcurl (for HTTP requests)
- nlohmann-json (header-only, included)

#### Build Commands

```bash
# Navigate to database directory
cd database_cpp/database_cpp

# Configure CMake
mkdir build && cd build
cmake ..

# Build all targets
cmake --build . --config Release

# Build specific components
cmake --build . --target perception_data_collector  # Data collector executable
cmake --build . --target basic_ingestion            # Example program
```

#### Build Outputs
```
build/
├── perception_database_service    # Main database service
├── perception_data_collector      # Data collector executable
├── basic_ingestion                # Example: SQLite ingestion
├── elasticsearch_ingestion        # Example: Elasticsearch ingestion
└── layer1_integration_test        # Example: Compression pipeline
```

### Running the Data Collector

#### SQLite Backend (Default)

```bash
# Run with default configuration
./perception_data_collector

# Custom configuration
./perception_data_collector \
  --api-url http://localhost:8777/context \
  --db-path ./my_data/events.db \
  --device-id my_laptop_001 \
  --poll-interval 5
```

#### Elasticsearch Backend

```bash
# First, start Elasticsearch (see Elasticsearch Setup section)
docker compose up -d elasticsearch

# Run data collector with Elasticsearch backend
./perception_data_collector \
  --storage elasticsearch \
  --es-url http://localhost:9200 \
  --es-index perception_raw_events \
  --api-url http://localhost:8777/context
```

### Configuration File

**Location:** `perception_data/collector_config.json`

```json
{
  "apiUrl": "http://localhost:8777/context",
  "dbPath": "./perception_data/raw_events.db",
  "deviceId": "pc_001",
  "pollIntervalSeconds": 5,
  "connectionTimeoutSeconds": 10,
  "maxRetries": 3,
  "storageBackend": "SQLITE",
  "elasticsearchUrl": "http://localhost:9200",
  "elasticsearchIndex": "perception_raw_events"
}
```

---

## Elasticsearch Setup

### Quick Start (Development)

**Using Docker Compose:**

```bash
cd database_cpp/database_cpp/elasticsearch_client_dll/docker

# Start Elasticsearch (single-node, no security)
docker compose up -d

# Verify it's running
curl http://localhost:9200

# Stop
docker compose down
```

### Production Deployment

**Using PowerShell script (Windows):**

```powershell
cd database_cpp\database_cpp\elasticsearch_client_dll\docker

# Deploy with security enabled
.\Deploy-Elasticsearch.ps1 -Action start -EnableSecurity $true

# Check status
.\Deploy-Elasticsearch.ps1 -Action status

# Stop
.\Deploy-Elasticsearch.ps1 -Action stop
```

### Docker Compose Configuration

**File:** `elasticsearch_client_dll/docker/docker-compose.yml`

```yaml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    container_name: elasticsearch
    environment:
      - discovery.type=single-node
      - xpack.security.enabled=false
      - "ES_JAVA_OPTS=-Xms2g -Xmx2g"
    ports:
      - "9200:9200"
      - "9300:9300"
    volumes:
      - esdata:/usr/share/elasticsearch/data
    restart: unless-stopped

volumes:
  esdata:
```

### Index Mapping

**Index name:** `perception_raw_events`

**Mapping:**
```json
{
  "mappings": {
    "properties": {
      "event_id": { "type": "keyword" },
      "timestamp": { "type": "date" },
      "device_id": { "type": "keyword" },
      "app_name": { "type": "keyword" },
      "window_title": { "type": "text" },
      "url": { "type": "text" },
      "screen_content": { "type": "text" },
      "screen_content_hash": { "type": "keyword" },
      "voice_transcription": { "type": "text" },
      "camera_description": { "type": "text" },
      "content_type": { "type": "keyword" },
      "domain": { "type": "keyword" },
      "session_id": { "type": "keyword" },
      "compressed": { "type": "boolean" }
    }
  }
}
```

**Initialization:** Automatic on first use (see `ElasticsearchClient::initializeIndex()`)

---

## API Reference

### DataIngestion Class (Layer 0 - SQLite)

**Location:** `include/layer0/DataIngestion.h`

```cpp
class DataIngestion {
public:
    // Constructor
    explicit DataIngestion(const std::string& dbPath, const DeviceId& deviceId);

    // Ingest single event
    EventId ingestEvent(const RawEvent& event);

    // Batch ingest (more efficient)
    std::vector<EventId> ingestEvents(const std::vector<RawEvent>& events);

    // Statistics
    int getEventCount() const;
    int getUncompressedEventCount() const;
    int getTodayEventCount() const;
};
```

**Example Usage:**
```cpp
#include "layer0/DataIngestion.h"

// Initialize
layer0::DataIngestion ingestion("./data/raw_events.db", "device_001");

// Create event
layer0::RawEvent event;
event.timestamp = std::chrono::system_clock::now();
event.appName = "chrome.exe";
event.windowTitle = "GitHub";
event.interactionCount = 5;

// Ingest
EventId id = ingestion.ingestEvent(event);
std::cout << "Ingested: " << id << std::endl;
```

### ElasticsearchClient Class (Layer 0 - Elasticsearch)

**Location:** `include/layer0/ElasticsearchClient.h`

```cpp
class ElasticsearchClient {
public:
    // Constructor
    explicit ElasticsearchClient(const std::string& esUrl = "http://localhost:9200");

    // Initialize index with mapping
    bool initializeIndex(const std::string& indexName);

    // Index single document
    std::string indexDocument(const std::string& indexName, const RawEvent& event);

    // Bulk indexing (recommended for performance)
    bool bulkIndexDocuments(const std::string& indexName,
                           const std::vector<RawEvent>& events);

    // Query uncompressed events
    std::vector<RawEvent> getUncompressedEvents(int hours = 24);

    // Mark as compressed
    bool markEventsAsCompressed(const std::vector<std::string>& eventIds,
                               const std::string& sessionId);

    // Statistics
    int getDocumentCount(const std::string& indexName);
    int getUncompressedCount();
};
```

### DuckDBManager Class (Layer 1)

**Location:** `include/layer1/DuckDBManager.h`

```cpp
class DuckDBManager {
public:
    // Constructor
    explicit DuckDBManager(const std::string& dbPath);

    // Initialize schema
    void initializeSchema();

    // Insert compressed session
    std::string insertCompressedSession(const CompressedSession& session);

    // Query sessions
    std::vector<CompressedSession> querySessionsByTimeRange(
        const Timestamp& startTime,
        const Timestamp& endTime
    );

    std::vector<CompressedSession> querySessionsByContentType(
        const std::string& contentType,
        int limit = 100
    );

    // Cleanup
    int deleteSessionsOlderThan(const Timestamp& cutoffTime);
};
```

### CompressionPipeline Class (Layer 1)

**Location:** `include/layer1/CompressionPipeline.h`

```cpp
class CompressionPipeline {
public:
    // Constructor
    CompressionPipeline(
        const std::string& sqlitePath,
        const std::string& duckdbPath,
        const SessionConfig& config
    );

    // Main processing method
    int processUncompressedEvents();

    // Get statistics
    CompressionStatistics getStatistics() const;

    // Cleanup compressed sessions
    int cleanupOldCompressedSessions(int retentionDays);
};
```

**Example Usage:**
```cpp
#include "layer1/CompressionPipeline.h"

// Initialize
SessionConfig config;
config.idleThresholdSeconds = 300;  // 5 minutes

CompressionPipeline pipeline(
    "./data/raw_events.db",
    "./data/compressed_sessions.duckdb",
    config
);

// Run compression
int sessionsCompressed = pipeline.processUncompressedEvents();
std::cout << "Compressed " << sessionsCompressed << " sessions" << std::endl;

// Get statistics
auto stats = pipeline.getStatistics();
std::cout << "Compression ratio: " << stats.averageCompressionRatio << std::endl;
```

### DataCollector Class

**Location:** `include/collector/DataCollector.h`

```cpp
class DataCollector {
public:
    // Constructor
    explicit DataCollector(const CollectorConfig& config);

    // Start data collection loop
    void start();

    // Stop collection
    void stop();

    // Statistics
    int getTotalEventsCollected() const;
    int getFailedRequests() const;
};
```

---

## Performance Tuning

### SQLite Optimization

**Pragmas (automatically applied):**
```sql
PRAGMA journal_mode = WAL;           -- Write-Ahead Logging (better concurrency)
PRAGMA synchronous = NORMAL;         -- Balance safety/performance
PRAGMA cache_size = -64000;          -- 64MB cache
PRAGMA temp_store = MEMORY;          -- Store temp tables in memory
PRAGMA mmap_size = 268435456;        -- 256MB memory-mapped I/O
```

**Batch inserts:**
```cpp
// ✅ Good: Batch insert (single transaction)
std::vector<RawEvent> events = fetchEvents();
ingestion.ingestEvents(events);  // ~1000x faster than individual inserts

// ❌ Bad: Individual inserts (transaction per event)
for (const auto& event : events) {
    ingestion.ingestEvent(event);
}
```

### Elasticsearch Optimization

**Bulk indexing:**
```cpp
// ✅ Good: Bulk insert (single HTTP request)
std::vector<RawEvent> events = fetchEvents();
esClient.bulkIndexDocuments("perception_raw_events", events);

// ❌ Bad: Individual indexing (HTTP request per event)
for (const auto& event : events) {
    esClient.indexDocument("perception_raw_events", event);
}
```

**Index settings:**
```json
{
  "settings": {
    "number_of_shards": 1,           // Single-node: use 1 shard
    "number_of_replicas": 0,         // No replicas for single-node
    "refresh_interval": "30s"        // Reduce refresh frequency
  }
}
```

### DuckDB Optimization

**Query optimization:**
```sql
-- ✅ Good: Use time range filter with index
SELECT * FROM compressed_sessions
WHERE device_id = 'device_001'
  AND start_time BETWEEN '2024-01-01' AND '2024-01-31'
ORDER BY start_time;

-- ❌ Bad: Full table scan
SELECT * FROM compressed_sessions
WHERE strftime('%Y-%m', start_time) = '2024-01';
```

**Batch inserts:**
```cpp
// Use prepared statements for bulk inserts
conn->Query("BEGIN TRANSACTION");
for (const auto& session : sessions) {
    insertCompressedSession(session);
}
conn->Query("COMMIT");
```

---

## Troubleshooting

### SQLite Database Locked

**Symptom:** `database is locked` error

**Cause:** Multiple processes trying to write simultaneously

**Solution:**
1. Ensure only one writer at a time (SQLite limitation)
2. Use WAL mode (automatically enabled)
3. Consider switching to Elasticsearch for multi-writer scenarios

### Elasticsearch Connection Failed

**Symptom:** `Connection refused` to port 9200

**Solution:**
```bash
# Check if Elasticsearch is running
docker ps | grep elasticsearch

# If not running, start it
cd database_cpp/database_cpp/elasticsearch_client_dll/docker
docker compose up -d

# Check logs
docker logs elasticsearch

# Test connection
curl http://localhost:9200
```

### DuckDB Out of Memory

**Symptom:** `Out of memory` error during compression

**Cause:** Processing too many events in single batch

**Solution:**
```cpp
// Reduce batch size in CompressionPipeline
// Edit database_cpp/database_cpp/src/layer1/CompressionPipeline.cpp
const int MAX_EVENTS_PER_BATCH = 1000;  // Reduce from default 10000
```

### Data Collector Not Ingesting

**Symptom:** Collector runs but no events in database

**Checklist:**
1. Is Perception Engine running? `curl http://localhost:8777/context`
2. Check collector logs: `./perception_data_collector --verbose`
3. Verify database path exists and is writable
4. Check device_id matches between collector and queries

---

## Future Enhancements

### Short-term (1-3 months)

1. **LLM Compression Integration**
   - Integrate with local LLM (llama.cpp) or API (OpenAI, Claude)
   - Implement dynamic token budgets based on engagement score
   - Add support for multi-language summarization

2. **Complete Layer 2 Implementation**
   - Work session detection and aggregation
   - Day session rollups
   - Cross-session entity tracking

3. **Enhanced Content Extraction**
   - Browser content extraction (full webpage text)
   - PDF/document OCR integration
   - Code syntax parsing for better CODE session analysis

### Medium-term (3-6 months)

4. **Vector Search (Layer 3)**
   - Embed compressed summaries using sentence transformers
   - Store embeddings in Qdrant or Weaviate
   - Semantic search across all sessions

5. **Multi-device Synchronization**
   - Centralized Elasticsearch cluster
   - Cross-device session correlation
   - Unified timeline across devices

6. **Privacy Features**
   - End-to-end encryption for sensitive content
   - Configurable content filtering (PII redaction)
   - Local-only mode (no cloud sync)

### Long-term (6+ months)

7. **Knowledge Graph**
   - Neo4j integration for entity relationships
   - Automatic project/task inference
   - Smart context retrieval for Claude/LLMs

8. **Real-time Analytics Dashboard**
   - Web dashboard for session visualization
   - Time-series charts (engagement over time)
   - Export to Notion/Obsidian

---

## Appendix: Implementation Status

**Legend:**
- ✅ **Completed** - Fully implemented and tested
- 🔨 **In Progress** - Partially implemented
- 📋 **Planned** - Design phase, not yet implemented

### Layer 0 - Raw Event Storage
- ✅ SQLite schema and ingestion
- ✅ Elasticsearch schema and ingestion
- ✅ Event ID generation (SHA-256 hashing)
- ✅ Batch ingestion
- ✅ Statistics API
- ✅ Automatic cleanup (24-hour retention)

### Layer 1 - Compressed Sessions
- ✅ DuckDB schema and storage
- ✅ Session detection algorithm
- ✅ Engagement calculation
- ✅ Content classification
- ✅ Content extraction (high-attention)
- 🔨 LLM compression (framework ready, LLM integration pending)
- ✅ Compression pipeline orchestration

### Layer 2 - Aggregated Sessions
- 📋 Work session detection
- 📋 Day session rollups
- 📋 Entity tracking
- 📋 Project inference

### Data Collector
- ✅ HTTP API polling
- ✅ JSON parsing
- ✅ SQLite backend
- ✅ Elasticsearch backend
- ✅ Error handling and retry logic
- ✅ Command-line interface

### Integration
- ✅ Perception Engine HTTP API (`/context` endpoint)
- ✅ Data collector → Layer 0 pipeline
- 🔨 Layer 0 → Layer 1 compression (manual trigger, auto-scheduling pending)
- 📋 Layer 1 → Layer 2 aggregation

---

## References

**Related Documentation:**
- [CLAUDE.md](CLAUDE.md) - Perception Engine overview and build instructions
- [ARCHITECTURE_CN.md](database_cpp/database_cpp/ARCHITECTURE_CN.md) - Database architecture (Chinese)
- [Elasticsearch Client README](database_cpp/database_cpp/elasticsearch_client_dll/docs/INDEX.md) - Elasticsearch setup

**External Resources:**
- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [DuckDB Documentation](https://duckdb.org/docs/)
- [Elasticsearch Documentation](https://www.elastic.co/guide/en/elasticsearch/reference/current/index.html)

---

**Document Version:** 1.0.0
**Last Updated:** 2025-11-14
**Authors:** Perception Engine Team + Claude
