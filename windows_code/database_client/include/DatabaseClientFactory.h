// include/DatabaseClientFactory.h
#pragma once

#include "IDatabaseClient.h"
#include "DatabaseTypes.h"
#include <memory>
#include <string>

namespace database {

/**
 * @brief Factory for creating database client instances
 * 
 * This factory provides a unified way to create different database client implementations.
 */
class DB_CLIENT_API DatabaseClientFactory {
public:
    /**
     * @brief Create an Elasticsearch client
     * 
     * @param url Elasticsearch base URL (e.g., "http://localhost:9200")
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> createElasticsearch(const std::string& url);
    
    /**
     * @brief Create a MongoDB client (not yet implemented)
     * 
     * @param connectionString MongoDB connection string
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> createMongoDB(const std::string& connectionString);
    
    /**
     * @brief Create a PostgreSQL client (not yet implemented)
     * 
     * @param connectionString PostgreSQL connection string
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> createPostgreSQL(const std::string& connectionString);
    
    /**
     * @brief Create a SQLite client (not yet implemented)
     * 
     * @param dbPath Path to SQLite database file
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> createSQLite(const std::string& dbPath);
    
    /**
     * @brief Create a MeiliSearch client
     * 
     * @param meiliUrl MeiliSearch URL (e.g., "http://localhost:7700")
     * @param apiKey API key for authentication (optional, default: "")
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> createMeiliSearch(const std::string& meiliUrl, 
                                                              const std::string& apiKey = "");
    
    /**
     * @brief Create a database client from type and connection string
     * 
     * @param type Database type
     * @param connectionString Connection string/URL
     * @return Unique pointer to IDatabaseClient
     */
    static std::unique_ptr<IDatabaseClient> create(DatabaseType type, 
                                                   const std::string& connectionString);
};

} // namespace database
