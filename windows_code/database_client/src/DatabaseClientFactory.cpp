// src/DatabaseClientFactory.cpp
#include "DatabaseClientFactory.h"
#include "ElasticsearchClient.h"
#include "PostgreSQLClient.h"
//#include "SQLiteClient.h"
#include "MeiliSearchClient.h"
#include <stdexcept>

namespace database {

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createElasticsearch(const std::string& url) {
    return std::make_unique<ElasticsearchClient>(url);
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createMongoDB(const std::string& connectionString) {
    // TODO: Implement MongoDB client
    throw std::runtime_error("MongoDB client not yet implemented");
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createPostgreSQL(const std::string& connectionString) {
    // Default: automatically create database if it doesn't exist
    return std::make_unique<PostgreSQLClient>(connectionString, true);
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createSQLite(const std::string& dbPath) {
    //return std::make_unique<SQLiteClient>(dbPath);
    return nullptr;
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createMeiliSearch(const std::string& meiliUrl, const std::string& apiKey) {
    return std::make_unique<MeiliSearchClient>(meiliUrl, apiKey);
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::create(DatabaseType type, const std::string& connectionString) {
    switch (type) {
        case DatabaseType::ELASTICSEARCH:
            return createElasticsearch(connectionString);
        case DatabaseType::MONGODB:
            return createMongoDB(connectionString);
        case DatabaseType::POSTGRESQL:
            return createPostgreSQL(connectionString);
        case DatabaseType::SQLITE:
            return createSQLite(connectionString);
        case DatabaseType::MEILISEARCH:
            return createMeiliSearch(connectionString, "");
        default:
            throw std::runtime_error("Unsupported database type");
    }
}

} // namespace database
