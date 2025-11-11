#pragma once

#include <string>

// Forward declaration for SQLite
struct sqlite3;

namespace perception {
namespace layer0 {

// Schema manager for Layer 0 (SQLite)
class SchemaManager {
public:
    // Initialize Layer 0 raw events schema
    static void initializeSqliteSchema(sqlite3* db);
    
    // Check if schema is initialized
    static bool isSqliteSchemaInitialized(sqlite3* db);
    
    // Get schema version
    static int getSchemaVersion(sqlite3* db);
    
private:
    static void createRawEventsTable(sqlite3* db);
    static void createIndexes(sqlite3* db);
    static void createMetadataTable(sqlite3* db);
};

} // namespace layer0
} // namespace perception
