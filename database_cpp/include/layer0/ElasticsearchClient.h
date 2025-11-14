// include/layer0/ElasticsearchClient.h
#pragma once

#include "common/Types.h"
#include "layer0/DataIngestion.h"
#include <string>
#include <vector>
#include <memory>

namespace perception {
namespace layer0 {

class ElasticsearchClient {
public:
    // Constructor
    explicit ElasticsearchClient(const std::string& esUrl = "http://localhost:9200");
    ~ElasticsearchClient();
    
    // Initialize index with mapping
    bool initializeIndex(const std::string& indexName);
    
    // Index a document (insert/update)
    std::string indexDocument(const std::string& indexName, const RawEvent& event);
    
    // Bulk indexing for performance
    bool bulkIndexDocuments(const std::string& indexName, 
                           const std::vector<RawEvent>& events);
    
    // Search
    std::vector<RawEvent> search(const std::string& indexName,
                                const std::string& query,
                                int from = 0,
                                int size = 100);
    
    // Query uncompressed events
    std::vector<RawEvent> getUncompressedEvents(int hours = 24);
    
    // Update document
    bool updateDocument(const std::string& indexName,
                       const std::string& docId,
                       const std::string& updateScript);
    
    // Mark events as compressed
    bool markEventsAsCompressed(const std::vector<std::string>& eventIds,
                               const std::string& sessionId);
    
    // Delete old documents
    int deleteOlderThan(const Timestamp& cutoffTime);
    
    // Refresh index (force immediate availability of indexed documents)
    bool refreshIndex(const std::string& indexName);
    
    // Statistics
    int getDocumentCount(const std::string& indexName);
    int getUncompressedCount();
    
private:
    std::string esUrl_;
    std::string indexName_;
    
    // HTTP helpers
    bool httpRequest(const std::string& method,
                    const std::string& endpoint,
                    const std::string& body,
                    std::string& response);
    
    // JSON conversion
    std::string eventToJson(const RawEvent& event) const;
    RawEvent jsonToEvent(const std::string& json) const;
};

} // namespace layer0
} // namespace perception