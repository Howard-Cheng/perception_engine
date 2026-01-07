// include/MeiliSearchClient.h
#pragma once

#include "IDatabaseClient.h"
#include <memory>
#include <string>

namespace database {

/**
 * @brief MeiliSearch Client Implementation
 * 
 * Provides full-text search capabilities using MeiliSearch REST API.
 * MeiliSearch is a lightweight, fast, and easy-to-use search engine with
 * built-in typo tolerance and instant search features.
 * 
 * Features:
 * - Typo-tolerant search (automatic)
 * - Fast search (< 50ms typical)
 * - RESTful API via cURL
 * - API key authentication support
 * - Compatible with IDatabaseClient interface
 * 
 * Usage:
 * @code
 * auto client = std::make_unique<MeiliSearchClient>(
 *     "http://localhost:7700",
 *     "your_api_key"
 * );
 * client->initializeCollection("events");
 * client->indexDocument("events", event);
 * auto results = client->search("events", "keyword", 0, 10);
 * @endcode
 */
class DB_CLIENT_API MeiliSearchClient : public IDatabaseClient {
public:
    /**
     * @brief Construct a new MeiliSearch Client
     * 
     * @param meiliUrl MeiliSearch server URL (e.g., "http://localhost:7700")
     * @param apiKey API key for authentication (optional, use master key for production)
     */
    explicit MeiliSearchClient(const std::string& meiliUrl, const std::string& apiKey = "");
    
    /**
     * @brief Destroy the MeiliSearch Client
     */
    ~MeiliSearchClient() override;

    // Disable copy constructor and assignment operator
    MeiliSearchClient(const MeiliSearchClient&) = delete;
    MeiliSearchClient& operator=(const MeiliSearchClient&) = delete;

    // IDatabaseClient interface implementation
    DatabaseType getType() const override;
    bool initializeCollection(const std::string& collectionName) override;
    std::string indexDocument(const std::string& collectionName, const RawEvent& event) override;
    bool bulkIndexDocuments(const std::string& collectionName, const std::vector<RawEvent>& events) override;
    SearchResult search(const std::string& collectionName, const std::string& query, int from = 0, int size = 100) override;
    std::vector<RawEvent> getUncompressedEvents(const std::string& collectionName, int max_count = 100) override;
    bool updateDocument(const std::string& collectionName, const std::string& docId, const std::string& updateData) override;
    bool markEventsAsCompressed(const std::string& collectionName, const std::vector<std::string>& eventIds, const std::string& sessionId) override;
    int deleteOlderThan(const std::string& collectionName, std::time_t cutoffTime) override;
    bool refreshCollection(const std::string& collectionName) override;
    CollectionStats getCollectionStats(const std::string& collectionName) override;
    int getDocumentCount(const std::string& collectionName) override;
    int getUncompressedCount(const std::string& collectionName) override;
    bool deleteCollection(const std::string& collectionName) override;
    bool collectionExists(const std::string& collectionName) override;
    bool testConnection() override;
    std::string getServerInfo() override;
    RawEvent getDocumentByAppName(const std::string& collectionName, const std::string& appName) override;
    bool deleteDocumentByAppName(const std::string& collectionName, const std::string& appName) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace database
