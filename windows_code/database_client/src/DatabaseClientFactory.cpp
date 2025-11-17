// src/DatabaseClientFactory.cpp
#include "DatabaseClientFactory.h"
#include "ElasticsearchClient.h"
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
    // TODO: Implement PostgreSQL client
    throw std::runtime_error("PostgreSQL client not yet implemented");
}

std::unique_ptr<IDatabaseClient> 
DatabaseClientFactory::createSQLite(const std::string& dbPath) {
    // TODO: Implement SQLite client
    throw std::runtime_error("SQLite client not yet implemented");
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
        default:
            throw std::runtime_error("Unknown database type");
    }
}

} // namespace database
