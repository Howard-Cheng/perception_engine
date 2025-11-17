// include/IDatabaseClient.h
#pragma once

#include "DatabaseTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace database {

/**
 * @brief Abstract base class for database clients
 * 
 * This interface defines the contract that all database implementations must follow.
 * Supports various database backends like Elasticsearch, MongoDB, PostgreSQL, etc.
 */
class DB_CLIENT_API IDatabaseClient {
public:
    virtual ~IDatabaseClient() = default;
    
    /**
     * @brief Get the database type
     * 
     * @return DatabaseType enumeration value
     */
    virtual DatabaseType getType() const = 0;
    
    /**
     * @brief Initialize a collection/index with proper schema
     * 
     * @param collectionName Collection/Index name
     * @return true if successful
     */
    virtual bool initializeCollection(const std::string& collectionName) = 0;
    
    /**
     * @brief Index/Insert a single document
     * 
     * @param collectionName Collection/Index name
     * @param event Event to index/insert
     * @return Event ID if successful, empty string otherwise
     */
    virtual std::string indexDocument(const std::string& collectionName, 
                                     const RawEvent& event) = 0;
    
    /**
     * @brief Bulk index/insert multiple documents
     * 
     * @param collectionName Collection/Index name
     * @param events Vector of events to index/insert
     * @return true if successful
     */
    virtual bool bulkIndexDocuments(const std::string& collectionName, 
                                   const std::vector<RawEvent>& events) = 0;
    
    /**
     * @brief Search/Query documents
     * 
     * @param collectionName Collection/Index name
     * @param query Query string (format depends on database type)
     * @param from Starting position (for pagination)
     * @param size Number of results to return
     * @return Search result
     */
    virtual SearchResult search(const std::string& collectionName,
                               const std::string& query,
                               int from = 0,
                               int size = 100) = 0;
    
    /**
     * @brief Get uncompressed events within time range
     * 
     * @param collectionName Collection/Index name
     * @param hours Number of hours to look back
     * @return Vector of uncompressed events
     */
    virtual std::vector<RawEvent> getUncompressedEvents(const std::string& collectionName, 
                                                       int hours = 24) = 0;
    
    /**
     * @brief Update a document
     * 
     * @param collectionName Collection/Index name
     * @param docId Document ID
     * @param updateData Update data (format depends on database type)
     * @return true if successful
     */
    virtual bool updateDocument(const std::string& collectionName,
                               const std::string& docId,
                               const std::string& updateData) = 0;
    
    /**
     * @brief Mark events as compressed
     * 
     * @param collectionName Collection/Index name
     * @param eventIds Vector of event IDs to mark
     * @param sessionId Session ID to associate
     * @return true if successful
     */
    virtual bool markEventsAsCompressed(const std::string& collectionName,
                                       const std::vector<std::string>& eventIds,
                                       const std::string& sessionId) = 0;
    
    /**
     * @brief Delete documents older than specified time
     * 
     * @param collectionName Collection/Index name
     * @param cutoffTime Cutoff timestamp
     * @return Number of documents deleted
     */
    virtual int deleteOlderThan(const std::string& collectionName, 
                               std::time_t cutoffTime) = 0;
    
    /**
     * @brief Refresh collection (make changes immediately visible)
     * 
     * @param collectionName Collection/Index name
     * @return true if successful
     */
    virtual bool refreshCollection(const std::string& collectionName) = 0;
    
    /**
     * @brief Get collection statistics
     * 
     * @param collectionName Collection/Index name
     * @return Collection statistics
     */
    virtual CollectionStats getCollectionStats(const std::string& collectionName) = 0;
    
    /**
     * @brief Get document count
     * 
     * @param collectionName Collection/Index name
     * @return Number of documents
     */
    virtual int getDocumentCount(const std::string& collectionName) = 0;
    
    /**
     * @brief Get uncompressed document count
     * 
     * @param collectionName Collection/Index name
     * @return Number of uncompressed documents
     */
    virtual int getUncompressedCount(const std::string& collectionName) = 0;
    
    /**
     * @brief Delete a collection/index
     * 
     * @param collectionName Collection/Index name
     * @return true if successful
     */
    virtual bool deleteCollection(const std::string& collectionName) = 0;
    
    /**
     * @brief Check if collection/index exists
     * 
     * @param collectionName Collection/Index name
     * @return true if collection exists
     */
    virtual bool collectionExists(const std::string& collectionName) = 0;
    
    /**
     * @brief Test connection to database
     * 
     * @return true if connected
     */
    virtual bool testConnection() = 0;
    
    /**
     * @brief Get database server info
     * 
     * @return JSON/String with server information
     */
    virtual std::string getServerInfo() = 0;
    
    /**
     * @brief Get document by app_name
     * 
     * @param collectionName Collection/Index name
     * @param appName Application name to query
     * @return RawEvent if found, empty event otherwise
     */
    virtual RawEvent getDocumentByAppName(const std::string& collectionName, 
                                         const std::string& appName) = 0;
    
    /**
     * @brief Delete document by app_name
     * 
     * @param collectionName Collection/Index name
     * @param appName Application name to delete
     * @return true if successful
     */
    virtual bool deleteDocumentByAppName(const std::string& collectionName, 
                                        const std::string& appName) = 0;
};

} // namespace database
