#pragma once

#include <string>
#include <memory>
#include <atomic>
#include "common/Types.h"

// Forward declarations
namespace perception {
namespace layer0 {
class DataIngestion;
#ifdef ELASTICSEARCH_ENABLED
class ElasticsearchClient;
#endif
}
}

namespace perception {
namespace collector {

enum class StorageBackend {
    SQLITE,
    ELASTICSEARCH
};

struct CollectorConfig {
    std::string apiUrl = "http://localhost:8777/context";
    std::string dbPath = "./perception_data/raw_events.db";
    std::string deviceId = "pc_001";
    int pollIntervalSeconds = 5;
    int connectionTimeoutSeconds = 10;
    int maxRetries = 3;
    
    // Elasticsearch configuration
    StorageBackend storageBackend = StorageBackend::SQLITE;
    std::string elasticsearchUrl = "http://localhost:9200";
    std::string elasticsearchIndex = "perception_raw_events";
    
    CollectorConfig() = default;
};

class DataCollector {
public:
    explicit DataCollector(const CollectorConfig& config);
    ~DataCollector();
    
    // Delete copy constructor and assignment
    DataCollector(const DataCollector&) = delete;
    DataCollector& operator=(const DataCollector&) = delete;
    
    // Start the data collection loop
    void start();
    
    // Stop the data collection
    void stop();
    
    // Check if collector is running
    bool isRunning() const;
    
    // Get statistics
    int getTotalEventsCollected() const;
    int getFailedRequests() const;
    
private:
    // Fetch context data from Perception Engine API
    bool fetchContextData(std::string& jsonData);
    
    // Parse and ingest the context data
    bool processContextData(const std::string& jsonData);
    
    // Test API connection
    bool testConnection();
    
    // HTTP GET request helper
    bool httpGet(const std::string& url, std::string& response, int timeoutSeconds = 10);
    
    CollectorConfig config_;
    std::unique_ptr<layer0::DataIngestion> ingestion_;
#ifdef ELASTICSEARCH_ENABLED
    std::unique_ptr<layer0::ElasticsearchClient> esClient_;
#endif
    std::atomic<bool> running_;
    std::atomic<int> totalEvents_;
    std::atomic<int> failedRequests_;
};

} // namespace collector
} // namespace perception
