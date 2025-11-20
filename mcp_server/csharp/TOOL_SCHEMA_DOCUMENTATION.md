# MCP Server Tool Schema Documentation - Perception Engine

**Version:** 1.0.0
**Last Updated:** 2025-11-14
**Status:** Production Design for v1 Sprint
**Target LLM:** Azure GPT (GPT-4, GPT-4o)

---

## Executive Summary

This document specifies the production-ready tool schema for the Perception Engine MCP Server, designed to bridge real-time user context and historical activity data to Azure GPT via the Model Context Protocol (MCP).

**Current State:**
- ✅ 2 tools in production: `get_perception_context`, `query_perception_history`
- ✅ Basic keyword search over raw events
- ✅ HTTP bridge to Perception Engine

**Target State (End of Week 1 Sprint):**
- 🎯 3 production tools with clear time-scale separation
- 🎯 Semantic search via Qdrant vector database (Layer 2)
- 🎯 Local embedding generation (sentence-transformers)
- 🎯 Robust error handling and graceful degradation
- 🎯 Optional: Context enrichment based on current app

**Architecture:**
```
Azure GPT (Client)
    ↓ MCP Protocol
MCP Server (C# Bridge)
    ↓ HTTP API
Perception Engine (C++ Backend)
    ↓ Data Layers
├─ Layer 0: Elasticsearch/SQLite (24h raw events, keyword search)
└─ Layer 2: Qdrant (30d sessions, semantic search)
```

---

## Tool Schema Overview

### Design Philosophy

**Three tools, three time scales:**
1. **Real-time** (0 sec): `get_current_context` - What's happening RIGHT NOW
2. **Recent raw** (1-24h): `search_recent_activity` - Keyword search in today's events
3. **Long-term semantic** (7-30d): `find_related_sessions` - Concept search across past work

**Key Design Principles:**
- ✅ **Clear time boundaries** - No overlap, easy LLM tool selection
- ✅ **Simple parameters** - Avoid complex nested objects (error-prone)
- ✅ **Explicit use cases** - Description engineering for accurate tool RAG
- ✅ **Graceful degradation** - Fallbacks when services unavailable

---

## Tool 1: `get_current_context`

### Status
✅ **Production** (exists, minor refactoring)

### Purpose
Provides real-time snapshot of user's current activity. This tool is designed to be called automatically/frequently to ground LLM responses in current context.

### Specification

```csharp
[Description(
    "🔴 Get REAL-TIME snapshot of what the user is doing RIGHT NOW (this moment). " +
    "Returns: Currently active application, window title, live voice transcription, " +
    "camera vision description, system metrics (CPU, memory, battery), network status. " +
    "⏱️ Time scope: Present moment only (last 5 seconds). " +
    "💡 Use when: User asks 'what am I doing now', 'what's on my screen', " +
    "'am I in a meeting', or when you need to ground responses in current activity. " +
    "May include enriched context (e.g., recent related activity based on current app). " +
    "❌ NOT for: Historical queries - use search_recent_activity or find_related_sessions instead."
)]
[McpServerTool(
    Name = "get_current_context",
    Title = "Get Current Context",
    Idempotent = true,
    ReadOnly = true
)]
public async Task<CallToolResult> GetCurrentContextAsync(
    CancellationToken cancellationToken)
{
    // Implementation remains mostly unchanged
    // Returns formatted markdown with current context
}
```

### Parameters
None.

### Backend Endpoint
`GET http://localhost:8777/context`

### Response Format
```markdown
# Perception Engine Context

**Summary:** Active: chrome.exe (GitHub PR review) | Said: "looks good to me"

**Active Application:** chrome.exe
**Active App Content:**
```
Pull Request #123: Fix database migration bug
Files changed: 3 (database/migration.sql, ...)
```

**Voice:** "looks good to me"
  (Latency: 180ms)

**Camera Vision:** Person sitting at desk, looking at screen
  (Latency: 1.7s)

## System Status
- **CPU:** 25.3%
- **Memory:** 8.2 / 16.0 GB (51.2%)
- **Battery:** 85% (Charging)
- **Network:** Connected (WiFi)

## Recent Applications
- chrome.exe (120s)
- code.exe (45s)
- slack.exe (30s)

## Performance Metrics
- Context Update: 28ms
- Voice Processing: 180ms

*Updated: 2025-11-14T14:30:45+08:00*
```

### Context Injection Strategy (Parallel RAG)

**Goal:** Always provide current context to Azure GPT for grounded responses.

**Implementation:** Pending cloud team confirmation on injection point. Options:

**Option A: Client-side system message injection**
```python
# Before calling Azure GPT API
current_context = await mcp_client.call_tool('get_current_context')

messages = [
    {
        "role": "system",
        "content": f"Current user context:\n{current_context}\n\nUse this to ground your responses."
    },
    {
        "role": "user",
        "content": user_message
    }
]

response = await openai.ChatCompletion.create(
    model="gpt-4",
    messages=messages
)
```

**Option B: MCP server middleware**
- Automatic context prepending on every request
- Transparent to client

**Action Required:** Coordinate with cloud team to determine optimal injection point.

### Optional Enhancement: Conditional Context Enrichment

**Status:** 🔮 Optional for v1 (stretch goal)

**Concept:** Augment current context with relevant history based on active application.

**Example:**
```json
{
  "activeApp": "chrome.exe",
  "url": "https://github.com/user/repo/pull/123",
  "enrichedContext": {
    "type": "github_activity",
    "recentActivity": [
      "10:30 - Reviewed PR #122 (database schema)",
      "09:15 - Commented on issue #45",
      "Yesterday - Merged PR #120"
    ]
  }
}
```

**Implementation:** Backend detects app patterns, queries Layer 0/2, returns unified context.

**Sprint Priority:** Low (nice-to-have if time permits)

---

## Tool 2: `search_recent_activity`

### Status
✅ **Production** (refactor from `query_perception_history`)

### Purpose
Fast keyword search through raw event history for the **last 24 hours only**. Use for precise, recent queries requiring exact content matches.

### Specification

```csharp
[Description(
    "🟡 Search recent raw activity by EXACT KEYWORDS for the last 24 hours. " +
    "Returns: Event-level results with content excerpts showing where keyword appears. " +
    "⏱️ Time scope: Last 1-24 hours ONLY (raw events, short-term memory). " +
    "💡 Use when: User asks 'when did I mention X today', 'find that command from this morning', " +
    "'what was I doing 3 hours ago', 'show me the API endpoint I looked at earlier'. " +
    "✅ Best for: Precise keyword matching, finding specific mentions, TODAY's activity. " +
    "❌ NOT for: Activity older than 24 hours (use find_related_sessions) or conceptual queries " +
    "(use find_related_sessions for semantic search)."
)]
[McpServerTool(
    Name = "search_recent_activity",
    Title = "Search Recent Activity",
    Idempotent = true,
    ReadOnly = true
)]
public async Task<CallToolResult> SearchRecentActivityAsync(
    [Description("Exact keyword to search for in screen content, window titles, app names, voice transcriptions")]
    string keyword,

    [Description("How many hours back to search (1-24 hours). Default: 24 hours")]
    int hours_back = 24,

    [Description("Maximum number of results to return (1-50). Default: 10")]
    int max_results = 10,

    CancellationToken cancellationToken)
{
    // Validation
    if (hours_back > 24) {
        return new CallToolResult {
            Content = new List<ContentBlock> {
                new TextContentBlock {
                    Text = "⚠️ Raw events only retained for 24 hours. For older activity, use find_related_sessions to search compressed sessions."
                }
            }
        };
    }

    // Call backend
    var queryUrl = $"query?keyword={Uri.EscapeDataString(keyword)}&hours={hours_back}&maxcount={max_results}";
    var json = await _httpClient.GetStringAsync(queryUrl, cancellationToken);

    return FormatSearchResults(json);
}
```

### Parameters

| Parameter | Type | Required | Default | Range | Description |
|-----------|------|----------|---------|-------|-------------|
| `keyword` | string | ✅ Yes | - | - | Exact keyword to search for |
| `hours_back` | int | ❌ No | 24 | 1-24 | Time window (hours) |
| `max_results` | int | ❌ No | 10 | 1-50 | Result limit |

### Backend Endpoint
`GET http://localhost:8777/query?keyword={keyword}&hours={hours}&maxcount={max_results}`

### Search Layer
**Layer 0:** Elasticsearch (full-text BM25) or SQLite (FTS5)

### Response Format
```markdown
# Perception Activity Search Results

**Total Hits:** 5
**Search Time:** 42ms

## Events (5 results)

### Event 1
**Event ID:** evt_20251114_143045_001
**Timestamp:** 2025-11-14 14:30:45
**Device ID:** laptop_001
**Application:** code.exe
**Screen Content:**
```
// Fix database migration bug
async function migrateDatabase() {
  await db.query("ALTER TABLE users ADD COLUMN status VARCHAR(20)");
}
```

**System Info:**
  - CPU: 35.2%
  - Memory: 52.1%
  - Battery: 85% (Charging)
  - Network: WiFi

---

### Event 2
...
```

### Tool Selection Indicators

**LLM should pick this tool when:**
- ✅ Query contains temporal words: "today", "this morning", "earlier", "3 hours ago"
- ✅ Query needs exact content: "find that command", "show me the API endpoint"
- ✅ Query is precise: "when did I mention 'database migration'"

**LLM should NOT pick this tool when:**
- ❌ Query is conceptual: "find sessions about database work" → use `find_related_sessions`
- ❌ Query spans multiple days: "last week" → use `find_related_sessions`
- ❌ Query is current moment: "what am I doing now" → use `get_current_context`

---

## Tool 3: `find_related_sessions`

### Status
🔨 **Week 1 Sprint** (new implementation)

### Purpose
Semantic search across compressed session summaries using natural language queries. Retrieves high-level work sessions from the past 7-30 days.

### Specification

```csharp
[Description(
    "🟢 Semantic search across PAST WORK SESSIONS using natural language (last 7-30 days). " +
    "Returns: Comprehensive session summaries including high-level description, key points, " +
    "timestamps, duration, engagement score, primary apps, and content type. " +
    "⏱️ Time scope: Last 7-30 days (long-term memory, compressed sessions). " +
    "💡 Use when: User asks 'find sessions about X topic', 'when did I work on Y project', " +
    "'show me database design work from last week', 'what meetings did I have about budgets'. " +
    "✅ Best for: Conceptual queries, finding related work, understanding past activities, " +
    "longer time ranges, exploratory search. " +
    "❌ NOT for: Real-time context (use get_current_context) or last 24 hours raw data " +
    "(use search_recent_activity for precise keyword matches in today's events)."
)]
[McpServerTool(
    Name = "find_related_sessions",
    Title = "Find Related Sessions",
    Idempotent = true,
    ReadOnly = true
)]
public async Task<CallToolResult> FindRelatedSessionsAsync(
    [Description("Natural language query describing what to search for (e.g., 'machine learning projects', 'database design work', 'meetings about budget planning')")]
    string query,

    [Description("How many days back to search (7-30 days). Default: 7 days")]
    int days_back = 7,

    [Description("Maximum number of sessions to return (1-30). Default: 10")]
    int max_results = 10,

    CancellationToken cancellationToken)
{
    // Call backend semantic search endpoint
    var queryUrl = $"semantic_search?query={Uri.EscapeDataString(query)}&days={days_back}&limit={max_results}";
    var json = await _httpClient.GetStringAsync(queryUrl, cancellationToken);

    return FormatSessionResults(json);
}
```

### Parameters

| Parameter | Type | Required | Default | Range | Description |
|-----------|------|----------|---------|-------|-------------|
| `query` | string | ✅ Yes | - | - | Natural language query |
| `days_back` | int | ❌ No | 7 | 7-30 | Time window (days) |
| `max_results` | int | ❌ No | 10 | 1-30 | Result limit |

### Backend Endpoint
`GET http://localhost:8777/semantic_search?query={query}&days={days}&limit={max_results}` **(NEW)**

### Search Layer
**Layer 2:** Qdrant (vector similarity with cosine distance)

### Response Format
```markdown
# Related Sessions

**Total Sessions:** 3
**Search Time:** 85ms

## Session 1
**Session ID:** session_20251110_100000_abc123
**Summary:** Worked on database schema design for perception engine. Implemented session detection algorithm using idle threshold and context switches. Tested with sample data and fixed engagement score calculation bug.

**Key Points:**
- Designed three-layer database architecture (Elasticsearch, compression, Qdrant)
- Implemented session boundary detection with 5-minute idle threshold
- Created engagement score formula with weighted interaction metrics
- Fixed bug in normalized_dwell_time calculation

**Timestamp:** 2025-11-10 10:00:00 - 11:45:00 (Duration: 105 minutes)
**Engagement Score:** 0.85 (Very High)
**Primary App:** code.exe
**Content Type:** CODE
**Domain:** WORK

---

## Session 2
**Session ID:** session_20251108_143000_def456
**Summary:** Database design discussion meeting with team. Reviewed architecture diagrams, debated Elasticsearch vs SQLite for Layer 0, decided on dual-support approach.

**Key Points:**
- Discussed Layer 0 storage options (Elasticsearch primary, SQLite alternative)
- Aligned on 24-hour retention for raw events
- Planned Qdrant integration for semantic search
- Action items: Benchmark SQLite FTS5 performance

**Timestamp:** 2025-11-08 14:30:00 - 15:15:00 (Duration: 45 minutes)
**Engagement Score:** 0.72 (High)
**Primary App:** zoom.exe
**Content Type:** MEETING
**Domain:** WORK

---

## Session 3
...
```

### Tool Selection Indicators

**LLM should pick this tool when:**
- ✅ Query is conceptual: "sessions about machine learning", "database work"
- ✅ Query spans multiple days: "last week", "this month", "recent work on X"
- ✅ Query needs high-level understanding: "summarize my coding sessions"
- ✅ Query is exploratory: "what have I been working on related to X"

**LLM should NOT pick this tool when:**
- ❌ Query is present moment: "what am I doing now" → use `get_current_context`
- ❌ Query needs precise content from today: "find that command from this morning" → use `search_recent_activity`
- ❌ Query needs exact text match: "find where I mentioned 'API_KEY_123'" → use `search_recent_activity`

---

## Tool Selection Decision Tree

```
┌─────────────────────────────────────────────────────────────┐
│ Tool Selection Logic for LLM                                 │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Query Type: "What am I doing now?" / "Currently..."         │
│  Time: Present moment                                        │
│         ↓                                                     │
│  🔴 get_current_context                                      │
│                                                               │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Query Type: "Find X from this morning" / "3 hours ago"      │
│  Time: Last 1-24 hours                                       │
│  Need: Exact keyword match                                   │
│         ↓                                                     │
│  🟡 search_recent_activity(keyword="X", hours_back=24)      │
│                                                               │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Query Type: "Find sessions about X" / "Last week's work"   │
│  Time: 7-30 days                                             │
│  Need: Semantic/conceptual match                             │
│         ↓                                                     │
│  🟢 find_related_sessions(query="X", days_back=7)           │
│                                                               │
└─────────────────────────────────────────────────────────────┘

Time Boundaries (Clean Separation):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  NOW       ← Tool 1 (real-time)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  0-24h     ← Tool 2 (raw events, keyword)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  [Gap: 24h-7d handled by Tool 3 if needed]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  7-30d     ← Tool 3 (sessions, semantic)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Backend API Contract

### Existing Endpoints (Production)

#### `GET /context`
Returns real-time user context.

**Response Schema:**
```json
{
  "activeApp": "chrome.exe",
  "windowTitle": "GitHub - Pull Request #123",
  "url": "https://github.com/...",
  "activeAppContent": "Pull request content...",
  "voiceTranscription": "looks good to me",
  "voiceLatency": 180.5,
  "cameraDescription": "Person at desk",
  "cameraLatency": 1700,
  "cpuUsage": 25.3,
  "memoryUsage": 51.2,
  "memoryUsedGB": 8.2,
  "totalMemoryGB": 16.0,
  "battery": "85",
  "isCharging": true,
  "networkConnected": true,
  "networkType": "WiFi",
  "RecentPeriodActiveApps": [
    {"appName": "chrome.exe", "durationSeconds": 120},
    {"appName": "code.exe", "durationSeconds": 45}
  ],
  "fusedContext": "Active: chrome.exe | Said: \"looks good to me\"",
  "timestamp": "2025-11-14T14:30:45.123+08:00",
  "contextUpdateLatency": 28.3
}
```

#### `GET /query?keyword={keyword}&hours={hours}&maxcount={max}`
Keyword search in raw events (Layer 0).

**Response Schema:**
```json
{
  "totalHits": 5,
  "searchTimeMs": 42,
  "results": [
    {
      "eventId": "evt_20251114_143045_001",
      "timestamp": 1731569445,
      "deviceId": "laptop_001",
      "appName": "code.exe",
      "windowTitle": "main.cpp - Visual Studio Code",
      "screenContent": "async function migrateDatabase() {...}",
      "systemInfo": {
        "cpuUsage": 35.2,
        "memoryUsage": 52.1,
        "batteryPercent": 85,
        "isCharging": true,
        "networkType": "WiFi"
      }
    }
  ]
}
```

### New Endpoints Required (Week 1 Sprint)

#### `GET /semantic_search?query={query}&days={days}&limit={limit}` ⚡ NEW
Semantic search in session summaries (Layer 2).

**Implementation Required:**
1. Embed incoming query using local embedding service
2. Search Qdrant collection `perception_sessions` with vector similarity
3. Filter by timestamp (days_back)
4. Return top `limit` results with payloads

**Response Schema:**
```json
{
  "totalSessions": 3,
  "searchTimeMs": 85,
  "results": [
    {
      "sessionId": "session_20251110_100000_abc123",
      "startTime": "2025-11-10T10:00:00Z",
      "endTime": "2025-11-10T11:45:00Z",
      "durationMinutes": 105,
      "summary": "Worked on database schema design for perception engine...",
      "keyPoints": [
        "Designed three-layer database architecture",
        "Implemented session detection algorithm",
        "Fixed engagement score calculation bug"
      ],
      "engagementScore": 0.85,
      "primaryApp": "code.exe",
      "contentType": "CODE",
      "domain": "WORK",
      "deviceId": "laptop_001"
    }
  ]
}
```

---

## Implementation Roadmap (Week 1 Sprint)

**Sprint Window:** Next Monday - Friday
**Goal:** Feature-complete by Friday EOD
**Team:** Backend (C++), MCP Server (C#), ML/Embeddings

### Day 1 (Monday): Backend Foundation ⚡

**Owner:** Backend team (C++)

**Morning:**
- [ ] Integrate Qdrant C++ client
  - Use libcurl for REST API calls
  - Create `QdrantClient` class with basic methods: `upsert()`, `search()`, `healthCheck()`
  - Test connection to Qdrant (assume Docker running on `localhost:6333`)

**Afternoon:**
- [ ] Add `/semantic_search` endpoint stub to `HttpServer.cpp`
  - Parse query parameters: `query`, `days`, `limit`
  - Mock response (hardcoded JSON) for MCP team to develop against
  - Validate parameter ranges

**Deliverables:**
- ✅ Qdrant client compiles and connects
- ✅ `/semantic_search` endpoint returns mock data

---

### Day 2 (Tuesday): Local Embedding Service ⚡

**Owner:** ML engineer + Backend team

**Morning:**
- [ ] Set up local embedding microservice (Python + FastAPI)
  - Install: `pip install fastapi uvicorn sentence-transformers`
  - Model: `all-MiniLM-L6-v2` (384 dimensions, 80MB)
  - Simple API:
    ```python
    from fastapi import FastAPI
    from sentence_transformers import SentenceTransformer

    app = FastAPI()
    model = SentenceTransformer('all-MiniLM-L6-v2')

    @app.post("/embed")
    async def embed_text(text: str):
        embedding = model.encode(text).tolist()
        return {"embedding": embedding, "dimensions": 384}
    ```
  - Run: `uvicorn embedding_service:app --host 0.0.0.0 --port 5000`

**Afternoon:**
- [ ] C++ integration with embedding service
  - Use libcurl to call `POST http://localhost:5000/embed`
  - Parse JSON response, extract embedding vector
  - Test end-to-end: C++ string → Python embedding → C++ receives float array

**Deliverables:**
- ✅ Embedding service running as Windows Service (or manual start for v1)
- ✅ C++ can generate embeddings for arbitrary text

---

### Day 3 (Wednesday): Database Integration ⚡

**Owner:** Backend team

**Morning:**
- [ ] Layer 1 → Layer 2 pipeline (session → Qdrant)
  - Query compressed sessions from Elasticsearch (Layer 0)
  - For each session: Generate embedding from `compressed_session_summary`
  - Bulk upsert to Qdrant collection `perception_sessions`
  - Test with 10-20 sample sessions

**Afternoon:**
- [ ] Implement real `/semantic_search` logic
  - Embed incoming query text
  - Call Qdrant search API with vector, filters (timestamp), limit
  - Parse Qdrant response, extract payloads
  - Format as JSON per API contract

**Deliverables:**
- ✅ Sample sessions indexed in Qdrant
- ✅ `/semantic_search` returns real results (not mock)

---

### Day 4 (Thursday): MCP Server Enhancement ⚡

**Owner:** MCP/C# team

**Morning:**
- [ ] Production-ize existing tools
  - Rename `query_perception_history` → `search_recent_activity`
  - Update tool descriptions with time-scope indicators (⏱️, 💡, ❌)
  - Add validation: `hours_back <= 24`
  - Improve error messages with actionable suggestions
  - Add retry logic (3 retries, exponential backoff)

**Afternoon:**
- [ ] Implement `find_related_sessions` tool
  - Add new method to `PerceptionTools` class
  - Call `/semantic_search` endpoint
  - Format results as markdown (session summaries, key points, metadata)
  - Handle errors gracefully (Qdrant down → clear error message)

**Deliverables:**
- ✅ All 3 tools functional in MCP server
- ✅ Tool descriptions optimized for LLM selection

---

### Day 5 (Friday): Integration & Feature Complete 🎯

**Owner:** Full team

**Morning:**
- [ ] End-to-end integration testing
  - Start full stack: Perception Engine → Elasticsearch → Qdrant → Embedding Service → MCP Server
  - Test realistic queries:
    - "What am I doing now?" → Tool 1
    - "Find the API endpoint I looked at this morning" → Tool 2
    - "Show me database design sessions from last week" → Tool 3
  - Verify results are relevant and well-formatted

**Afternoon:**
- [ ] Error handling & edge cases
  - Test: Qdrant down → MCP server returns helpful error
  - Test: Embedding service down → Graceful failure
  - Test: No sessions found → User-friendly message
  - Test: Malformed queries → Validation errors
  - Load testing: 20 concurrent requests

**Deliverables:**
- ✅ **Feature complete** - All 3 tools working end-to-end
- ✅ Error handling verified
- ✅ Ready for QA phase

---

### Post-Sprint: QA & Polish (Stable Pace)

**Following week:**
- Edge case testing
- Performance tuning (caching, optimization)
- Documentation updates
- Deployment automation
- User acceptance testing

---

## Error Handling Strategy

### Retry Logic

Use exponential backoff for transient failures:

```csharp
using Polly;
using Polly.Retry;

private readonly AsyncRetryPolicy<HttpResponseMessage> _retryPolicy = Policy
    .HandleResult<HttpResponseMessage>(r => !r.IsSuccessStatusCode)
    .Or<HttpRequestException>()
    .WaitAndRetryAsync(
        retryCount: 3,
        sleepDurationProvider: attempt => TimeSpan.FromSeconds(Math.Pow(2, attempt)),
        onRetry: (outcome, timespan, retryCount, context) =>
        {
            _logger.LogWarning("Retry {RetryCount} after {Delay}s due to {Reason}",
                retryCount, timespan.TotalSeconds, outcome.Exception?.Message);
        }
    );
```

### Service Availability Checks

```csharp
public async Task<CallToolResult> FindRelatedSessionsAsync(...)
{
    // Health check before expensive operation
    if (!await IsPerceptionEngineHealthyAsync())
    {
        return new CallToolResult
        {
            Content = new List<ContentBlock>
            {
                new TextContentBlock
                {
                    Text = "❌ Perception Engine is not responding. Please verify:\n" +
                           "1. PerceptionEngine.exe is running\n" +
                           "2. Port 8777 is accessible\n" +
                           "3. Run: cd windows_code/build/bin/Release && ./PerceptionEngine.exe --console"
                }
            },
            IsError = true
        };
    }

    // Proceed with query...
}
```

### Graceful Degradation

```csharp
public async Task<CallToolResult> FindRelatedSessionsAsync(...)
{
    try
    {
        // Try semantic search (Layer 2)
        return await SemanticSearchAsync(query, days_back, max_results);
    }
    catch (QdrantUnavailableException ex)
    {
        _logger.LogWarning("Qdrant unavailable, falling back to keyword search: {Error}", ex.Message);

        // Fallback to keyword search (Layer 0)
        return await KeywordSearchFallbackAsync(query, days_back * 24, max_results);
    }
}
```

### User-Friendly Error Messages

❌ **Bad:**
```
Error: Connection refused to 127.0.0.1:6333
```

✅ **Good:**
```
⚠️ Semantic search service (Qdrant) is unavailable.
This feature requires the vector database to be running.

Troubleshooting:
1. Start Qdrant: docker compose up -d qdrant
2. Verify: curl http://localhost:6333/collections
3. Check logs: docker logs qdrant

Falling back to keyword search for this query.
```

---

## Testing Plan

### Unit Tests

**MCP Server (C#):**
```csharp
[Fact]
public async Task SearchRecentActivity_ValidatesHoursBack()
{
    // Test parameter validation
    var result = await _tools.SearchRecentActivityAsync(
        keyword: "test",
        hours_back: 72,  // Invalid: > 24
        max_results: 10,
        CancellationToken.None
    );

    Assert.True(result.IsError);
    Assert.Contains("24 hours", result.Content[0].Text);
}

[Fact]
public async Task FindRelatedSessions_ReturnsFormattedResults()
{
    // Mock backend response
    _mockHttpClient.SetupResponse("/semantic_search", sampleSessionJson);

    var result = await _tools.FindRelatedSessionsAsync(
        query: "database design",
        days_back: 7,
        max_results: 10,
        CancellationToken.None
    );

    Assert.False(result.IsError);
    Assert.Contains("Session 1", result.Content[0].Text);
    Assert.Contains("Engagement Score:", result.Content[0].Text);
}
```

### Integration Tests

**End-to-End Scenarios:**

| Scenario | Input | Expected Tool | Expected Output |
|----------|-------|---------------|-----------------|
| Real-time query | "What am I doing now?" | `get_current_context` | Current app + voice + camera |
| Recent keyword | "Find 'API_KEY' from this morning" | `search_recent_activity` | Event with exact match |
| Semantic query | "Find database design sessions" | `find_related_sessions` | Relevant session summaries |
| Temporal precision | "What was I doing 5 hours ago?" | `search_recent_activity` | Events from ~5h ago |
| Conceptual search | "Show me coding work from last week" | `find_related_sessions` | CODE sessions, 7d back |

**Performance Tests:**
- Latency: All tools < 500ms p99
- Throughput: 10 concurrent requests without errors
- Large results: 50 events returned without truncation

---

## Local Embedding Service Specification

### Deployment: Windows Service

**Installation:**
```powershell
# 1. Install Python dependencies
pip install fastapi uvicorn sentence-transformers

# 2. Create Windows Service (using NSSM)
nssm install PerceptionEmbedding "C:\Python39\python.exe"
nssm set PerceptionEmbedding AppDirectory "C:\PerceptionEngine\embedding_service"
nssm set PerceptionEmbedding AppParameters "-m uvicorn embedding_service:app --host 0.0.0.0 --port 5000"
nssm set PerceptionEmbedding DisplayName "Perception Engine Embedding Service"
nssm set PerceptionEmbedding Start SERVICE_AUTO_START

# 3. Start service
nssm start PerceptionEmbedding
```

**Service Code (embedding_service.py):**
```python
from fastapi import FastAPI, HTTPException
from sentence_transformers import SentenceTransformer
from pydantic import BaseModel
import logging

app = FastAPI(title="Perception Embedding Service", version="1.0.0")
model = SentenceTransformer('all-MiniLM-L6-v2')

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class EmbedRequest(BaseModel):
    text: str

class EmbedResponse(BaseModel):
    embedding: list[float]
    dimensions: int
    model: str

@app.get("/health")
async def health_check():
    return {"status": "healthy", "model": "all-MiniLM-L6-v2"}

@app.post("/embed", response_model=EmbedResponse)
async def embed_text(request: EmbedRequest):
    try:
        logger.info(f"Embedding text (length: {len(request.text)} chars)")
        embedding = model.encode(request.text).tolist()
        return {
            "embedding": embedding,
            "dimensions": len(embedding),
            "model": "all-MiniLM-L6-v2"
        }
    except Exception as e:
        logger.error(f"Embedding failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=5000)
```

**Configuration:**
```json
{
  "embeddingService": {
    "url": "http://localhost:5000",
    "model": "all-MiniLM-L6-v2",
    "dimensions": 384,
    "timeout_ms": 5000
  }
}
```

---

## v2 Roadmap (Post-Sprint)

**Not for Week 1 - Future Enhancements:**

### Advanced RAG Patterns
- **Speculative RAG**: Pre-fetch results while user is typing (requires client modification)
- **Reranking pipeline**: Cross-encoder models for result re-scoring
- **Hybrid search**: Merge keyword (Layer 0) + semantic (Layer 2) results via Reciprocal Rank Fusion

### Context Enrichment
- **Advanced app detection**: ML-based relevance prediction instead of hardcoded rules
- **Smart context injection**: Automatically include top-3 relevant sessions based on current activity
- **Multi-device correlation**: "Show me what I was working on when I last used this app on my laptop"

### Performance Optimizations
- **Result caching**: Redis cache for frequent queries (30s TTL)
- **Embedding caching**: Pre-compute embeddings for common query patterns
- **Batch embedding**: Generate embeddings for multiple sessions in parallel

### Privacy & Security
- **PII filtering**: Automatic redaction of sensitive data (handled in database layer)
- **User-controlled exclusions**: Blacklist certain apps/domains from indexing
- **Local-only mode**: Disable cloud services, full on-device processing

### Analytics & Insights
- **Productivity scoring**: Aggregate engagement metrics over time
- **Work pattern detection**: Identify focus hours, meeting load, context switches
- **Project inference**: Auto-group sessions into projects using graph clustering

---

## Appendix: Configuration Files

### MCP Server Config (`appsettings.json`)

```json
{
  "PerceptionEngine": {
    "BaseUrl": "http://localhost:8777/",
    "Timeout": 5000,
    "RetryAttempts": 3,
    "RetryDelayMs": 1000
  },
  "Logging": {
    "LogLevel": {
      "Default": "Information",
      "Microsoft": "Warning",
      "System": "Warning"
    }
  },
  "Serilog": {
    "MinimumLevel": "Information",
    "WriteTo": [
      {
        "Name": "File",
        "Args": {
          "path": "logs/mcp-server-.txt",
          "rollingInterval": "Day",
          "retainedFileCountLimit": 7
        }
      }
    ]
  }
}
```

### Claude Desktop Config (Reference for Azure GPT Integration)

**Note:** Since you're using Azure GPT, this section is for reference only. Coordinate with cloud team on how to configure MCP server connection in your Azure GPT client.

**Typical MCP configuration pattern:**
```json
{
  "mcpServers": {
    "perception-engine": {
      "command": "C:\\path\\to\\PerceptionMcpBridge.exe",
      "args": [],
      "env": {
        "PERCEPTION_ENGINE_URL": "http://localhost:8777"
      }
    }
  }
}
```

---

## Document Metadata

**Version:** 1.0.0
**Created:** 2025-11-14
**Authors:** Perception Engine Team + Claude
**Target Audience:** Engineering team, sprint participants
**Status:** Production specification for Week 1 implementation

**Related Documentation:**
- [DATABASE_DESIGN.md](../../DATABASE_DESIGN.md) - Three-layer database architecture
- [CLAUDE.md](../../CLAUDE.md) - Perception Engine overview
- [Program.cs](./Program.cs) - Current MCP server implementation

**Changelog:**
- v1.0.0 (2025-11-14): Initial production specification
  - 3 tools with clean time-scale separation
  - Local embedding service (Windows Service)
  - Week 1 sprint roadmap
  - Azure GPT integration notes
  - Error handling patterns
  - Optional context enrichment
