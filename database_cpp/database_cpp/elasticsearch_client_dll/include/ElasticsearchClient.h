// include/ElasticsearchClient.h
#pragma once

#include "ElasticsearchTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace elasticsearch {

/**
 * @brief Elasticsearch C++ Client
 * 
 * Thread-safe client for interacting with Elasticsearch.
 */
class ES_CLIENT_API ElasticsearchClient {
public:
    /**
     * @brief Construct a new Elasticsearch Client
     * 
     * @param esUrl Elasticsearch base URL (e.g., "http://localhost:9200")
     */
    explicit ElasticsearchClient(const std::string& esUrl = "http://localhost:9200");
    
    /**
     * @brief Destroy the Elasticsearch Client
     */
    ~ElasticsearchClient();
    
    // Delete copy constructor and assignment operator
    ElasticsearchClient(const ElasticsearchClient&) = delete;
    ElasticsearchClient& operator=(const ElasticsearchClient&) = delete;
    
    /**
     * @brief Initialize an index with proper mapping
     * 
     * @param indexName Index name
     * @return true if successful
     */
    bool initializeIndex(const std::string& indexName);
    
    /**
     * @brief Index a single document
     * 
     * @param indexName Index name
     * @param event Event to index
     * @return Event ID if successful, empty string otherwise
     */
    std::string indexDocument(const std::string& indexName, const RawEvent& event);
    
    /**
     * @brief Bulk index multiple documents
     * 
     * @param indexName Index name
     * @param events Vector of events to index
     * @return true if successful
     */
    bool bulkIndexDocuments(const std::string& indexName, 
                           const std::vector<RawEvent>& events);
    
    /**
     * @brief Search documents
     * 
     * @param indexName Index name
     * @param query JSON query string
     * @param from Starting position (for pagination)
     * @param size Number of results to return
     * @return Search result
     */
    SearchResult search(const std::string& indexName,
                       const std::string& query,
                       int from = 0,
                       int size = 100);
    
    /**
     * @brief Get uncompressed events within time range
     * 
     * @param indexName Index name
     * @param hours Number of hours to look back
     * @return Vector of uncompressed events
     */
    std::vector<RawEvent> getUncompressedEvents(const std::string& indexName, 
                                                int hours = 24);
    
    /**
     * @brief Update a document
     * 
     * @param indexName Index name
     * @param docId Document ID
     * @param updateJson JSON update script
     * @return true if successful
     */
    bool updateDocument(const std::string& indexName,
                       const std::string& docId,
                       const std::string& updateJson);
    
    /**
     * @brief Mark events as compressed
     * 
     * @param indexName Index name
     * @param eventIds Vector of event IDs to mark
     * @param sessionId Session ID to associate
     * @return true if successful
     */
    bool markEventsAsCompressed(const std::string& indexName,
                               const std::vector<std::string>& eventIds,
                               const std::string& sessionId);
    
    /**
     * @brief Delete documents older than specified time
     * 
     * @param indexName Index name
     * @param cutoffTime Cutoff timestamp
     * @return Number of documents deleted
     */
    int deleteOlderThan(const std::string& indexName, 
                       std::time_t cutoffTime);
    
    /**
     * @brief Refresh index (make indexed documents immediately searchable)
     * 
     * @param indexName Index name
     * @return true if successful
     */
    bool refreshIndex(const std::string& indexName);
    
    /**
     * @brief Get index statistics
     * 
     * @param indexName Index name
     * @return Index statistics
     */
    IndexStats getIndexStats(const std::string& indexName);
    
    /**
     * @brief Get document count
     * 
     * @param indexName Index name
     * @return Number of documents
     */
    int getDocumentCount(const std::string& indexName);
    
    /**
     * @brief Get uncompressed document count
     * 
     * @param indexName Index name
     * @return Number of uncompressed documents
     */
    int getUncompressedCount(const std::string& indexName);
    
    /**
     * @brief Delete an index
     * 
     * @param indexName Index name
     * @return true if successful
     */
    bool deleteIndex(const std::string& indexName);
    
    /**
     * @brief Check if index exists
     * 
     * @param indexName Index name
     * @return true if index exists
     */
    bool indexExists(const std::string& indexName);
    
    /**
     * @brief Test connection to Elasticsearch
     * 
     * @return true if connected
     */
    bool testConnection();
    
    /**
     * @brief Get Elasticsearch cluster info
     * 
     * @return JSON string with cluster information
     */
    std::string getClusterInfo();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace elasticsearch
