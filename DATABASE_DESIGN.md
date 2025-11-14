# Database Design Documentation

## Overview

The Nova Perception Engine uses a **three-layer database architecture** optimized for real-time context capture, full-text search, and semantic retrieval. The system is designed to handle high-frequency perception events while enabling powerful search capabilities across both structured metadata and natural language queries.

**Current Architecture:**
- **Layer 0**: Raw event storage with full-text search (Elasticsearch primary, SQLite+FTS5 alternative)
- **Layer 1**: Session compression and LLM summarization
- **Layer 2**: Vector database for semantic search (Qdrant)

**Design Philosophy:**
- Elasticsearch provides distributed, scalable full-text search for production deployments
- SQLite + FTS5 offers a lightweight, embedded alternative for single-device or development scenarios
- Qdrant enables semantic search across LLM-generated session summaries
- All layers support cross-device context correlation

---

## Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    Perception Engine (C++)                        │
│                  Real-time Context Capture                        │
│         (Screen + Audio + Camera + System Metrics)                │
└────────────────────────────┬─────────────────────────────────────┘
                             │ HTTP API (Port 8777)
                             │ GET /context (500ms polling)
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                      Data Collector                               │
│               (perception_data_collector)                         │
│         Polls API → Parses JSON → Ingests Events                 │
└────────────────────────────┬─────────────────────────────────────┘
                             │
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                         LAYER 0                                   │
│                   Raw Event Storage                               │
│                  + Full-Text Search                               │
│                                                                   │
│  ┌──────────────────────┐          ┌──────────────────────┐     │
│  │   Elasticsearch      │   OR     │  SQLite + FTS5       │     │
│  │   (PRIMARY)          │          │  (ALTERNATIVE)       │     │
│  │   Port 9200          │          │  .db file            │     │
│  │                      │          │                      │     │
│  │ ✓ Distributed        │          │ ✓ Embedded           │     │
│  │ ✓ Production-ready   │          │ ✓ Zero config        │     │
│  │ ✓ Multi-device       │          │ ✓ Single-device      │     │
│  │ ✓ REST API           │          │ ✓ Lightweight        │     │
│  └──────────────────────┘          └──────────────────────┘     │
│                                                                   │
│  Features: Full-text search, aggregations, time-series queries   │
│  Retention: 24 hours (configurable)                              │
│  Volume: 2-5 GB/day/device                                       │
│  Write throughput: ~200 events/hour                              │
└────────────────────────────┬─────────────────────────────────────┘
                             │ Session Detection
                             │ (Idle threshold + context switches)
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                         LAYER 1                                   │
│                 Session Compression                               │
│                   + LLM Summarization                             │
│                                                                   │
│  1. Session Detection                                            │
│     • Group events by idle threshold (5 min)                     │
│     • Detect context switches (app/domain changes)               │
│                                                                   │
│  2. Engagement Calculation                                       │
│     • Interaction count, dwell time                              │
│     • Copy/selection events (high attention)                     │
│                                                                   │
│  3. Content Classification                                       │
│     • ContentType: EMAIL, CHAT, CODE, DOCUMENT, etc.             │
│     • Domain: WORK, ENTERTAINMENT, LIFE                          │
│                                                                   │
│  4. LLM Compression (PLANNED)                                    │
│     • Input: Full event content from session                     │
│     • Process: LLM summarization (10-20% of original)            │
│     • Output: Compressed summary + key points                    │
│                                                                   │
│  Storage: Back to Layer 0 (Elasticsearch/SQLite)                 │
│  Field: compressed_session_summary, session_id, embeddings       │
└────────────────────────────┬─────────────────────────────────────┘
                             │ Embedding Generation
                             │ (sentence-transformers, OpenAI, etc.)
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                         LAYER 2                                   │
│                    Vector Database                                │
│                   Semantic Search                                 │
│                                                                   │
│                    ┌──────────────────┐                          │
│                    │     Qdrant       │                          │
│                    │   Port 6333      │                          │
│                    │                  │                          │
│                    │ ✓ Fast vector    │                          │
│                    │   similarity     │                          │
│                    │ ✓ Payload        │                          │
│                    │   filtering      │                          │
│                    │ ✓ Distributed    │                          │
│                    └──────────────────┘                          │
│                                                                   │
│  Storage: Session embeddings (384-1536 dims)                     │
│  Index: HNSW (Hierarchical Navigable Small World)                │
│  Distance: Cosine similarity                                     │
│  Retention: 30 days (configurable)                               │
│                                                                   │
│  Queries:                                                         │
│   • "Find sessions about machine learning projects"              │
│   • "When did I last discuss budget with Alice?"                 │
│   • "Show work sessions related to database design"              │
└──────────────────────────────────────────────────────────────────┘
```

---

## Layer 0: Raw Event Storage + Full-Text Search

### Purpose
Layer 0 serves as the **primary data store** for all perception events, with built-in full-text search capabilities for fast retrieval by keywords, time ranges, and structured filters.

### Storage Options

#### Option A: Elasticsearch (PRIMARY - Production)

**When to use:**
- Multi-device deployments
- Production environments
- Distributed systems requiring high availability
- Teams needing REST API access

**Advantages:**
- ✅ Distributed architecture (horizontal scaling)
- ✅ Built-in full-text search (BM25 ranking)
- ✅ Powerful aggregations (analytics, time-series)
- ✅ REST API (language-agnostic)
- ✅ Production-ready with replication/failover
- ✅ Real-time search (near-instant indexing)
- ✅ Rich query DSL (filters, boosting, highlighting)

**Disadvantages:**
- ❌ Requires separate server deployment (Docker)
- ❌ Higher memory requirements (~2GB minimum)
- ❌ More complex setup and maintenance
- ❌ Overkill for single-device use cases

**Performance:**
- Write throughput: ~5,000 events/sec (bulk indexing)
- Search latency: <50ms (p99)
- Storage overhead: ~3KB/event (with full indexing)
- Memory: ~2-4GB for typical workload

#### Option B: SQLite + FTS5 Extension (ALTERNATIVE - Evaluation)

**When to use:**
- Single-device deployment
- Development/testing environments
- Embedded systems with limited resources
- Privacy-focused local-only scenarios

**Advantages:**
- ✅ Zero configuration (single .db file)
- ✅ Embedded (no separate server)
- ✅ ACID transactions
- ✅ Low memory footprint (~50-100MB)
- ✅ FTS5 provides full-text search capabilities
- ✅ Portable (backup = copy file)
- ✅ Battle-tested reliability

**Disadvantages:**
- ❌ Single-writer limitation (sequential writes)
- ❌ FTS5 performance degrades with large datasets (>10M rows)
- ❌ Limited to ~200 concurrent writes/sec
- ❌ Not suitable for multi-device synchronization
- ❌ Basic ranking compared to Elasticsearch

**Performance (needs measurement):**
- Write throughput: ~200-500 events/sec (with FTS5 indexing)
- Search latency: <100ms for typical queries (needs benchmarking)
- Storage overhead: ~2-3KB/event (with FTS5 index)
- Memory: ~50-100MB

**FTS5 Configuration:**
```sql
-- Create FTS5 virtual table for full-text search
CREATE VIRTUAL TABLE raw_events_fts USING fts5(
    event_id UNINDEXED,
    screen_content,
    window_title,
    voice_transcription,
    camera_description,
    content='raw_events',
    content_rowid='rowid'
);

-- Triggers to keep FTS5 index synchronized
CREATE TRIGGER raw_events_ai AFTER INSERT ON raw_events BEGIN
  INSERT INTO raw_events_fts(rowid, screen_content, window_title, voice_transcription, camera_description)
  VALUES (new.rowid, new.screen_content, new.window_title, new.voice_transcription, new.camera_description);
END;

-- Full-text search query example
SELECT * FROM raw_events
WHERE rowid IN (
  SELECT rowid FROM raw_events_fts
  WHERE raw_events_fts MATCH 'machine learning database design'
  ORDER BY rank
);
```

**Evaluation Criteria:**
- [ ] Benchmark write performance with FTS5 indexing enabled
- [ ] Measure search latency for typical queries (10K, 100K, 1M events)
- [ ] Compare relevance ranking: FTS5 BM25 vs Elasticsearch BM25
- [ ] Test memory usage under load
- [ ] Evaluate FTS5 phrase search and proximity operators

### Recommendation: When to Use Each

| Factor | Elasticsearch | SQLite + FTS5 |
|--------|---------------|---------------|
| **Deployment** | Multi-device, distributed | Single device, embedded |
| **Scale** | >100K events/day | <50K events/day |
| **Team size** | Multiple developers | Solo developer |
| **Infrastructure** | Cloud, containers | Desktop app, edge device |
| **Search complexity** | Complex queries, aggregations | Basic full-text search |
| **Budget** | Have infra budget | Minimize costs |

---

### Schema: `raw_events` Index/Table

**Elasticsearch Mapping:**
```json
{
  "mappings": {
    "properties": {
      "event_id": { "type": "keyword" },
      "timestamp": { "type": "date" },
      "device_id": { "type": "keyword" },

      "app_name": { "type": "keyword" },
      "window_title": {
        "type": "text",
        "fields": { "keyword": { "type": "keyword" } }
      },
      "url": { "type": "text" },

      "screen_content": {
        "type": "text",
        "analyzer": "english"
      },
      "screen_content_hash": { "type": "keyword" },

      "mouse_events": { "type": "nested" },
      "interaction_count": { "type": "integer" },
      "dwell_time_seconds": { "type": "integer" },

      "voice_transcription": { "type": "text" },
      "camera_description": { "type": "text" },

      "battery_percent": { "type": "integer" },
      "is_charging": { "type": "boolean" },
      "network_type": { "type": "keyword" },
      "location": { "type": "geo_point" },
      "cpu_usage": { "type": "float" },
      "memory_usage": { "type": "float" },

      "content_type": { "type": "keyword" },
      "domain": { "type": "keyword" },

      "session_id": { "type": "keyword" },
      "compressed": { "type": "boolean" },

      "compressed_session_summary": { "type": "text" },
      "session_key_points": { "type": "text" },
      "engagement_score": { "type": "float" },

      "created_at": { "type": "date" }
    }
  },
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "refresh_interval": "5s",
    "analysis": {
      "analyzer": {
        "english": {
          "type": "standard",
          "stopwords": "_english_"
        }
      }
    }
  }
}
```

**SQLite Schema (with FTS5):**
```sql
-- Main events table
CREATE TABLE raw_events (
    event_id TEXT PRIMARY KEY,
    timestamp TIMESTAMP NOT NULL,
    device_id TEXT NOT NULL,

    -- App context
    app_name TEXT NOT NULL,
    window_title TEXT,
    url TEXT,

    -- Content
    screen_content TEXT,
    screen_content_hash TEXT,

    -- Interaction signals
    mouse_events TEXT,              -- JSON array
    interaction_count INTEGER DEFAULT 0,
    dwell_time_seconds INTEGER DEFAULT 0,

    -- Multimodal inputs
    voice_transcription TEXT,
    camera_description TEXT,

    -- System info
    battery_percent INTEGER,
    is_charging BOOLEAN DEFAULT 0,
    network_type TEXT,
    location_lat REAL,
    location_lon REAL,
    cpu_usage REAL,
    memory_usage REAL,

    -- Classification
    content_type TEXT,
    domain TEXT,

    -- Session linking
    session_id TEXT,
    compressed BOOLEAN DEFAULT 0,

    -- Compressed session data (added after Layer 1 processing)
    compressed_session_summary TEXT,
    session_key_points TEXT,        -- JSON array
    engagement_score REAL,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FTS5 virtual table for full-text search
CREATE VIRTUAL TABLE raw_events_fts USING fts5(
    screen_content,
    window_title,
    voice_transcription,
    camera_description,
    compressed_session_summary,
    content='raw_events',
    content_rowid='rowid',
    tokenize='porter unicode61'
);

-- Standard indexes
CREATE INDEX idx_timestamp ON raw_events(timestamp);
CREATE INDEX idx_device_session ON raw_events(device_id, session_id);
CREATE INDEX idx_compressed ON raw_events(compressed);
CREATE INDEX idx_content_type ON raw_events(content_type);
CREATE INDEX idx_content_hash ON raw_events(screen_content_hash);

-- FTS5 sync triggers
CREATE TRIGGER raw_events_ai AFTER INSERT ON raw_events BEGIN
  INSERT INTO raw_events_fts(rowid, screen_content, window_title, voice_transcription, camera_description, compressed_session_summary)
  VALUES (new.rowid, new.screen_content, new.window_title, new.voice_transcription, new.camera_description, new.compressed_session_summary);
END;

CREATE TRIGGER raw_events_au AFTER UPDATE ON raw_events BEGIN
  UPDATE raw_events_fts SET
    screen_content = new.screen_content,
    window_title = new.window_title,
    voice_transcription = new.voice_transcription,
    camera_description = new.camera_description,
    compressed_session_summary = new.compressed_session_summary
  WHERE rowid = new.rowid;
END;

CREATE TRIGGER raw_events_ad AFTER DELETE ON raw_events BEGIN
  DELETE FROM raw_events_fts WHERE rowid = old.rowid;
END;
```

### Full-Text Search Examples

**Elasticsearch Query DSL:**
```json
{
  "query": {
    "bool": {
      "must": [
        {
          "multi_match": {
            "query": "machine learning database design",
            "fields": ["screen_content^2", "window_title^1.5", "voice_transcription", "compressed_session_summary^3"],
            "type": "best_fields",
            "operator": "and"
          }
        }
      ],
      "filter": [
        { "term": { "device_id": "laptop_001" } },
        { "range": { "timestamp": { "gte": "2024-01-01", "lte": "2024-01-31" } } },
        { "term": { "content_type": "CODE" } }
      ]
    }
  },
  "highlight": {
    "fields": {
      "screen_content": {},
      "compressed_session_summary": {}
    }
  },
  "sort": [
    { "_score": "desc" },
    { "engagement_score": "desc" }
  ]
}
```

**SQLite + FTS5 Query:**
```sql
-- Basic full-text search
SELECT
    e.*,
    fts.rank
FROM raw_events e
JOIN raw_events_fts fts ON e.rowid = fts.rowid
WHERE fts MATCH 'machine learning database design'
  AND e.device_id = 'laptop_001'
  AND e.timestamp BETWEEN '2024-01-01' AND '2024-01-31'
  AND e.content_type = 'CODE'
ORDER BY fts.rank, e.engagement_score DESC;

-- Phrase search with proximity
SELECT * FROM raw_events
WHERE rowid IN (
  SELECT rowid FROM raw_events_fts
  WHERE raw_events_fts MATCH '"machine learning" NEAR/5 database'
);

-- Boolean operators
SELECT * FROM raw_events
WHERE rowid IN (
  SELECT rowid FROM raw_events_fts
  WHERE raw_events_fts MATCH '(python OR javascript) AND (async OR concurrent) NOT deprecated'
);
```

### Data Retention

**Automatic cleanup policy:**
- Raw events: 24 hours (default, configurable)
- Compressed sessions: Kept in same table with `session_id` field populated
- Cleanup runs hourly via scheduled task

**Elasticsearch:**
```json
// Delete old events using Delete By Query API
POST /perception_raw_events/_delete_by_query
{
  "query": {
    "range": {
      "timestamp": {
        "lt": "now-24h"
      }
    }
  }
}
```

**SQLite:**
```sql
-- Delete old events
DELETE FROM raw_events
WHERE timestamp < datetime('now', '-24 hours')
  AND compressed = 1;  -- Only delete if already compressed

-- Vacuum to reclaim space
VACUUM;
```

---

## Layer 1: Session Compression + LLM Summarization

### Purpose
Layer 1 transforms raw event streams into meaningful sessions with:
1. **Session boundaries** detected via idle thresholds and context switches
2. **Engagement metrics** calculated from interaction patterns
3. **Content classification** (type, domain)
4. **LLM-generated summaries** (10-20% of original content)
5. **Preparation for vector embedding** (Layer 2)

### Session Detection Algorithm

**Triggers for session boundary:**

1. **Idle threshold:** Gap between events > 5 minutes (configurable)
2. **Domain change:** WORK → ENTERTAINMENT, etc.
3. **App type change:** Browser → IDE → Video player
4. **Content type change:** EMAIL → CHAT → CODE

**Implementation:**
```cpp
// Pseudo-code for session detection
bool shouldBreakSession(const RawEvent& prev, const RawEvent& curr) {
    // Idle threshold
    auto timeDiff = curr.timestamp - prev.timestamp;
    if (timeDiff > config.idleThresholdSeconds) return true;

    // Domain change
    if (prev.domain != curr.domain && config.domainChangeTriggers) return true;

    // App type change (major category)
    if (getAppCategory(prev.appName) != getAppCategory(curr.appName)) return true;

    // Content type change
    if (prev.contentType != curr.contentType && config.contentTypeChangeTriggers) return true;

    return false;
}
```

**Example session grouping:**
```
Event 1: chrome.exe, GitHub,    10:00:00, WORK, CODE   → Session A
Event 2: chrome.exe, GitHub,    10:05:00, WORK, CODE   → Session A
Event 3: chrome.exe, Gmail,     10:10:00, WORK, EMAIL  → Session B (content type change)
Event 4: code.exe,   main.cpp,  10:20:00, WORK, CODE   → Session C (app change)
Event 5: code.exe,   main.cpp,  10:30:00, WORK, CODE   → Session C
Event 6: youtube.com, Video,    11:00:00, ENT,  VIDEO  → Session D (domain change + idle)
```

### Engagement Calculation

**Formula:**
```cpp
engagement_score = (
    normalized_interaction_count * 0.3 +
    normalized_dwell_time * 0.2 +
    has_copied * 0.25 +
    has_selected * 0.15 +
    normalized_selection_count * 0.1
)

Where:
- normalized_interaction_count = min(1.0, interaction_count / 50)
- normalized_dwell_time = min(1.0, dwell_time_seconds / 300)  // 5 min cap
- normalized_selection_count = min(1.0, selection_count / 10)
```

**Engagement categories:**
- **0.0 - 0.3**: Low engagement (passive viewing)
- **0.3 - 0.6**: Medium engagement (active reading)
- **0.6 - 0.8**: High engagement (deep work)
- **0.8 - 1.0**: Very high engagement (flow state)

### Content Classification

**ContentType Taxonomy:**
```cpp
enum class ContentType {
    EMAIL,              // Outlook, Gmail, Thunderbird
    CHAT,               // Slack, Teams, Discord, WhatsApp
    MEETING,            // Zoom, Teams, Google Meet
    CODE,               // VS Code, Visual Studio, IntelliJ, Vim
    DOCUMENT,           // Word, Google Docs, Notion, Obsidian
    WEB_ARTICLE,        // Medium, blogs, news articles (>1000 words)
    WEB_PAGE,           // General web browsing
    VIDEO,              // YouTube, Netflix, streaming
    SOCIAL,             // Twitter, Facebook, LinkedIn, Reddit
    RESEARCH_PAPER,     // arXiv, Google Scholar, PDF papers
    SHOPPING,           // Amazon, e-commerce sites
    UNKNOWN
};
```

**Domain Taxonomy:**
```cpp
enum class Domain {
    WORK,               // Productivity, development, email, meetings
    ENTERTAINMENT,      // Videos, music, gaming, streaming
    LIFE,               // Shopping, banking, health, travel, personal
    INTERACTION,        // Chat, social media, communication
    SYSTEM              // Settings, utilities, OS-level
};
```

### LLM Compression (PLANNED)

**Workflow:**
```
1. Collect all screen_content + voice_transcription + camera_description from session events
2. Concatenate into single context string (may be 10K-100K characters)
3. Calculate engagement score (determines compression budget)
4. Call LLM with dynamic prompt based on content_type and engagement

Prompt template:
---
Summarize the following {content_type} session (engagement: {engagement_score}).
Focus on key actions, decisions, and outcomes.

Content:
{concatenated_content}

Generate:
1. A concise summary (100-200 words)
2. 3-5 key points (bullet list)

If engagement < 0.3: Ultra-brief summary (50 words max)
If engagement > 0.7: Detailed summary (200 words) + entities extraction
---

5. Parse LLM response → compressed_session_summary + session_key_points
6. Store back in Layer 0 (update same event row or create session record)
```

**Token Budget Strategy:**
```cpp
int calculateTokenBudget(double engagementScore, ContentType type) {
    int baseTokens;

    // Base budget by content type
    switch (type) {
        case ContentType::CODE:
        case ContentType::RESEARCH_PAPER:
            baseTokens = 200;  // Technical content needs more detail
            break;
        case ContentType::EMAIL:
        case ContentType::CHAT:
            baseTokens = 100;  // Communication can be brief
            break;
        case ContentType::DOCUMENT:
        case ContentType::WEB_ARTICLE:
            baseTokens = 150;
            break;
        default:
            baseTokens = 100;
    }

    // Scale by engagement
    int scaledTokens = baseTokens * (0.5 + engagementScore);  // 0.5x to 1.5x multiplier

    return scaledTokens;
}
```

**LLM Options:**
- **Local**: llama.cpp (Llama 3.1 8B, Mistral 7B)
- **API**: OpenAI GPT-4o-mini, Anthropic Claude Haiku
- **Self-hosted**: Ollama, vLLM, TGI

**Expected compression ratio:** 85-95% reduction in storage

### Storage After Compression

**Option 1: Update existing events (current approach)**
```sql
-- Add session summary fields to each event in the session
UPDATE raw_events
SET
    session_id = 'session_uuid_123',
    compressed = 1,
    compressed_session_summary = 'LLM-generated summary...',
    session_key_points = '["Point 1", "Point 2", "Point 3"]',
    engagement_score = 0.75
WHERE event_id IN (SELECT event_id FROM events_in_session);
```

**Option 2: Separate sessions table (alternative)**
```sql
-- Create dedicated sessions table
CREATE TABLE sessions (
    session_id TEXT PRIMARY KEY,
    device_id TEXT NOT NULL,
    start_time TIMESTAMP NOT NULL,
    end_time TIMESTAMP NOT NULL,
    duration_seconds INTEGER,

    content_type TEXT,
    domain TEXT,
    app_name TEXT,

    engagement_score REAL,
    interaction_count INTEGER,
    total_dwell_time INTEGER,

    compressed_summary TEXT,
    key_points TEXT,  -- JSON array

    event_ids TEXT,   -- JSON array of event IDs in this session

    created_at TIMESTAMP
);

-- Link events to sessions
UPDATE raw_events
SET session_id = 'session_uuid_123', compressed = 1
WHERE event_id IN (...);
```

---

## Layer 2: Vector Database - Semantic Search

### Purpose
Layer 2 enables **semantic search** across session summaries, allowing natural language queries like:
- "Find sessions about machine learning projects"
- "When did I last discuss budget planning with Alice?"
- "Show all code sessions related to database optimization"

### Technology: Qdrant (Primary Choice)

**Why Qdrant?**
- ✅ **Fast vector similarity search** (HNSW index, <10ms p99 latency)
- ✅ **Payload filtering** (filter by metadata: device_id, timestamp, content_type)
- ✅ **Distributed** (supports clustering for high availability)
- ✅ **gRPC + REST APIs** (high performance + easy integration)
- ✅ **Rich query capabilities** (hybrid search: vector + filter)
- ✅ **Snapshots & backups** (production-ready)
- ✅ **Docker deployment** (easy setup)
- ✅ **Active development** (Rust-based, well-maintained)

**Architecture:**
```
┌─────────────────────────────────────────────────────────────┐
│                      Qdrant Server                          │
│                      Port 6333 (REST)                       │
│                      Port 6334 (gRPC)                       │
│                                                              │
│  Collections:                                               │
│  └─ perception_sessions                                     │
│      ├─ Vectors: [384-dim or 1536-dim embeddings]          │
│      └─ Payload:                                            │
│          ├─ session_id                                      │
│          ├─ device_id                                       │
│          ├─ start_time / end_time                           │
│          ├─ content_type, domain                            │
│          ├─ engagement_score                                │
│          ├─ compressed_summary (for display)                │
│          └─ key_points                                      │
└─────────────────────────────────────────────────────────────┘
```

### Alternative Vector Database Options

| Database | Pros | Cons | Best For |
|----------|------|------|----------|
| **Qdrant** | Fast, payload filtering, distributed | Newer (less mature than some) | Production, multi-device |
| **Weaviate** | GraphQL API, modular architecture | Higher complexity | Complex schemas, graphs |
| **Milvus** | Highly scalable, battle-tested | Complex setup, heavy | Large-scale (>10M vectors) |
| **Chroma** | Embedded, easy to use, Python-native | Limited filtering, single-node | Development, prototyping |
| **Pinecone** | Managed service, zero ops | Proprietary, expensive, vendor lock-in | Quick start, SaaS preference |
| **pgvector** | PostgreSQL extension, familiar SQL | Slower than specialized DBs | Already using PostgreSQL |

**Recommendation:**
- **Qdrant** for production (best balance of features, performance, ease of use)
- **Chroma** for local development/testing (embedded, no server needed)
- **Weaviate** if you need complex entity relationships and graph queries

### Qdrant Setup

**Docker Compose:**
```yaml
version: '3.8'

services:
  qdrant:
    image: qdrant/qdrant:latest
    container_name: qdrant
    ports:
      - "6333:6333"  # REST API
      - "6334:6334"  # gRPC API
    volumes:
      - qdrant_storage:/qdrant/storage
    environment:
      - QDRANT__SERVICE__GRPC_PORT=6334
    restart: unless-stopped

volumes:
  qdrant_storage:
```

**Start Qdrant:**
```bash
cd database_cpp/database_cpp/qdrant_deployment  # (create this directory)
docker compose up -d

# Verify
curl http://localhost:6333/collections
```

### Collection Schema

**Create collection:**
```json
PUT http://localhost:6333/collections/perception_sessions

{
  "vectors": {
    "size": 384,  // or 1536 for OpenAI embeddings
    "distance": "Cosine"
  },
  "optimizers_config": {
    "indexing_threshold": 10000
  },
  "hnsw_config": {
    "m": 16,
    "ef_construct": 100
  }
}
```

**Point structure:**
```json
{
  "id": "session_uuid_123",
  "vector": [0.1, -0.5, 0.3, ...],  // 384 or 1536 dimensions
  "payload": {
    "session_id": "session_uuid_123",
    "device_id": "laptop_001",
    "start_time": "2024-01-15T10:00:00Z",
    "end_time": "2024-01-15T11:30:00Z",
    "duration_seconds": 5400,
    "content_type": "CODE",
    "domain": "WORK",
    "app_name": "code.exe",
    "engagement_score": 0.85,
    "compressed_summary": "Worked on database design for perception engine. Implemented session detection algorithm and tested with sample data. Fixed bug in engagement calculation.",
    "key_points": [
      "Implemented session detection using idle threshold",
      "Added engagement score calculation",
      "Fixed bug in scoring formula"
    ]
  }
}
```

### Embedding Generation

**Embedding Model Options:**

| Model | Dimensions | Speed | Quality | Cost |
|-------|------------|-------|---------|------|
| **all-MiniLM-L6-v2** | 384 | Very fast | Good | Free (local) |
| **all-mpnet-base-v2** | 768 | Fast | Better | Free (local) |
| **OpenAI text-embedding-3-small** | 1536 | API latency | Excellent | $0.02 / 1M tokens |
| **OpenAI text-embedding-3-large** | 3072 | API latency | Best | $0.13 / 1M tokens |

**Recommendation:**
- **Development**: all-MiniLM-L6-v2 (fast, good enough, free)
- **Production**: OpenAI text-embedding-3-small (best quality/cost ratio)
- **Privacy-focused**: all-mpnet-base-v2 (local, no API calls)

**Embedding workflow:**
```python
# Using sentence-transformers (local)
from sentence_transformers import SentenceTransformer

model = SentenceTransformer('all-MiniLM-L6-v2')
embedding = model.encode(session_summary)  # Returns 384-dim vector

# Using OpenAI API
import openai

response = openai.embeddings.create(
    model="text-embedding-3-small",
    input=session_summary
)
embedding = response.data[0].embedding  # Returns 1536-dim vector
```

### Semantic Search Examples

**Basic similarity search:**
```json
POST http://localhost:6333/collections/perception_sessions/points/search

{
  "vector": [0.1, -0.5, 0.3, ...],  // Query embedding
  "limit": 10,
  "with_payload": true
}
```

**Hybrid search (vector + filter):**
```json
POST http://localhost:6333/collections/perception_sessions/points/search

{
  "vector": [0.1, -0.5, 0.3, ...],
  "filter": {
    "must": [
      { "key": "device_id", "match": { "value": "laptop_001" } },
      { "key": "content_type", "match": { "value": "CODE" } },
      {
        "key": "start_time",
        "range": {
          "gte": "2024-01-01T00:00:00Z",
          "lte": "2024-01-31T23:59:59Z"
        }
      },
      {
        "key": "engagement_score",
        "range": { "gte": 0.5 }
      }
    ]
  },
  "limit": 10,
  "with_payload": true
}
```

**Query examples (user intent → embedding → search):**

```
User query: "machine learning database sessions"
  ↓ Embed with same model
Embedding: [0.12, -0.43, 0.67, ...]
  ↓ Search in Qdrant
Results:
  1. [Score: 0.92] "Designed database schema for ML feature store..."
  2. [Score: 0.88] "Implemented vector similarity search for embeddings..."
  3. [Score: 0.85] "Researched DuckDB vs ClickHouse for ML pipelines..."
```

### Qdrant C++ Client

**Options:**
1. **HTTP/REST client** using libcurl (easiest)
2. **gRPC client** using official Qdrant C++ SDK (faster, more features)

**Example (REST API with libcurl):**
```cpp
#include <curl/curl.h>
#include <nlohmann/json.hpp>

class QdrantClient {
public:
    QdrantClient(const std::string& url) : baseUrl_(url) {}

    // Upsert point (insert or update)
    bool upsertPoint(const std::string& collection,
                     const std::string& pointId,
                     const std::vector<float>& vector,
                     const nlohmann::json& payload) {
        nlohmann::json body = {
            {"points", {{
                {"id", pointId},
                {"vector", vector},
                {"payload", payload}
            }}}
        };

        std::string response;
        return httpRequest("PUT",
                          "/collections/" + collection + "/points",
                          body.dump(),
                          response);
    }

    // Search similar points
    std::vector<SearchResult> search(const std::string& collection,
                                     const std::vector<float>& queryVector,
                                     int limit = 10,
                                     const nlohmann::json& filter = {}) {
        nlohmann::json body = {
            {"vector", queryVector},
            {"limit", limit},
            {"with_payload", true}
        };

        if (!filter.empty()) {
            body["filter"] = filter;
        }

        std::string response;
        httpRequest("POST",
                   "/collections/" + collection + "/points/search",
                   body.dump(),
                   response);

        return parseSearchResults(response);
    }

private:
    std::string baseUrl_;

    bool httpRequest(const std::string& method,
                    const std::string& endpoint,
                    const std::string& body,
                    std::string& response);

    std::vector<SearchResult> parseSearchResults(const std::string& json);
};
```

---

## Integration: Complete Data Flow

### End-to-End Pipeline

```
1. CAPTURE (Perception Engine)
   └─> Screen + Audio + Camera → ContextCollector → HTTP API (port 8777)

2. INGEST (Data Collector → Layer 0)
   └─> Poll /context → Parse JSON → RawEvent → Elasticsearch/SQLite

3. SESSION DETECTION (Layer 1 - Scheduled job, runs every 5 min)
   └─> Query uncompressed events → Group by session → Calculate engagement

4. LLM COMPRESSION (Layer 1 - For high-engagement sessions)
   └─> Concatenate session content → LLM summarization → Update Layer 0

5. EMBEDDING GENERATION (Layer 1 → Layer 2)
   └─> Session summary → Embedding model → Vector (384 or 1536 dims)

6. VECTOR INDEXING (Layer 2)
   └─> Upsert to Qdrant → Index with payload

7. SEMANTIC SEARCH (Query time)
   └─> User query → Embed query → Qdrant search → Retrieve sessions
```

### Code Locations

```
perception_engine/
├── windows_code/                          # Perception Engine (Step 1)
│   ├── PerceptionEngine.cpp               # Main context capture
│   ├── ContextCollector.cpp               # Context aggregation
│   └── HttpServer.cpp                     # /context API endpoint
│
├── database_cpp/database_cpp/
│   ├── src/collector/                     # Data ingestion (Step 2)
│   │   ├── DataCollector.cpp              # API polling
│   │   └── main.cpp                       # Collector executable
│   │
│   ├── src/layer0/                        # Layer 0 storage
│   │   ├── ElasticsearchClient.cpp        # Elasticsearch backend
│   │   └── DataIngestion.cpp              # SQLite backend (alternative)
│   │
│   ├── src/layer1/                        # Session processing (Steps 3-5)
│   │   ├── SessionDetector.cpp            # Session boundaries
│   │   ├── EngagementCalculator.cpp       # Engagement metrics
│   │   ├── ContentClassifier.cpp          # Content classification
│   │   ├── CompressionPipeline.cpp        # LLM orchestration (TODO)
│   │   └── EmbeddingGenerator.cpp         # Vector generation (TODO)
│   │
│   └── src/layer2/                        # Vector DB (Steps 6-7) (TODO)
│       ├── QdrantClient.cpp               # Qdrant integration
│       └── SemanticSearch.cpp             # Query interface
│
└── deployment/
    ├── docker-compose.elasticsearch.yml   # Elasticsearch + Kibana
    └── docker-compose.qdrant.yml          # Qdrant vector DB
```

### Configuration

**Unified config file:** `perception_data/config.json`

```json
{
  "layer0": {
    "backend": "elasticsearch",  // or "sqlite"
    "elasticsearch": {
      "url": "http://localhost:9200",
      "index": "perception_raw_events",
      "bulkSize": 100
    },
    "sqlite": {
      "dbPath": "./perception_data/raw_events.db",
      "enableFts5": true
    },
    "retentionHours": 24
  },

  "layer1": {
    "sessionDetection": {
      "idleThresholdSeconds": 300,
      "domainChangeTriggers": true,
      "contentTypeChangeTriggers": true
    },
    "llmCompression": {
      "enabled": true,
      "provider": "openai",  // or "local", "anthropic"
      "model": "gpt-4o-mini",
      "minEngagementForCompression": 0.3,
      "localModelPath": "./models/llama-3.1-8b.gguf"
    },
    "embedding": {
      "provider": "sentence-transformers",  // or "openai"
      "model": "all-MiniLM-L6-v2",
      "dimensions": 384
    }
  },

  "layer2": {
    "qdrant": {
      "url": "http://localhost:6333",
      "collection": "perception_sessions",
      "distanceMetric": "Cosine"
    },
    "retentionDays": 30
  },

  "collector": {
    "apiUrl": "http://localhost:8777/context",
    "pollIntervalSeconds": 5,
    "deviceId": "laptop_001"
  }
}
```

---

## Performance Benchmarks (To Be Measured)

### Layer 0: Storage Performance

**Elasticsearch (expected):**
- Write throughput: 5,000 events/sec (bulk)
- Search latency: <50ms (p99)
- Storage: ~3KB/event
- Memory: ~2GB base + 1GB per 1M events

**SQLite + FTS5 (needs measurement):**
- Write throughput: ? events/sec (with FTS5 indexing)
- Search latency: ? ms (full-text search)
- Storage: ~2-3KB/event
- Memory: ~50-100MB base + ? per 1M events

**Benchmark plan:**
```bash
# Test 1: Write performance
- Ingest 10K, 100K, 1M events
- Measure: events/sec, latency p50/p99, memory usage

# Test 2: Search performance
- Run 100 random full-text queries
- Measure: latency p50/p99, relevance quality

# Test 3: Storage efficiency
- Compare disk usage after 1 week of data
- Measure: compression ratio, index overhead
```

### Layer 1: Compression Performance

**LLM compression (estimated):**
- Local (llama.cpp, 8B model): ~500ms/session (GPU), 2-5s (CPU)
- API (GPT-4o-mini): ~200-500ms/session
- Compression ratio: 85-95% reduction

### Layer 2: Vector Search Performance

**Qdrant (expected):**
- Indexing: <10ms/vector
- Search latency: <10ms (p99) for <1M vectors
- Memory: ~4 bytes/dim/vector (e.g., 384 dims = 1.5KB/vector)

---

## Deployment Guide

### Quick Start (Development)

```bash
# 1. Start Elasticsearch
cd database_cpp/database_cpp/elasticsearch_client_dll/docker
docker compose up -d elasticsearch

# 2. Start Qdrant
cd ../../qdrant_deployment
docker compose up -d qdrant

# 3. Build database components
cd ../..
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 4. Start Perception Engine (in separate terminal)
cd ../../../../windows_code/build/bin/Release
./PerceptionEngine.exe --console

# 5. Start Data Collector
cd ../../../../database_cpp/database_cpp/build
./perception_data_collector \
  --storage elasticsearch \
  --es-url http://localhost:9200 \
  --api-url http://localhost:8777/context

# 6. Run session compression (manual trigger for now)
./session_compressor \
  --es-url http://localhost:9200 \
  --qdrant-url http://localhost:6333
```

### Production Deployment

**Architecture:**
```
┌───────────────────────────────────────────────────────────┐
│                    Load Balancer                          │
│                   (nginx / Traefik)                       │
└─────────────────────────┬─────────────────────────────────┘
                          │
          ┌───────────────┼───────────────┐
          │               │               │
┌─────────▼─────┐  ┌──────▼──────┐  ┌────▼────────┐
│ Perception    │  │ Perception  │  │ Perception  │
│ Engine (Dev1) │  │ Engine (Dev2│  │ Engine (Dev3│
│ Port 8777     │  │ Port 8777   │  │ Port 8777   │
└───────┬───────┘  └──────┬──────┘  └────┬────────┘
        │                 │               │
        └─────────────────┼───────────────┘
                          │
                ┌─────────▼──────────┐
                │  Data Collectors   │
                │   (Kubernetes)     │
                └─────────┬──────────┘
                          │
          ┌───────────────┼───────────────┐
          │               │               │
┌─────────▼─────────┐  ┌──▼─────────┐  ┌─▼─────────┐
│ Elasticsearch     │  │ Qdrant     │  │ Redis     │
│ Cluster (3 nodes) │  │ Cluster    │  │ (cache)   │
│ Port 9200         │  │ Port 6333  │  │ Port 6379 │
└───────────────────┘  └────────────┘  └───────────┘
```

---

## Troubleshooting

### Elasticsearch Issues

**Issue: "Connection refused" to port 9200**
```bash
# Check if running
docker ps | grep elasticsearch

# Check logs
docker logs elasticsearch

# Restart
docker compose restart elasticsearch
```

**Issue: "Out of memory" errors**
```yaml
# Increase heap size in docker-compose.yml
environment:
  - "ES_JAVA_OPTS=-Xms4g -Xmx4g"  # Increase from default 2g
```

### SQLite + FTS5 Issues

**Issue: "no such module: fts5"**
```bash
# Verify FTS5 is compiled in
sqlite3 test.db "PRAGMA compile_options;" | grep FTS5

# If missing, rebuild SQLite with FTS5 enabled
# Or use system SQLite (usually has FTS5)
```

**Issue: Slow full-text search**
```sql
-- Rebuild FTS5 index
INSERT INTO raw_events_fts(raw_events_fts) VALUES('rebuild');

-- Optimize index
INSERT INTO raw_events_fts(raw_events_fts) VALUES('optimize');
```

### Qdrant Issues

**Issue: "Collection not found"**
```bash
# Create collection
curl -X PUT http://localhost:6333/collections/perception_sessions \
  -H 'Content-Type: application/json' \
  -d '{
    "vectors": {
      "size": 384,
      "distance": "Cosine"
    }
  }'
```

---

## Future Enhancements

### Short-term (1-3 months)

- [ ] **Complete LLM compression integration**
  - Local llama.cpp integration
  - OpenAI/Anthropic API integration
  - Dynamic token budget based on engagement

- [ ] **Embedding pipeline**
  - sentence-transformers integration
  - Batch embedding generation
  - Automatic Qdrant indexing

- [ ] **SQLite + FTS5 benchmarking**
  - Performance comparison vs Elasticsearch
  - Document decision: production backend choice

### Medium-term (3-6 months)

- [ ] **Hybrid search**
  - Combine full-text (Elasticsearch/FTS5) + semantic (Qdrant)
  - Reciprocal Rank Fusion (RRF) for result merging

- [ ] **Multi-device synchronization**
  - Central Elasticsearch cluster
  - Cross-device session correlation

- [ ] **Privacy features**
  - Content filtering (PII redaction)
  - End-to-end encryption for sensitive sessions
  - Local-only mode

### Long-term (6+ months)

- [ ] **Knowledge graph (Layer 3)**
  - Entity extraction and linking
  - Neo4j integration for relationships
  - Project/task inference

- [ ] **Real-time analytics dashboard**
  - Grafana + Elasticsearch integration
  - Session visualization, engagement charts

- [ ] **Claude/LLM integration**
  - MCP server with semantic search
  - "Ask Claude about my past work" feature

---

## Appendix: Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| **Layer 0 - Elasticsearch** | ✅ Implemented | Fully functional, production-ready |
| **Layer 0 - SQLite + FTS5** | 📋 Evaluation | Need performance benchmarks |
| **Data Collector** | ✅ Implemented | Supports both ES and SQLite |
| **Session Detection** | ✅ Implemented | Idle threshold + context switches |
| **Engagement Calculation** | ✅ Implemented | Weighted scoring formula |
| **Content Classification** | ✅ Implemented | Rule-based classifier |
| **LLM Compression** | 🔨 In Progress | Framework ready, LLM integration pending |
| **Embedding Generation** | 📋 Planned | sentence-transformers integration |
| **Layer 2 - Qdrant** | 📋 Planned | Schema designed, client pending |
| **Semantic Search API** | 📋 Planned | Query interface design |

**Legend:**
- ✅ Implemented and tested
- 🔨 In progress
- 📋 Planned (not started)

---

## References

**Internal Documentation:**
- [CLAUDE.md](CLAUDE.md) - Perception Engine overview
- [ARCHITECTURE_CN.md](database_cpp/database_cpp/ARCHITECTURE_CN.md) - Original database design (Chinese)
- [Elasticsearch Client Docs](database_cpp/database_cpp/elasticsearch_client_dll/docs/INDEX.md)

**External Resources:**
- [Elasticsearch Documentation](https://www.elastic.co/guide/en/elasticsearch/reference/current/index.html)
- [SQLite FTS5 Extension](https://www.sqlite.org/fts5.html)
- [Qdrant Documentation](https://qdrant.tech/documentation/)
- [sentence-transformers](https://www.sbert.net/)

---

**Document Version:** 2.0.0
**Last Updated:** 2025-11-14
**Authors:** Perception Engine Team + Claude

**Changelog:**
- v2.0.0 (2025-11-14): Complete rewrite to reflect Elasticsearch + Qdrant architecture
- v1.0.0 (2025-11-14): Initial version (DuckDB-based, deprecated)
