// include/ElasticsearchClient.h
#pragma once

#include "DatabaseTypes.h"
#include "IDatabaseClient.h"
#include <memory>

namespace database {

/**
 * @brief Elasticsearch implementation of IDatabaseClient
 * 
 * Thread-safe client for interacting with Elasticsearch.
 */
class DB_CLIENT_API ElasticsearchClient : public IDatabaseClient {
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
    ~ElasticsearchClient() override;
    
    // Delete copy constructor and assignment operator
    ElasticsearchClient(const ElasticsearchClient&) = delete;
    ElasticsearchClient& operator=(const ElasticsearchClient&) = delete;
    
    // IDatabaseClient interface implementation
    DatabaseType getType() const override;
    bool initializeCollection(const std::string& collectionName) override;
    std::string indexDocument(const std::string& collectionName, const RawEvent& event) override;
    bool bulkIndexDocuments(const std::string& collectionName, 
                           const std::vector<RawEvent>& events) override;
    SearchResult search(const std::string& collectionName,
                       const std::string& query,
                       int from = 0,
                       int size = 100) override;
    std::vector<RawEvent> getUncompressedEvents(const std::string& collectionName, 
                                               int hours = 24) override;
    bool updateDocument(const std::string& collectionName,
                       const std::string& docId,
                       const std::string& updateData) override;
    bool markEventsAsCompressed(const std::string& collectionName,
                               const std::vector<std::string>& eventIds,
                               const std::string& sessionId) override;
    
    /**
     * @brief Mark events as compressed and update similar screen content
     * 
     * Updates multiple events with compressed status, session ID, and similar screen content.
     * This is useful when grouping events into sessions and storing similarity information.
     * 
     * @param collectionName Name of the collection
     * @param eventIds Vector of event IDs to update
     * @param sessionId Session ID to assign to all events
     * @param similarScreenContent Similar screen content summary to store
     * @return true if successful, false otherwise
     * 
     * @note Uses bulk update API for efficiency
     * @note All events in the batch receive the same similarScreenContent
     */
    bool markEventsAsCompressedWithSimilarity(
        const std::string& collectionName,
        const std::vector<std::string>& eventIds,
        const std::string& sessionId,
        const std::string& similarScreenContent);
    
    int deleteOlderThan(const std::string& collectionName, 
                       std::time_t cutoffTime) override;
    bool refreshCollection(const std::string& collectionName) override;
    CollectionStats getCollectionStats(const std::string& collectionName) override;
    int getDocumentCount(const std::string& collectionName) override;
    int getUncompressedCount(const std::string& collectionName) override;
    bool deleteCollection(const std::string& collectionName) override;
    bool collectionExists(const std::string& collectionName) override;
    bool testConnection() override;
    std::string getServerInfo() override;
    RawEvent getDocumentByAppName(const std::string& collectionName, 
                                 const std::string& appName) override;
    bool deleteDocumentByAppName(const std::string& collectionName, 
                                const std::string& appName) override;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace database
