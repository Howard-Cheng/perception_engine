// include/PostgreSQLClient.h
#pragma once

#include "DatabaseTypes.h"
#include "IDatabaseClient.h"
#include <memory>

namespace database {

/**
 * @brief PostgreSQL implementation of IDatabaseClient
 * 
 * Thread-safe client for interacting with PostgreSQL database.
 * Uses libpq (PostgreSQL C API) for database operations.
 */
class DB_CLIENT_API PostgreSQLClient : public IDatabaseClient {
public:
    /**
     * @brief Construct a new PostgreSQL Client
     * 
     * @param connectionString PostgreSQL connection string
     *        Examples:
     *          "host=localhost port=5432 dbname=perception user=postgres password=yourpass"
     *          "postgresql://postgres:yourpass@localhost:5432/perception"
     */
    explicit PostgreSQLClient(const std::string& connectionString);
    
    /**
     * @brief Destroy the PostgreSQL Client
     */
    ~PostgreSQLClient() override;
    
    // Delete copy constructor and assignment operator
    PostgreSQLClient(const PostgreSQLClient&) = delete;
    PostgreSQLClient& operator=(const PostgreSQLClient&) = delete;
    
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
     * @param collectionName Name of the table
     * @param eventIds Vector of event IDs to update
     * @param sessionId Session ID to assign to all events
     * @param similarScreenContent Similar screen content summary to store
     * @return true if successful, false otherwise
     * 
     * @note Uses prepared statements for efficiency and security
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
    
    /**
     * @brief Perform fuzzy search on a specific field using PostgreSQL pg_trgm extension
     * 
     * This method provides fuzzy string matching similar to Elasticsearch's fuzzy query.
     * It uses trigram similarity matching to find strings that are similar to the search value,
     * even with typos or slight variations.
     * 
     * @param tableName Name of the table to search
     * @param field Field name to search (e.g., "screen_content", "window_title")
     * @param searchValue Value to search for (can have typos)
     * @param similarityThreshold Minimum similarity threshold (0.0 to 1.0, default 0.3)
     * @param from Starting position for pagination (default 0)
     * @param size Number of results to return (default 100)
     * @return SearchResult containing matching events sorted by similarity
     * 
     * @note Requires pg_trgm extension to be enabled (automatically done in initializeCollection)
     * @note Results are ordered by similarity score (highest first)
     * @note similarityThreshold of 0.3 is similar to Elasticsearch's "AUTO" fuzziness
     * 
     * @example
     * // Find documents with "elasticsarch" (typo) in screen_content
     * auto results = client.fuzzySearch("events", "screen_content", "elasticsarch", 0.3);
     */
    SearchResult fuzzySearch(
        const std::string& tableName,
        const std::string& field,
        const std::string& searchValue,
        double similarityThreshold = 0.3,
        int from = 0,
        int size = 100);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace database
