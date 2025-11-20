# VectorDB C++ 库 API 文档

VectorDB C++ 库的完整 API 参考，包括 QdrantClient、EmbeddingModel 和 VectorStore 接口。

## 目录

- [概述](#概述)
- [QdrantClient](#qdrantclient)
- [EmbeddingModel](#embeddingmodel)
- [VectorStore](#vectorstore)
- [数据类型](#数据类型)
- [示例](#示例)

---

## 概述

VectorDB C++ 库提供三个主要组件：

1. **QdrantClient**: Qdrant 向量数据库操作的低级接口
2. **EmbeddingModel**: 基于 ONNX 的文本向量化嵌入模型
3. **VectorStore**: 结合嵌入和向量存储的高级接口

所有类都在 `vectordb` 命名空间中。

---

## QdrantClient

Qdrant 向量数据库操作的低级接口。通过 HTTP 连接到远程 Qdrant 服务器。

### 配置

#### `QdrantClient::Config`

连接配置结构。

**字段：**
- `std::string url` - Qdrant 服务器 URL（默认：`"http://localhost:6333"`）
- `std::optional<std::string> apiKey` - 可选的 API 密钥用于身份验证
- `float timeout` - 请求超时时间（秒，默认：30.0f）

**静态方法：**

```cpp
// 为远程 Qdrant 服务器创建配置
static Config remote(const std::string& url, 
                     const std::optional<std::string>& apiKey = {});
```

**示例：**
```cpp
// 远程模式（连接到 Qdrant 服务器）
auto remoteConfig = QdrantClient::Config::remote("http://localhost:6333");
```

### 构造函数

```cpp
explicit QdrantClient(const Config& config);
```

使用指定配置创建 Qdrant 客户端。

**参数：**
- `config` - 连接配置

**示例：**
```cpp
auto config = QdrantClient::Config::remote("http://localhost:6333");
QdrantClient client(config);
```

### 集合管理

#### `createCollection`

```cpp
bool createCollection(
    const std::string& collectionName,
    size_t vectorSize,
    DistanceMetric distance = DistanceMetric::COSINE,
    bool recreate = false
);
```

创建新集合。

**参数：**
- `collectionName` - 集合名称
- `vectorSize` - 此集合中向量的维度
- `distance` - 距离度量（默认：`COSINE`）
- `recreate` - 如果为 true，在创建前删除现有集合

**返回：** 如果集合创建成功则返回 `true`

**示例：**
```cpp
client.createCollection("my_collection", 384, DistanceMetric::COSINE);
```

#### `deleteCollection`

```cpp
bool deleteCollection(const std::string& collectionName);
```

删除集合。

**参数：**
- `collectionName` - 要删除的集合名称

**返回：** 如果集合删除成功则返回 `true`

#### `collectionExists`

```cpp
bool collectionExists(const std::string& collectionName);
```

检查集合是否存在。

**参数：**
- `collectionName` - 要检查的集合名称

**返回：** 如果集合存在则返回 `true`

#### `listCollections`

```cpp
std::vector<std::string> listCollections();
```

列出所有集合名称。

**返回：** 集合名称的向量

#### `getCollectionInfo`

```cpp
std::optional<CollectionInfo> getCollectionInfo(const std::string& collectionName);
```

获取集合信息。

**参数：**
- `collectionName` - 集合名称

**返回：** 集合信息，如果未找到则返回 `nullopt`

### 向量操作

#### `upsert`（多个点）

```cpp
bool upsert(
    const std::string& collectionName,
    const std::vector<VectorPoint>& points
);
```

在集合中插入或更新多个点。

**参数：**
- `collectionName` - 集合名称
- `points` - 要插入或更新的点的向量

**返回：** 如果操作成功则返回 `true`

**示例：**
```cpp
std::vector<VectorPoint> points;
points.push_back(VectorPoint("point1", {0.1f, 0.2f, 0.3f}, {{"key", "value"}}));
client.upsert("my_collection", points);
```

#### `upsert`（单个点）

```cpp
bool upsert(
    const std::string& collectionName,
    const VectorPoint& point
);
```

在集合中插入或更新单个点。

**参数：**
- `collectionName` - 集合名称
- `point` - 要插入或更新的点

**返回：** 如果操作成功则返回 `true`

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

在集合中搜索相似向量。

**参数：**
- `collectionName` - 集合名称
- `queryVector` - 要搜索的查询向量
- `limit` - 最大结果数（默认：10）
- `scoreThreshold` - 最小相似度分数阈值（可选）
- `filter` - 可选的元数据过滤器
- `withPayload` - 是否在结果中包含负载（默认：true）
- `withVectors` - 是否在结果中包含向量（默认：false）

**返回：** 搜索结果向量

**示例：**
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

通过 ID 从集合中删除点。

**参数：**
- `collectionName` - 集合名称
- `pointIds` - 要删除的点 ID 向量

**返回：** 如果操作成功则返回 `true`

#### `deletePointsByFilter`

```cpp
bool deletePointsByFilter(
    const std::string& collectionName,
    const Filter& filter
);
```

通过元数据过滤器从集合中删除点。

**参数：**
- `collectionName` - 集合名称
- `filter` - 用于匹配要删除的点的元数据过滤器

**返回：** 如果操作成功则返回 `true`

#### `retrieve`

```cpp
std::vector<VectorPoint> retrieve(
    const std::string& collectionName,
    const std::vector<PointId>& pointIds,
    bool withPayload = true,
    bool withVectors = false
);
```

通过 ID 检索点。

**参数：**
- `collectionName` - 集合名称
- `pointIds` - 要检索的点 ID 向量
- `withPayload` - 是否包含负载（默认：true）
- `withVectors` - 是否包含向量（默认：false）

**返回：** 检索到的点的向量

### 工具方法

#### `testConnection`

```cpp
bool testConnection();
```

测试与 Qdrant 服务器的连接。

**返回：** 如果连接成功则返回 `true`

#### `getLastError`

```cpp
std::string getLastError() const;
```

获取最后的错误消息（如果有）。

**返回：** 错误消息字符串

---

## EmbeddingModel

基于 ONNX 的文本向量化嵌入模型。支持导出为 ONNX 格式的模型（如 E5-small）。

### 构造函数

```cpp
explicit EmbeddingModel(
    const std::string& modelPath,
    bool normalize = true
);
```

创建嵌入模型实例。

**参数：**
- `modelPath` - ONNX 模型文件路径
- `normalize` - 是否归一化嵌入（L2 归一化，默认：true）

**示例：**
```cpp
EmbeddingModel model("models/e5-small/model.onnx");
```

### 编码方法

#### `encode`

```cpp
std::vector<float> encode(const std::string& text);
```

将单个文本编码为嵌入向量。

**参数：**
- `text` - 要编码的输入文本

**返回：** 表示嵌入的浮点数向量

**抛出：** 如果编码失败则抛出 `std::runtime_error`

**注意：** 对于 E5 模型，文本应包含前缀：
- `"query: "` 用于搜索查询
- `"passage: "` 用于要搜索的文档

**示例：**
```cpp
auto embedding = model.encode("passage: Hello world");
// 对于 E5-small 返回 384 维向量
```

#### `encodeBatch`

```cpp
std::vector<std::vector<float>> encodeBatch(const std::vector<std::string>& texts);
```

将一批文本编码为嵌入向量。

**参数：**
- `texts` - 要编码的输入文本向量

**返回：** 嵌入向量向量，每个都是浮点数向量

**抛出：** 如果编码失败则抛出 `std::runtime_error`

**示例：**
```cpp
std::vector<std::string> texts = {
    "passage: First document",
    "passage: Second document"
};
auto embeddings = model.encodeBatch(texts);
```

### 信息方法

#### `getDimension`

```cpp
size_t getDimension() const;
```

获取此模型生成的嵌入维度。

**返回：** 嵌入维度，如果模型未加载则返回 0

**示例：**
```cpp
size_t dim = model.getDimension(); // E5-small 为 384
```

#### `isLoaded`

```cpp
bool isLoaded() const;
```

检查模型是否已加载并可以使用。

**返回：** 如果模型已加载则返回 `true`

#### `getLastError`

```cpp
std::string getLastError() const;
```

获取最后的错误消息（如果有）。

**返回：** 错误消息字符串

#### `reload`

```cpp
bool reload();
```

重新加载模型（如果模型文件已更新则很有用）。

**返回：** 如果重新加载成功则返回 `true`

---

## VectorStore

结合 QdrantClient 和 EmbeddingModel 的高级接口。自动处理嵌入生成和向量操作。

### 构造函数

```cpp
VectorStore(
    const std::string& collectionName,
    const std::string& embeddingModelPath,
    const QdrantClient::Config& qdrantConfig
);
```

创建 VectorStore 实例。

**参数：**
- `collectionName` - Qdrant 集合名称
- `embeddingModelPath` - ONNX 嵌入模型文件路径
- `qdrantConfig` - Qdrant 客户端配置（必需，必须使用远程模式）

**示例：**
```cpp
auto config = QdrantClient::Config::remote("http://localhost:6333");
VectorStore store("my_collection", "models/e5-small/model.onnx", config);
```

### 初始化

#### `initialize`

```cpp
bool initialize();
```

初始化向量存储（加载嵌入模型，连接到 Qdrant，如果需要则创建集合）。

**返回：** 如果初始化成功则返回 `true`

**示例：**
```cpp
if (!store.initialize()) {
    std::cerr << "Failed to initialize VectorStore" << std::endl;
    return;
}
```

### 存储方法

#### `storeText`

```cpp
bool storeText(
    const std::string& text,
    const Payload& payload = {},
    std::optional<PointId> pointId = {}
);
```

在集合中存储带元数据的文本。自动生成嵌入，如果需要则添加 "passage: " 前缀。

**参数：**
- `text` - 要存储的文本（将自动嵌入）
- `payload` - 元数据负载（可选）
- `pointId` - 可选的点 ID（如果未提供则自动生成）

**返回：** 如果存储成功则返回 `true`

**示例：**
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

在集合中存储多个带元数据的文本。

**参数：**
- `texts` - 要存储的文本向量
- `payloads` - 元数据负载向量（每个文本一个，可选）
- `pointIds` - 可选的点 ID 向量（如果未提供则自动生成）

**返回：** 如果存储成功则返回 `true`

**示例：**
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

### 搜索方法

#### `search`

```cpp
std::vector<SearchResult> search(
    const std::string& queryText,
    size_t limit = 10,
    std::optional<float> scoreThreshold = {},
    const std::optional<Filter>& filter = {}
);
```

在集合中搜索相似文本。自动编码查询文本，如果需要则添加 "query: " 前缀。

**参数：**
- `queryText` - 要搜索的查询文本
- `limit` - 最大结果数（默认：10）
- `scoreThreshold` - 可选的最小相似度分数
- `filter` - 可选的元数据过滤器

**返回：** 搜索结果向量

**示例：**
```cpp
auto results = store.search("GitHub pull request", 10);
for (const auto& result : results) {
    std::cout << "Score: " << result.score << std::endl;
    if (result.payload.has_value()) {
        // 访问负载数据
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

使用原始查询向量进行搜索（不进行文本编码）。

**参数：**
- `queryVector` - 查询向量
- `limit` - 最大结果数（默认：10）
- `scoreThreshold` - 可选的最小相似度分数
- `filter` - 可选的元数据过滤器

**返回：** 搜索结果向量

### 访问器方法

#### `getClient`

```cpp
QdrantClient& getClient();
```

获取 QdrantClient 实例（用于高级操作）。

**返回：** QdrantClient 的引用

#### `getEmbeddingModel`

```cpp
std::optional<std::reference_wrapper<EmbeddingModel>> getEmbeddingModel();
```

获取 EmbeddingModel 实例（用于高级操作）。

**返回：** EmbeddingModel 的引用，如果未加载则返回 `nullopt`

#### `getEmbeddingDimension`

```cpp
size_t getEmbeddingDimension() const;
```

获取嵌入维度。

**返回：** 嵌入维度，如果模型未加载则返回 0

#### `getCollectionName`

```cpp
const std::string& getCollectionName() const;
```

获取集合名称。

**返回：** 集合名称

---

## 数据类型

### `DistanceMetric`

距离度量的枚举：

```cpp
enum class DistanceMetric {
    COSINE,    // 余弦相似度
    EUCLID,    // 欧几里得距离
    DOT        // 点积
};
```

### `PointId`

点标识符类型（可以是字符串或整数）：

```cpp
using PointId = std::variant<std::string, uint64_t>;
```

**示例：**
```cpp
PointId id1 = std::string("point-1");
PointId id2 = uint64_t(123);
```

### `PayloadValue`

元数据值类型：

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

元数据负载（字符串键到值的映射）：

```cpp
using Payload = std::map<std::string, PayloadValue>;
```

**示例：**
```cpp
Payload payload;
payload["title"] = std::string("Document Title");
payload["score"] = 95.5;
payload["tags"] = std::vector<std::string>{"tag1", "tag2"};
```

### `VectorPoint`

向量点结构：

```cpp
struct VectorPoint {
    PointId id;
    std::vector<float> vector;
    Payload payload;
};
```

### `SearchResult`

搜索结果结构：

```cpp
struct SearchResult {
    PointId id;
    float score;                    // 相似度分数
    std::optional<Payload> payload; // 可选负载
    std::optional<std::vector<float>> vector; // 可选向量
};
```

### `CollectionInfo`

集合信息结构：

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

用于元数据过滤的过滤条件：

```cpp
struct FilterCondition {
    FilterConditionType type;
    std::string key;
    
    // 用于 MATCH
    std::optional<PayloadValue> matchValue;
    
    // 用于 MATCH_TEXT
    std::optional<std::string> matchText;
    
    // 用于 RANGE
    std::optional<double> rangeGt;
    std::optional<double> rangeGte;
    std::optional<double> rangeLt;
    std::optional<double> rangeLte;
};
```

**静态工厂方法：**

```cpp
// 精确匹配
static FilterCondition createMatch(const std::string& key, const PayloadValue& value);

// 文本匹配（子字符串）
static FilterCondition createMatchText(const std::string& key, const std::string& text);

// 范围匹配
static FilterCondition createRange(
    const std::string& key,
    std::optional<double> gt = {},
    std::optional<double> gte = {},
    std::optional<double> lt = {},
    std::optional<double> lte = {}
);
```

### `Filter`

用于复杂查询的过滤器结构：

```cpp
struct Filter {
    std::vector<FilterCondition> must;      // 必须全部满足（AND）
    std::vector<FilterCondition> mustNot;   // 必须全部不满足（NOT）
    std::vector<FilterCondition> should;     // 至少一个必须满足（OR）
};
```

**静态工厂方法：**

```cpp
// 创建带 must 条件的过滤器
static Filter createMust(const std::vector<FilterCondition>& conditions);

// 创建带 must_not 条件的过滤器
static Filter createMustNot(const std::vector<FilterCondition>& conditions);

// 创建带 should 条件的过滤器
static Filter createShould(const std::vector<FilterCondition>& conditions);
```

---

## 示例

### 基本用法

```cpp
#include "VectorStore.h"
#include <iostream>

int main() {
    // 初始化 VectorStore（连接到 Qdrant 服务器）
    auto config = QdrantClient::Config::remote("http://localhost:6333");
    VectorStore store("my_collection", "models/e5-small/model.onnx", config);
    
    if (!store.initialize()) {
        std::cerr << "Failed to initialize" << std::endl;
        return 1;
    }
    
    // 存储文本
    Payload payload;
    payload["type"] = std::string("document");
    store.storeText("Working on GitHub PR #123", payload);
    
    // 搜索
    auto results = store.search("GitHub pull request", 5);
    for (const auto& result : results) {
        std::cout << "Score: " << result.score << std::endl;
    }
    
    return 0;
}
```

### 使用过滤器

```cpp
// 使用元数据过滤器搜索
auto filter = Filter::createMust({
    FilterCondition::createMatch("type", std::string("document")),
    FilterCondition::createRange("score", {}, 80.0, {}, {}) // score >= 80
});

auto results = store.search("query text", 10, {}, filter);
```

### 高级 QdrantClient 用法

```cpp
#include "QdrantClient.h"

// 创建集合（连接到 Qdrant 服务器）
auto config = QdrantClient::Config::remote("http://localhost:6333");
QdrantClient client(config);

client.createCollection("my_collection", 384, DistanceMetric::COSINE);

// 插入或更新向量
std::vector<VectorPoint> points;
points.push_back(VectorPoint(
    "point1",
    {0.1f, 0.2f, 0.3f, /* ... */},
    {{"key", std::string("value")}}
));
client.upsert("my_collection", points);

// 搜索
std::vector<float> queryVector = {0.1f, 0.2f, 0.3f, /* ... */};
auto results = client.search("my_collection", queryVector, 10);
```

### 直接使用 EmbeddingModel

```cpp
#include "EmbeddingModel.h"

EmbeddingModel model("models/e5-small/model.onnx");

// 编码文本（E5 需要前缀）
auto embedding = model.encode("passage: Hello world");

// 批量编码
std::vector<std::string> texts = {
    "passage: First document",
    "passage: Second document"
};
auto embeddings = model.encodeBatch(texts);
```

---

## 注意事项

- **E5 模型前缀**：E5-small 模型需要文本前缀（查询使用 `"query: "`，文档使用 `"passage: "`）。VectorStore 会自动添加这些前缀。
- **Qdrant 服务器**：需要运行 Qdrant 服务器。使用 Docker 运行 Qdrant：`docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant`
- **Web 控制台**：访问 Qdrant Web UI：`http://localhost:6333/dashboard` 来查看集合和数据。
- **线程安全**：类不是线程安全的。对于多线程访问，请使用单独的实例或外部同步。
- **错误处理**：检查返回值并使用 `getLastError()` 方法来诊断失败。

