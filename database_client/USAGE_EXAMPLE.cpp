// 示例: 如何使用新的 Database Client 架构

#include "DatabaseClientFactory.h"
#include "DatabaseTypes.h"
#include <iostream>
#include <memory>

using namespace database;

int main() {
    // ========================================
    // 方法 1: 使用工厂直接创建 Elasticsearch 客户端
    // ========================================
    auto esClient = DatabaseClientFactory::createElasticsearch("http://localhost:9200");
    
    // ========================================
    // 方法 2: 使用统一工厂接口 (更灵活,便于切换数据库)
    // ========================================
    auto client = DatabaseClientFactory::create(
        DatabaseType::ELASTICSEARCH,
        "http://localhost:9200"
    );
    
    // 测试连接
    if (!client->testConnection()) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }
    
    std::cout << "Connected to " << client->getServerInfo() << std::endl;
    
    // 初始化集合
    const std::string collectionName = "perception_events";
    if (!client->initializeCollection(collectionName)) {
        std::cerr << "Failed to initialize collection" << std::endl;
        return 1;
    }
    
    // 创建事件
    RawEvent event;
    event.eventId = "test_event_001";
    event.timestamp = std::time(nullptr);
    event.createdAt = event.timestamp;
    event.deviceId = "device_001";
    event.appName = "TestApp";
    event.windowTitle = "Test Window";
    event.interactionCount = 5;
    event.dwellTimeSeconds = 120;
    event.compressed = false;
    
    // 索引文档
    std::string eventId = client->indexDocument(collectionName, event);
    if (!eventId.empty()) {
        std::cout << "Indexed event: " << eventId << std::endl;
    }
    
    // 批量索引
    std::vector<RawEvent> events;
    for (int i = 0; i < 10; i++) {
        RawEvent evt = event;
        evt.eventId = "test_event_00" + std::to_string(i);
        events.push_back(evt);
    }
    
    if (client->bulkIndexDocuments(collectionName, events)) {
        std::cout << "Bulk indexed " << events.size() << " events" << std::endl;
    }
    
    // 查询
    std::string query = R"({
        "query": {
            "match": {
                "app_name": "TestApp"
            }
        }
    })";
    
    SearchResult result = client->search(collectionName, query, 0, 10);
    std::cout << "Found " << result.totalHits << " events" << std::endl;
    
    // 获取统计信息
    CollectionStats stats = client->getCollectionStats(collectionName);
    std::cout << "Collection: " << stats.collectionName << std::endl;
    std::cout << "  Documents: " << stats.documentCount << std::endl;
    std::cout << "  Uncompressed: " << stats.uncompressedCount << std::endl;
    std::cout << "  Size: " << stats.sizeInBytes << " bytes" << std::endl;
    
    // ========================================
    // 切换到其他数据库 (未来)
    // ========================================
    /*
    // MongoDB
    auto mongoClient = DatabaseClientFactory::create(
        DatabaseType::MONGODB,
        "mongodb://localhost:27017"
    );
    
    // PostgreSQL
    auto pgClient = DatabaseClientFactory::create(
        DatabaseType::POSTGRESQL,
        "host=localhost port=5432 dbname=perception user=admin password=pass"
    );
    
    // 使用相同的接口
    mongoClient->initializeCollection("perception_events");
    mongoClient->indexDocument("perception_events", event);
    */
    
    return 0;
}

// ========================================
// 在 PerceptionEngine 中的使用示例
// ========================================
class PerceptionEngine {
private:
    std::unique_ptr<IDatabaseClient> dbClient_;
    
public:
    PerceptionEngine(const std::string& dbUrl) {
        // 从配置读取数据库类型
        DatabaseType dbType = DatabaseType::ELASTICSEARCH;
        
        // 创建数据库客户端
        dbClient_ = DatabaseClientFactory::create(dbType, dbUrl);
        
        // 初始化
        if (!dbClient_->testConnection()) {
            throw std::runtime_error("Failed to connect to database");
        }
        
        dbClient_->initializeCollection("perception_events");
    }
    
    void captureEvent(const RawEvent& event) {
        std::string eventId = dbClient_->indexDocument("perception_events", event);
        if (eventId.empty()) {
            // Handle error
        }
    }
    
    std::vector<RawEvent> getRecentEvents(int hours) {
        return dbClient_->getUncompressedEvents("perception_events", hours);
    }
};
