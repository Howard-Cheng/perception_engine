# VectorDB C++ Library API Documentation

Complete API reference for the VectorDB C++ library, including QdrantClient, EmbeddingModel, and VectorStore interfaces.

## Table of Contents

- [Overview](#overview)
- [QdrantClient](#qdrantclient)
- [EmbeddingModel](#embeddingmodel)
- [VectorStore](#vectorstore)
- [Data Types](#data-types)
- [Examples](#examples)

---

## Overview

The VectorDB C++ library provides three main components:

1. **QdrantClient**: Low-level interface for Qdrant vector database operations
2. **EmbeddingModel**: ONNX-based embedding model for text vectorization
3. **VectorStore**: High-level interface combining embeddings and vector storage

All classes are in the `vectordb` namespace.

---

## QdrantClient

Low-level interface for Qdrant vector database operations. Connects to a remote Qdrant server via HTTP.

### Configuration

#### `QdrantClient::Config`

Connection configuration structure.

**Fields:**
- `std::string url` - Qdrant server URL (default: `"http://localhost:6333"`)
- `std::optional<std::string> apiKey` - Optional API key for authentication
- `float timeout` - Request timeout in seconds (default: 30.0f)

**Static Methods:**

```cpp
// Create config for remote Qdrant server
static Config remote(const std::string& url, 
                     const std::optional<std::string>& apiKey = {});
```

**Example:**
```cpp
// Remote mode (connect to Qdrant server)
auto remoteConfig = QdrantClient::Config::remote("http://localhost:6333");
```

### Constructor

```cpp
explicit QdrantClient(const Config& config);
```

Creates a Qdrant client with the specified configuration.

**Parameters:**
- `config` - Connection configuration

**Example:**
```cpp
auto config = QdrantClient::Config::remote("http://localhost:6333");
QdrantClient client(config);
```

### Collection Management

#### `createCollection`

```cpp
bool createCollection(
    const std::string& collectionName,
    size_t vectorSize,
    DistanceMetric distance = DistanceMetric::COSINE,
    bool recreate = false
);
```

Creates a new collection.

**Parameters:**
- `collectionName` - Name of the collection
- `vectorSize` - Dimension of vectors in this collection
- `distance` - Distance metric (default: `COSINE`)
- `recreate` - If true, delete existing collection before creating

**Returns:** `true` if collection was created successfully

**Example:**
```cpp
client.createCollection("my_collection", 384, DistanceMetric::COSINE);
```

#### `deleteCollection`

```cpp
bool deleteCollection(const std::string& collectionName);
```

Deletes a collection.

**Parameters:**
- `collectionName` - Name of the collection to delete

**Returns:** `true` if collection was deleted successfully

#### `collectionExists`

```cpp
bool collectionExists(const std::string& collectionName);
```

Checks if a collection exists.

**Parameters:**
- `collectionName` - Name of the collection to check

**Returns:** `true` if collection exists

#### `listCollections`

```cpp
std::vector<std::string> listCollections();
```

Lists all collection names.

**Returns:** Vector of collection names

#### `getCollectionInfo`

```cpp
std::optional<CollectionInfo> getCollectionInfo(const std::string& collectionName);
```

Gets information about a collection.

**Parameters:**
- `collectionName` - Name of the collection

**Returns:** Collection information, or `nullopt` if not found

### Vector Operations

#### `upsert` (multiple points)

```cpp
bool upsert(
    const std::string& collectionName,
    const std::vector<VectorPoint>& points
);
```

Inserts or updates multiple points in a collection.

**Parameters:**
- `collectionName` - Name of the collection
- `points` - Vector of points to upsert

**Returns:** `true` if operation succeeded

**Example:**
```cpp
std::vector<VectorPoint> points;
points.push_back(VectorPoint("point1", {0.1f, 0.2f, 0.3f}, {{"key", "value"}}));
client.upsert("my_collection", points);
```

#### `upsert` (single point)

```cpp
bool upsert(
    const std::string& collectionName,
    const VectorPoint& point
);
```

Inserts or updates a single point in a collection.

**Parameters:**
- `collectionName` - Name of the collection
- `point` - Point to upsert

**Returns:** `true` if operation succeeded

#### `search`

```cpp
std::vector<SearchResult> search(
    const std::string& collectionName,
    const std::vector<float>& queryVector,
    size_t limit = 10,
    std::optional<float> scoreThreshold = {},
    const std::optional<Filter>& filter = {},
    bool withPayload = true,
    bool withVectors = false
);
```

Searches for similar vectors in a collection.

**Parameters:**
- `collectionName` - Name of the collection
- `queryVector` - Query vector to search for
- `limit` - Maximum number of results (default: 10)
- `scoreThreshold` - Minimum similarity score threshold (optional)
- `filter` - Optional metadata filter
- `withPayload` - Whether to include payload in results (default: true)
- `withVectors` - Whether to include vectors in results (default: false)

**Returns:** Vector of search results

**Example:**
```cpp
std::vector<float> queryVector = {0.1f, 0.2f, 0.3f};
auto results = client.search("my_collection", queryVector, 10);
for (const auto& result : results) {
    std::cout << "Score: " << result.score << std::endl;
}
```

#### `deletePoints`

```cpp
bool deletePoints(
    const std::string& collectionName,
    const std::vector<PointId>& pointIds
);
```

Deletes points from a collection by IDs.

**Parameters:**
- `collectionName` - Name of the collection
- `pointIds` - Vector of point IDs to delete

**Returns:** `true` if operation succeeded

#### `deletePointsByFilter`

```cpp
bool deletePointsByFilter(
    const std::string& collectionName,
    const Filter& filter
);
```

Deletes points from a collection by metadata filter.

**Parameters:**
- `collectionName` - Name of the collection
- `filter` - Metadata filter to match points for deletion

**Returns:** `true` if operation succeeded

#### `retrieve`

```cpp
std::vector<VectorPoint> retrieve(
    const std::string& collectionName,
    const std::vector<PointId>& pointIds,
    bool withPayload = true,
    bool withVectors = false
);
```

Retrieves points by IDs.

**Parameters:**
- `collectionName` - Name of the collection
- `pointIds` - Vector of point IDs to retrieve
- `withPayload` - Whether to include payload (default: true)
- `withVectors` - Whether to include vectors (default: false)

**Returns:** Vector of retrieved points

### Utility Methods

#### `testConnection`

```cpp
bool testConnection();
```

Tests connection to Qdrant server.

**Returns:** `true` if connection is successful

#### `getLastError`

```cpp
std::string getLastError() const;
```

Gets the last error message (if any).

**Returns:** Error message string

---

## EmbeddingModel

ONNX-based embedding model for text vectorization. Supports models like E5-small exported to ONNX format.

### Constructor

```cpp
explicit EmbeddingModel(
    const std::string& modelPath,
    bool normalize = true
);
```

Creates an embedding model instance.

**Parameters:**
- `modelPath` - Path to the ONNX model file
- `normalize` - Whether to normalize embeddings (L2 normalization, default: true)

**Example:**
```cpp
EmbeddingModel model("models/e5-small/model.onnx");
```

### Encoding Methods

#### `encode`

```cpp
std::vector<float> encode(const std::string& text);
```

Encodes a single text into an embedding vector.

**Parameters:**
- `text` - Input text to encode

**Returns:** Vector of floats representing the embedding

**Throws:** `std::runtime_error` if encoding fails

**Note:** For E5 models, text should include prefix:
- `"query: "` for search queries
- `"passage: "` for documents to be searched

**Example:**
```cpp
auto embedding = model.encode("passage: Hello world");
// Returns 384-dimensional vector for E5-small
```

#### `encodeBatch`

```cpp
std::vector<std::vector<float>> encodeBatch(const std::vector<std::string>& texts);
```

Encodes a batch of texts into embedding vectors.

**Parameters:**
- `texts` - Vector of input texts to encode

**Returns:** Vector of embedding vectors, each as a vector of floats

**Throws:** `std::runtime_error` if encoding fails

**Example:**
```cpp
std::vector<std::string> texts = {
    "passage: First document",
    "passage: Second document"
};
auto embeddings = model.encodeBatch(texts);
```

### Information Methods

#### `getDimension`

```cpp
size_t getDimension() const;
```

Gets the dimension of embeddings produced by this model.

**Returns:** Embedding dimension, or 0 if model is not loaded

**Example:**
```cpp
size_t dim = model.getDimension(); // 384 for E5-small
```

#### `isLoaded`

```cpp
bool isLoaded() const;
```

Checks if the model is loaded and ready to use.

**Returns:** `true` if model is loaded

#### `getLastError`

```cpp
std::string getLastError() const;
```

Gets the last error message (if any).

**Returns:** Error message string

#### `reload`

```cpp
bool reload();
```

Reloads the model (useful if model file was updated).

**Returns:** `true` if reload was successful

---

## VectorStore

High-level interface combining QdrantClient and EmbeddingModel. Automatically handles embedding generation and vector operations.

### Constructor

```cpp
VectorStore(
    const std::string& collectionName,
    const std::string& embeddingModelPath,
    const QdrantClient::Config& qdrantConfig
);
```

Creates a VectorStore instance.

**Parameters:**
- `collectionName` - Name of the Qdrant collection
- `embeddingModelPath` - Path to the ONNX embedding model file
- `qdrantConfig` - Qdrant client configuration (required, must use remote mode)

**Example:**
```cpp
auto config = QdrantClient::Config::remote("http://localhost:6333");
VectorStore store("my_collection", "models/e5-small/model.onnx", config);
```

### Initialization

#### `initialize`

```cpp
bool initialize();
```

Initializes the vector store (loads embedding model, connects to Qdrant, creates collection if needed).

**Returns:** `true` if initialization was successful

**Example:**
```cpp
if (!store.initialize()) {
    std::cerr << "Failed to initialize VectorStore" << std::endl;
    return;
}
```

### Storage Methods

#### `storeText`

```cpp
bool storeText(
    const std::string& text,
    const Payload& payload = {},
    std::optional<PointId> pointId = {}
);
```

Stores a text with metadata in the collection. Automatically generates embedding and adds "passage: " prefix if needed.

**Parameters:**
- `text` - Text to store (will be embedded automatically)
- `payload` - Metadata payload (optional)
- `pointId` - Optional point ID (auto-generated if not provided)

**Returns:** `true` if storage was successful

**Example:**
```cpp
Payload payload;
payload["type"] = std::string("document");
payload["timestamp"] = static_cast<int64_t>(std::time(nullptr));
store.storeText("Working on GitHub PR #123", payload);
```

#### `storeTexts`

```cpp
bool storeTexts(
    const std::vector<std::string>& texts,
    const std::vector<Payload>& payloads = {},
    const std::vector<PointId>& pointIds = {}
);
```

Stores multiple texts with metadata in the collection.

**Parameters:**
- `texts` - Vector of texts to store
- `payloads` - Vector of metadata payloads (one per text, optional)
- `pointIds` - Optional vector of point IDs (auto-generated if not provided)

**Returns:** `true` if storage was successful

**Example:**
```cpp
std::vector<std::string> texts = {
    "Working on GitHub PR #123",
    "Reviewing code changes"
};
std::vector<Payload> payloads;
for (size_t i = 0; i < texts.size(); ++i) {
    Payload p;
    p["index"] = static_cast<int64_t>(i);
    payloads.push_back(p);
}
store.storeTexts(texts, payloads);
```

### Search Methods

#### `search`

```cpp
std::vector<SearchResult> search(
    const std::string& queryText,
    size_t limit = 10,
    std::optional<float> scoreThreshold = {},
    const std::optional<Filter>& filter = {}
);
```

Searches for similar texts in the collection. Automatically encodes query text and adds "query: " prefix if needed.

**Parameters:**
- `queryText` - Query text to search for
- `limit` - Maximum number of results (default: 10)
- `scoreThreshold` - Optional minimum similarity score
- `filter` - Optional metadata filter

**Returns:** Vector of search results

**Example:**
```cpp
auto results = store.search("GitHub pull request", 10);
for (const auto& result : results) {
    std::cout << "Score: " << result.score << std::endl;
    if (result.payload.has_value()) {
        // Access payload data
    }
}
```

#### `searchByVector`

```cpp
std::vector<SearchResult> searchByVector(
    const std::vector<float>& queryVector,
    size_t limit = 10,
    std::optional<float> scoreThreshold = {},
    const std::optional<Filter>& filter = {}
);
```

Searches using a raw query vector (without text encoding).

**Parameters:**
- `queryVector` - Query vector
- `limit` - Maximum number of results (default: 10)
- `scoreThreshold` - Optional minimum similarity score
- `filter` - Optional metadata filter

**Returns:** Vector of search results

### Accessor Methods

#### `getClient`

```cpp
QdrantClient& getClient();
```

Gets the QdrantClient instance (for advanced operations).

**Returns:** Reference to the QdrantClient

#### `getEmbeddingModel`

```cpp
std::optional<std::reference_wrapper<EmbeddingModel>> getEmbeddingModel();
```

Gets the EmbeddingModel instance (for advanced operations).

**Returns:** Reference to the EmbeddingModel, or `nullopt` if not loaded

#### `getEmbeddingDimension`

```cpp
size_t getEmbeddingDimension() const;
```

Gets the embedding dimension.

**Returns:** Embedding dimension, or 0 if model is not loaded

#### `getCollectionName`

```cpp
const std::string& getCollectionName() const;
```

Gets the collection name.

**Returns:** Collection name

---

## Data Types

### `DistanceMetric`

Enumeration for distance metrics:

```cpp
enum class DistanceMetric {
    COSINE,    // Cosine similarity
    EUCLID,    // Euclidean distance
    DOT        // Dot product
};
```

### `PointId`

Point identifier type (can be string or integer):

```cpp
using PointId = std::variant<std::string, uint64_t>;
```

**Example:**
```cpp
PointId id1 = std::string("point-1");
PointId id2 = uint64_t(123);
```

### `PayloadValue`

Metadata value type:

```cpp
using PayloadValue = std::variant<
    std::string,
    int64_t,
    double,
    bool,
    std::vector<std::string>,
    std::vector<int64_t>,
    std::vector<double>
>;
```

### `Payload`

Metadata payload (map of string keys to values):

```cpp
using Payload = std::map<std::string, PayloadValue>;
```

**Example:**
```cpp
Payload payload;
payload["title"] = std::string("Document Title");
payload["score"] = 95.5;
payload["tags"] = std::vector<std::string>{"tag1", "tag2"};
```

### `VectorPoint`

Vector point structure:

```cpp
struct VectorPoint {
    PointId id;
    std::vector<float> vector;
    Payload payload;
};
```

### `SearchResult`

Search result structure:

```cpp
struct SearchResult {
    PointId id;
    float score;                    // Similarity score
    std::optional<Payload> payload; // Optional payload
    std::optional<std::vector<float>> vector; // Optional vector
};
```

### `CollectionInfo`

Collection information structure:

```cpp
struct CollectionInfo {
    std::string name;
    uint64_t pointsCount;
    uint64_t vectorsCount;
    size_t vectorSize;
    DistanceMetric distance;
    std::string status;
};
```

### `FilterCondition`

Filter condition for metadata filtering:

```cpp
struct FilterCondition {
    FilterConditionType type;
    std::string key;
    
    // For MATCH
    std::optional<PayloadValue> matchValue;
    
    // For MATCH_TEXT
    std::optional<std::string> matchText;
    
    // For RANGE
    std::optional<double> rangeGt;
    std::optional<double> rangeGte;
    std::optional<double> rangeLt;
    std::optional<double> rangeLte;
};
```

**Static Factory Methods:**

```cpp
// Exact match
static FilterCondition createMatch(const std::string& key, const PayloadValue& value);

// Text match (substring)
static FilterCondition createMatchText(const std::string& key, const std::string& text);

// Range match
static FilterCondition createRange(
    const std::string& key,
    std::optional<double> gt = {},
    std::optional<double> gte = {},
    std::optional<double> lt = {},
    std::optional<double> lte = {}
);
```

### `Filter`

Filter structure for complex queries:

```cpp
struct Filter {
    std::vector<FilterCondition> must;      // All must be satisfied (AND)
    std::vector<FilterCondition> mustNot;   // All must not be satisfied (NOT)
    std::vector<FilterCondition> should;    // At least one must be satisfied (OR)
};
```

**Static Factory Methods:**

```cpp
// Create filter with must conditions
static Filter createMust(const std::vector<FilterCondition>& conditions);

// Create filter with must_not conditions
static Filter createMustNot(const std::vector<FilterCondition>& conditions);

// Create filter with should conditions
static Filter createShould(const std::vector<FilterCondition>& conditions);
```

---

## Examples

### Basic Usage

```cpp
#include "VectorStore.h"
#include <iostream>

int main() {
    // Initialize VectorStore (connect to Qdrant server)
    auto config = QdrantClient::Config::remote("http://localhost:6333");
    VectorStore store("my_collection", "models/e5-small/model.onnx", config);
    
    if (!store.initialize()) {
        std::cerr << "Failed to initialize" << std::endl;
        return 1;
    }
    
    // Store text
    Payload payload;
    payload["type"] = std::string("document");
    store.storeText("Working on GitHub PR #123", payload);
    
    // Search
    auto results = store.search("GitHub pull request", 5);
    for (const auto& result : results) {
        std::cout << "Score: " << result.score << std::endl;
    }
    
    return 0;
}
```

### Using Filters

```cpp
// Search with metadata filter
auto filter = Filter::createMust({
    FilterCondition::createMatch("type", std::string("document")),
    FilterCondition::createRange("score", {}, 80.0, {}, {}) // score >= 80
});

auto results = store.search("query text", 10, {}, filter);
```

### Advanced QdrantClient Usage

```cpp
#include "QdrantClient.h"

// Create collection (connect to Qdrant server)
auto config = QdrantClient::Config::remote("http://localhost:6333");
QdrantClient client(config);

client.createCollection("my_collection", 384, DistanceMetric::COSINE);

// Upsert vectors
std::vector<VectorPoint> points;
points.push_back(VectorPoint(
    "point1",
    {0.1f, 0.2f, 0.3f, /* ... */},
    {{"key", std::string("value")}}
));
client.upsert("my_collection", points);

// Search
std::vector<float> queryVector = {0.1f, 0.2f, 0.3f, /* ... */};
auto results = client.search("my_collection", queryVector, 10);
```

### Direct EmbeddingModel Usage

```cpp
#include "EmbeddingModel.h"

EmbeddingModel model("models/e5-small/model.onnx");

// Encode text (E5 requires prefix)
auto embedding = model.encode("passage: Hello world");

// Batch encoding
std::vector<std::string> texts = {
    "passage: First document",
    "passage: Second document"
};
auto embeddings = model.encodeBatch(texts);
```

---

## Notes

- **E5 Model Prefixes**: E5-small model requires text prefixes (`"query: "` for queries, `"passage: "` for documents). VectorStore automatically adds these prefixes.
- **Qdrant Server**: Requires a running Qdrant server. Use Docker to run Qdrant: `docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant`
- **Web Dashboard**: Access the Qdrant web UI at `http://localhost:6333/dashboard` to view collections and data.
- **Thread Safety**: Classes are not thread-safe. Use separate instances or external synchronization for multi-threaded access.
- **Error Handling**: Check return values and use `getLastError()` methods to diagnose failures.

