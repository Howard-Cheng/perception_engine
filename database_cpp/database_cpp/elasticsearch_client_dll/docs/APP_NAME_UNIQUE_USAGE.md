# App Name 唯一性设计说明

## 概述

数据库已经重新设计，使用 `app_name` 作为文档的唯一标识符（Document ID）。这意味着：

- ? **相同 app_name 会自动更新**：如果插入相同的 app_name，Elasticsearch 会自动覆盖旧文档而不是创建新条目
- ? **避免重复数据**：保证每个应用程序只有一条记录
- ? **简化查询**：可以直接通过 app_name 快速获取文档

## 工作原理

### 原来的设计
```cpp
// 使用 event_id 作为文档 ID
std::string endpoint = "/" + indexName + "/_doc/" + event.eventId;
// 结果：每次插入都创建新文档，即使 app_name 相同
```

### 新设计
```cpp
// 使用 app_name 作为文档 ID
std::string endpoint = "/" + indexName + "/_doc/" + event.appName;
// 结果：相同 app_name 会自动覆盖旧文档
```

## 使用示例

### 1. 插入/更新文档

```cpp
#include "ElasticsearchClient.h"

using namespace elasticsearch;

ElasticsearchClient client("http://localhost:9200");

// 第一次插入 "Chrome" 应用
RawEvent event1;
event1.eventId = "evt_001";
event1.appName = "Chrome";  // 使用 Chrome 作为唯一标识
event1.deviceId = "device_123";
event1.timestamp = std::time(nullptr);
event1.createdAt = std::time(nullptr);
event1.interactionCount = 5;
event1.dwellTimeSeconds = 120;

client.indexDocument("my_index", event1);
// 文档被创建，ID = "Chrome"

// 稍后再次插入 "Chrome" 应用（不同的 event_id）
RawEvent event2;
event2.eventId = "evt_002";  // 不同的 event_id
event2.appName = "Chrome";   // 相同的 app_name
event2.deviceId = "device_123";
event2.timestamp = std::time(nullptr);
event2.createdAt = std::time(nullptr);
event2.interactionCount = 10;  // 更新的数据
event2.dwellTimeSeconds = 300;

client.indexDocument("my_index", event2);
// 旧文档被覆盖，仍然只有一条 "Chrome" 记录
// interaction_count 从 5 更新为 10
// dwell_time_seconds 从 120 更新为 300
```

### 2. 查询特定应用

```cpp
// 直接通过 app_name 获取文档
RawEvent chromeEvent = client.getDocumentByAppName("my_index", "Chrome");

if (!chromeEvent.appName.empty()) {
    std::cout << "App: " << chromeEvent.appName << std::endl;
    std::cout << "Interactions: " << chromeEvent.interactionCount << std::endl;
    std::cout << "Dwell Time: " << chromeEvent.dwellTimeSeconds << "s" << std::endl;
}
```

### 3. 删除特定应用

```cpp
// 删除 Chrome 的记录
bool deleted = client.deleteDocumentByAppName("my_index", "Chrome");
if (deleted) {
    std::cout << "Chrome 记录已删除" << std::endl;
}
```

### 4. 批量插入（自动去重）

```cpp
std::vector<RawEvent> events;

// 添加多个应用
RawEvent chrome, firefox, vscode;
chrome.appName = "Chrome";
firefox.appName = "Firefox";
vscode.appName = "VSCode";
// ... 设置其他字段 ...

events.push_back(chrome);
events.push_back(firefox);
events.push_back(vscode);

// 批量插入
client.bulkIndexDocuments("my_index", events);

// 如果稍后再次批量插入相同的应用名称
RawEvent chromeUpdated;
chromeUpdated.appName = "Chrome";  // 相同的 app_name
chromeUpdated.interactionCount = 100;  // 新的数据
// ... 设置其他字段 ...

events.clear();
events.push_back(chromeUpdated);

client.bulkIndexDocuments("my_index", events);
// Chrome 的旧数据会被更新，而不是创建新条目
```

## 注意事项

### 1. app_name 必须唯一且有意义
```cpp
// ? 好的做法
event.appName = "Chrome";
event.appName = "Visual Studio Code";
event.appName = "Microsoft Word";

// ? 不好的做法
event.appName = "";  // 空字符串
event.appName = "app_" + std::to_string(rand());  // 随机名称
```

### 2. event_id 仍然会被保存
```cpp
// event_id 仍然会被存储在文档中，只是不再用作文档 ID
event.eventId = "evt_12345";  // 会被保存，可以用于其他用途
event.appName = "Chrome";     // 用作文档 ID
```

### 3. 查询所有文档
```cpp
// 获取所有文档（分页）
json query = {
    {"query", {{"match_all", json::object()}}}
};

SearchResult result = client.search("my_index", query.dump(), 0, 100);

for (const auto& event : result.events) {
    std::cout << "App: " << event.appName << std::endl;
}
```

### 4. 按其他字段查询
```cpp
// 查询特定设备的所有应用
json query = {
    {"query", {
        {"term", {{"device_id", "device_123"}}}
    }}
};

SearchResult result = client.search("my_index", query.dump());
```

## 迁移指南

如果你已有使用旧版本（event_id 作为 ID）的数据：

### 选项 1: 重新索引数据
```cpp
// 1. 创建新索引
client.initializeIndex("my_index_v2");

// 2. 从旧索引读取数据
SearchResult oldData = client.search("my_index", "{\"query\":{\"match_all\":{}}}", 0, 10000);

// 3. 写入新索引（自动去重）
client.bulkIndexDocuments("my_index_v2", oldData.events);

// 4. 删除旧索引
client.deleteIndex("my_index");
```

### 选项 2: 使用别名切换
```bash
# 使用 Elasticsearch REST API
curl -X POST "http://localhost:9200/_aliases" -H 'Content-Type: application/json' -d'
{
  "actions": [
    {"remove": {"index": "my_index", "alias": "current_index"}},
    {"add": {"index": "my_index_v2", "alias": "current_index"}}
  ]
}
'
```

## 优势总结

1. **数据一致性**：每个应用只有一条记录，避免重复
2. **性能优化**：减少存储空间，加快查询速度
3. **简化逻辑**：不需要额外的去重代码
4. **自动更新**：新数据自动覆盖旧数据，保持最新状态

## API 变更

### 新增方法

```cpp
// 根据 app_name 获取文档
RawEvent getDocumentByAppName(const std::string& indexName, 
                               const std::string& appName);

// 根据 app_name 删除文档
bool deleteDocumentByAppName(const std::string& indexName, 
                             const std::string& appName);
```

### 修改的方法

```cpp
// indexDocument - 返回值变更
// 旧版本：返回 event.eventId
// 新版本：返回 event.appName
std::string docId = client.indexDocument("my_index", event);
// docId 现在是 app_name 而不是 event_id

// bulkIndexDocuments - 内部使用 app_name 作为 ID
// 使用方式不变，但行为改变（自动去重）
```
