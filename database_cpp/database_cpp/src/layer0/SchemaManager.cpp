#include "layer0/SchemaManager.h"
#include "common/Logger.h"
#include <sqlite3.h>
#include <stdexcept>

namespace perception {
namespace layer0 {

void SchemaManager::initializeSqliteSchema(sqlite3* db) {
    if (!db) {
        throw std::runtime_error("Invalid SQLite database connection");
    }
    
    LOG_INFO("Initializing SQLite schema for Layer 0...");
    
    createMetadataTable(db);
    createRawEventsTable(db);
    createIndexes(db);
    
    LOG_INFO("SQLite schema initialized successfully");
}

bool SchemaManager::isSqliteSchemaInitialized(sqlite3* db) {
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='raw_events'";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

int SchemaManager::getSchemaVersion(sqlite3* db) {
    const char* sql = "SELECT value FROM schema_metadata WHERE key='version'";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return version;
}

void SchemaManager::createRawEventsTable(sqlite3* db) {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS raw_events (
            event_id TEXT PRIMARY KEY,
            timestamp TIMESTAMP NOT NULL,
            device_id TEXT NOT NULL,
            
            -- App context
            app_name TEXT NOT NULL,
            window_title TEXT,
            url TEXT,
            
            -- Content
            screen_content TEXT,
            screen_content_hash TEXT,
            
            -- Interaction signals
            mouse_events TEXT,
            interaction_count INTEGER DEFAULT 0,
            dwell_time_seconds INTEGER DEFAULT 0,
            
            -- Audio/Camera
            voice_transcription TEXT,
            camera_description TEXT,
            
            -- System info
            battery_percent INTEGER,
            is_charging BOOLEAN DEFAULT 0,
            network_type TEXT,
            location_lat REAL,
            location_lon REAL,
            cpu_usage REAL,
            memory_usage REAL,
            
            -- Classification
            content_type TEXT,
            domain TEXT,
            
            -- Session linking
            session_id TEXT,
            
            -- Status
            compressed BOOLEAN DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "Failed to create raw_events table: ";
        if (errMsg) {
            error += errMsg;
            sqlite3_free(errMsg);
        }
        throw std::runtime_error(error);
    }
    
    LOG_INFO("Created raw_events table");
}

void SchemaManager::createIndexes(sqlite3* db) {
    const char* indexes[] = {
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON raw_events(timestamp)",
        "CREATE INDEX IF NOT EXISTS idx_compressed ON raw_events(compressed)",
        "CREATE INDEX IF NOT EXISTS idx_session ON raw_events(session_id)",
        "CREATE INDEX IF NOT EXISTS idx_device ON raw_events(device_id)",
        "CREATE INDEX IF NOT EXISTS idx_app_name ON raw_events(app_name)",
        "CREATE INDEX IF NOT EXISTS idx_content_hash ON raw_events(screen_content_hash)"
    };
    
    for (const char* sql : indexes) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            LOG_WARNING(std::string("Failed to create index: ") + (errMsg ? errMsg : "unknown error"));
            if (errMsg) sqlite3_free(errMsg);
        }
    }
    
    LOG_INFO("Created database indexes");
}

void SchemaManager::createMetadataTable(sqlite3* db) {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            LOG_WARNING(std::string("Failed to create metadata table: ") + errMsg);
            sqlite3_free(errMsg);
        }
    }
    
    // Insert schema version
    const char* insertSql = "INSERT OR REPLACE INTO schema_metadata (key, value) VALUES ('version', '1')";
    if (sqlite3_exec(db, insertSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        if (errMsg) {
            LOG_WARNING(std::string("Failed to set schema version: ") + errMsg);
            sqlite3_free(errMsg);
        }
    }
}

} // namespace layer0
} // namespace perception
