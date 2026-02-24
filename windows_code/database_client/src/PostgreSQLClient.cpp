// src/PostgreSQLClient.cpp
#include "PostgreSQLClient.h"
#include "pe_base/logger.h"
#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <chrono>
#include <vector>
#include <set>
#include <map>
#include <fstream>

using json = nlohmann::json;

namespace database {

// Helper: Convert time_t to PostgreSQL timestamp string (Local Time)
static std::string timestampToPostgreSQL(std::time_t timestamp) {
    std::tm tm_val;
#ifdef _WIN32
    // Use localtime_s to store as local time
    localtime_s(&tm_val, &timestamp);
#else
    localtime_r(&timestamp, &tm_val);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Helper: Convert PostgreSQL timestamp string to time_t (Local Time)
static std::time_t postgresqlToTimestamp(const std::string& pgTimestamp) {
    std::tm tm_val = {};
    std::istringstream ss(pgTimestamp);
    ss >> std::get_time(&tm_val, "%Y-%m-%d %H:%M:%S");
    
    // Use mktime to convert local time tm to time_t
    return std::mktime(&tm_val);
}

// Helper: Escape string for SQL (without adding quotes)
static std::string escapeStringForSQL(PGconn* conn, const std::string& input) {
    if (!conn || input.empty()) return input;
    
    // PQescapeStringConn doesn't add quotes, just escapes special characters
    size_t length = input.length();
    char* escaped = new char[length * 2 + 1];  // Max possible size
    
    size_t escapedLength = PQescapeStringConn(conn, escaped, input.c_str(), length, nullptr);
    std::string result(escaped, escapedLength);
    
    delete[] escaped;
    return result;
}

// Helper: Escape string for PostgreSQL (with quotes for SQL literals)
static std::string escapeString(PGconn* conn, const std::string& input) {
    if (!conn) return input;
    
    char* escaped = PQescapeLiteral(conn, input.c_str(), input.length());
    if (!escaped) return input;
    
    std::string result(escaped);
    PQfreemem(escaped);
    return result;
}

// Helper: Content type to string
static std::string contentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::TEXT: return "TEXT";
        case ContentType::IMAGE: return "IMAGE";
        case ContentType::VIDEO: return "VIDEO";
        case ContentType::AUDIO: return "AUDIO";
        case ContentType::CODE: return "CODE";
        case ContentType::DOCUMENT: return "DOCUMENT";
        case ContentType::MEETING: return "MEETING";
        default: return "UNKNOWN";
    }
}

// Helper: String to content type
static ContentType stringToContentType(const std::string& str) {
    if (str == "TEXT") return ContentType::TEXT;
    if (str == "IMAGE") return ContentType::IMAGE;
    if (str == "VIDEO") return ContentType::VIDEO;
    if (str == "AUDIO") return ContentType::AUDIO;
    if (str == "CODE") return ContentType::CODE;
    if (str == "DOCUMENT") return ContentType::DOCUMENT;
    if (str == "MEETING") return ContentType::MEETING;
    return ContentType::UNKNOWN;
}

// Helper: Domain to string
static std::string domainToString(Domain domain) {
    switch (domain) {
        case Domain::WORK: return "WORK";
        case Domain::ENTERTAINMENT: return "ENTERTAINMENT";
        case Domain::SOCIAL: return "SOCIAL";
        case Domain::SHOPPING: return "SHOPPING";
        case Domain::NEWS: return "NEWS";
        case Domain::EDUCATION: return "EDUCATION";
        case Domain::HEALTH: return "HEALTH";
        case Domain::OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

// Helper: String to domain
static Domain stringToDomain(const std::string& str) {
    if (str == "WORK") return Domain::WORK;
    if (str == "ENTERTAINMENT") return Domain::ENTERTAINMENT;
    if (str == "SOCIAL") return Domain::SOCIAL;
    if (str == "SHOPPING") return Domain::SHOPPING;
    if (str == "NEWS") return Domain::NEWS;
    if (str == "EDUCATION") return Domain::EDUCATION;
    if (str == "HEALTH") return Domain::HEALTH;
    if (str == "OTHER") return Domain::OTHER;
    return Domain::UNKNOWN;
}

// Implementation class (Pimpl pattern)
class PostgreSQLClient::Impl {
public:
    PGconn* conn_;
    std::string connectionString_;
    bool autoCreateDatabase_;
    
    explicit Impl(const std::string& connectionString, bool autoCreateDatabase) 
        : conn_(nullptr), connectionString_(connectionString), autoCreateDatabase_(autoCreateDatabase) {
        connect();
    }
    
    ~Impl() {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }
    
    bool connect() {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
        
        conn_ = PQconnectdb(connectionString_.c_str());
        
        if (PQstatus(conn_) != CONNECTION_OK) {
            PE_ERROR("PostgreSQL connection failed: " << PQerrorMessage(conn_));
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
        }
        
        // Set search_path to public schema after successful connection
        PGresult* res = PQexec(conn_, "SET search_path TO public;");
        if (res) {
            ExecStatusType status = PQresultStatus(res);
            if (status != PGRES_COMMAND_OK) {
                PE_ERROR("Failed to set search_path: " << PQerrorMessage(conn_));
            }
            PQclear(res);
        }
        
        return true;
    }
    
    
    /**
     * @brief Parse database name from connection string
     */
    std::string parseDbName() const {
        std::string dbName;
        size_t dbNamePos = connectionString_.find("dbname=");
        if (dbNamePos != std::string::npos) {
            size_t startPos = dbNamePos + 7; // Length of "dbname="
            size_t endPos = connectionString_.find_first_of(" \t\n\r", startPos);
            if (endPos == std::string::npos) {
                dbName = connectionString_.substr(startPos);
            } else {
                dbName = connectionString_.substr(startPos, endPos - startPos);
            }
        }
        return dbName;
    }
    
    /**
     * @brief Create a connection string pointing to 'postgres' database
     */
    std::string buildPostgresConnStr() const {
        std::string postgresConnStr = connectionString_;
        size_t dbNameStart = postgresConnStr.find("dbname=");
        if (dbNameStart != std::string::npos) {
            size_t valueStart = dbNameStart + 7;
            size_t valueEnd = postgresConnStr.find_first_of(" \t\n\r", valueStart);
            
            if (valueEnd == std::string::npos) {
                postgresConnStr.replace(valueStart, std::string::npos, "postgres");
            } else {
                postgresConnStr.replace(valueStart, valueEnd - valueStart, "postgres");
            }
        }
        return postgresConnStr;
    }
    
    bool executeQuery(const std::string& query, PGresult** result = nullptr) {
        if (!conn_) {
            if (!connect()) return false;
        }
        
        PGresult* res = PQexec(conn_, query.c_str());
        
        if (!res) {
            PE_ERROR("Query failed (null result): " << query.substr(0, 100));
            return false;
        }
        
        ExecStatusType status = PQresultStatus(res);
        
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            PE_ERROR("Query failed: " << PQerrorMessage(conn_));
            PE_ERROR("Query (first 200 chars): " << query.substr(0, 200));
            PQclear(res);
            return false;
        }
        
        if (result) {
            *result = res;
        } else {
            PQclear(res);
        }
        
        return true;
    }
    
    RawEvent resultToEvent(PGresult* res, int row) {
        RawEvent event;
        
        // Required fields
        event.eventId = PQgetvalue(res, row, PQfnumber(res, "event_id"));
        event.timestamp = postgresqlToTimestamp(PQgetvalue(res, row, PQfnumber(res, "timestamp")));
        event.createdAt = postgresqlToTimestamp(PQgetvalue(res, row, PQfnumber(res, "created_at")));
        event.deviceId = PQgetvalue(res, row, PQfnumber(res, "device_id"));
        event.appName = PQgetvalue(res, row, PQfnumber(res, "app_name"));
        
        // Integer fields
        int interactionIdx = PQfnumber(res, "interaction_count");
        if (interactionIdx >= 0 && !PQgetisnull(res, row, interactionIdx)) {
            event.interactionCount = std::atoi(PQgetvalue(res, row, interactionIdx));
        }
        
        int dwellIdx = PQfnumber(res, "dwell_time_seconds");
        if (dwellIdx >= 0 && !PQgetisnull(res, row, dwellIdx)) {
            event.dwellTimeSeconds = std::atoi(PQgetvalue(res, row, dwellIdx));
        }
        
        // Boolean fields
        int compressedIdx = PQfnumber(res, "compressed");
        if (compressedIdx >= 0 && !PQgetisnull(res, row, compressedIdx)) {
            event.compressed = (std::string(PQgetvalue(res, row, compressedIdx)) == "t");
        }
        
        int summarizedIdx = PQfnumber(res, "summarized");
        if (summarizedIdx >= 0 && !PQgetisnull(res, row, summarizedIdx)) {
            event.summarized = (std::string(PQgetvalue(res, row, summarizedIdx)) == "t");
        }
        
        // Optional string fields
        auto getOptionalString = [&](const char* fieldName) -> std::optional<std::string> {
            int idx = PQfnumber(res, fieldName);
            if (idx >= 0 && !PQgetisnull(res, row, idx)) {
                return std::string(PQgetvalue(res, row, idx));
            }
            return std::nullopt;
        };
        
        event.windowTitle = getOptionalString("window_title");
        event.url = getOptionalString("url");
        event.screenContent = getOptionalString("screen_content");
        event.screenContentHash = getOptionalString("screen_content_hash");
        event.similarScreenContent = getOptionalString("similar_screen_content");
        event.voiceTranscription = getOptionalString("voice_transcription");
        event.cameraDescription = getOptionalString("camera_description");
        event.audioContent = getOptionalString("audio_content");
        event.sessionId = getOptionalString("session_id");
        
        // Enum fields
        auto contentTypeStr = getOptionalString("content_type");
        if (contentTypeStr) {
            event.contentType = stringToContentType(*contentTypeStr);
        }
        
        auto domainStr = getOptionalString("domain");
        if (domainStr) {
            event.domain = stringToDomain(*domainStr);
        }
        
        // Mouse events (stored as JSONB)
        int mouseEventsIdx = PQfnumber(res, "mouse_events");
        if (mouseEventsIdx >= 0 && !PQgetisnull(res, row, mouseEventsIdx)) {
            try {
                std::string mouseEventsJson = PQgetvalue(res, row, mouseEventsIdx);
                auto mouseArray = json::parse(mouseEventsJson);
                
                for (const auto& meJson : mouseArray) {
                    MouseEvent me;
                    if (meJson.contains("timestamp")) {
                        me.timestamp = postgresqlToTimestamp(meJson["timestamp"].get<std::string>());
                    }
                    /*if (meJson.contains("event_type")) {
                        me.eventType = meJson["event_type"].get<std::string>();
                    }*/
                    if (meJson.contains("slectedcontent")) {
                        me.content = meJson["content"].get<std::string>();
                    }
                    /*if (meJson.contains("pos_x")) {
                        me.posX = meJson["pos_x"].get<int>();
                    }
                    if (meJson.contains("pos_y")) {
                        me.posY = meJson["pos_y"].get<int>();
                    }
                    if (meJson.contains("element_type")) {
                        me.elementType = meJson["element_type"].get<std::string>();
                    }*/
                    event.mouseEvents.push_back(me);
                }
            } catch (...) {
                // Ignore JSON parse errors
            }
        }
        
        // System info (stored as JSONB)
        int sysInfoIdx = PQfnumber(res, "system_info");
        if (sysInfoIdx >= 0 && !PQgetisnull(res, row, sysInfoIdx)) {
            try {
                std::string sysInfoJson = PQgetvalue(res, row, sysInfoIdx);
                auto sysInfo = json::parse(sysInfoJson);
                
                if (sysInfo.contains("battery_percent") && !sysInfo["battery_percent"].is_null()) {
                    event.systemInfo.batteryPercent = sysInfo["battery_percent"].get<int>();
                }
                if (sysInfo.contains("is_charging") && !sysInfo["is_charging"].is_null()) {
                    event.systemInfo.isCharging = sysInfo["is_charging"].get<bool>();
                }
                if (sysInfo.contains("network_type") && !sysInfo["network_type"].is_null()) {
                    event.systemInfo.networkType = sysInfo["network_type"].get<std::string>();
                }
                if (sysInfo.contains("location_lat") && !sysInfo["location_lat"].is_null()) {
                    event.systemInfo.locationLat = sysInfo["location_lat"].get<double>();
                }
                if (sysInfo.contains("location_lon") && !sysInfo["location_lon"].is_null()) {
                    event.systemInfo.locationLon = sysInfo["location_lon"].get<double>();
                }
                if (sysInfo.contains("cpu_usage") && !sysInfo["cpu_usage"].is_null()) {
                    event.systemInfo.cpuUsage = sysInfo["cpu_usage"].get<double>();
                }
                if (sysInfo.contains("memory_usage") && !sysInfo["memory_usage"].is_null()) {
                    event.systemInfo.memoryUsage = sysInfo["memory_usage"].get<double>();
                }
            } catch (...) {
                // Ignore JSON parse errors
            }
        }
        
        return event;
    }
};

// PostgreSQLClient implementation
PostgreSQLClient::PostgreSQLClient(const std::string& connectionString, bool autoCreateDatabase)
    : pImpl_(std::make_unique<Impl>(connectionString, autoCreateDatabase)) {
    
    if (autoCreateDatabase) {
        ensureDatabaseExists();
    }
}

PostgreSQLClient::~PostgreSQLClient() = default;

DatabaseType PostgreSQLClient::getType() const {
    return DatabaseType::POSTGRESQL;
}




bool PostgreSQLClient::initializeCollection(const std::string& tableName) {
    // Ensure we have a valid connection
    if (!pImpl_->conn_) {
        PE_ERROR("[PostgreSQL] No database connection available");
        return false;
    }
    
    // Ensure public schema exists (some cloud PostgreSQL instances don't have it by default)
    if (!pImpl_->executeQuery("CREATE SCHEMA IF NOT EXISTS public;")) {
        PE_INFO("[PostgreSQL] Warning: Could not create public schema (may already exist or require privileges)");
    }
    
    // Grant permissions on public schema (may fail without superuser privileges, but that's OK)
    pImpl_->executeQuery("GRANT ALL ON SCHEMA public TO PUBLIC;");
    
    // Set the search path to public schema explicitly
    if (!pImpl_->executeQuery("SET search_path TO public;")) {
        PE_ERROR("[PostgreSQL] Failed to set search_path to public schema");
        // Continue anyway - try with default search path
    }
    
    // Try to enable pg_trgm extension for fuzzy search (may fail without superuser privileges)
    // This is optional - fuzzy search will just not work if extension is not available
    if (!pImpl_->executeQuery("CREATE EXTENSION IF NOT EXISTS pg_trgm;")) {
        PE_INFO("[PostgreSQL] Warning: Could not create pg_trgm extension (may require superuser privileges). Fuzzy search will be limited.");
    }
    
    
    // Define expected columns with their types and defaults
// Format: {column_name, {data_type, character_maximum_length (0 if N/A), is_nullable, default_value, extra_constraints}}
struct ColumnDef {
    std::string name;
    std::string dataType;        // PostgreSQL data type (lowercase)
    int maxLength;               // For varchar, 0 means no limit or N/A
    bool nullable;               // true = nullable, false = NOT NULL
    std::string defaultValue;    // Default value (empty if none)
    std::string extraConstraints; // e.g., "PRIMARY KEY"
};

// Define indexes with their properties for dynamic creation
struct IndexDef {
    std::string columnName;
    std::string indexType;      // "btree", "gin", "gist", etc.
    std::string indexMethod;    // "", "trgm", "tsvector"
    std::string language;       // For tsvector, e.g., "english"
};

std::vector<ColumnDef> expectedColumns;
std::vector<IndexDef> expectedIndexes;

// Try to load from db_structure.ini config file
bool configLoaded = false;
std::ifstream configFile("db_structure.ini");
if (configFile.is_open()) {
    try {
        json config;
        configFile >> config;
        configFile.close();
        
        // Parse columns
        if (config.contains("columns") && config["columns"].is_array()) {
            for (const auto& col : config["columns"]) {
                ColumnDef colDef;
                colDef.name = col.value("name", "");
                colDef.dataType = col.value("dataType", "");
                colDef.maxLength = col.value("maxLength", 0);
                colDef.nullable = col.value("nullable", true);
                colDef.defaultValue = col.value("defaultValue", "");
                colDef.extraConstraints = col.value("extraConstraints", "");
                if (!colDef.name.empty() && !colDef.dataType.empty()) {
                    expectedColumns.push_back(colDef);
                }
            }
        }
        
        // Parse indexes
        if (config.contains("indexes") && config["indexes"].is_array()) {
            for (const auto& idx : config["indexes"]) {
                IndexDef idxDef;
                idxDef.columnName = idx.value("columnName", "");
                idxDef.indexType = idx.value("indexType", "btree");
                idxDef.indexMethod = idx.value("indexMethod", "");
                idxDef.language = idx.value("language", "");
                if (!idxDef.columnName.empty()) {
                    expectedIndexes.push_back(idxDef);
                }
            }
        }
        
        if (!expectedColumns.empty()) {
            configLoaded = true;
            PE_INFO("[PostgreSQL] Loaded db_structure.ini with " << expectedColumns.size() 
                   << " columns and " << expectedIndexes.size() << " indexes");
        }
    } catch (const std::exception& e) {
        PE_ERROR("[PostgreSQL] Failed to parse db_structure.ini: " << e.what());
    }
} else {
    PE_INFO("[PostgreSQL] db_structure.ini not found, using default schema definition");
}

// Use default values if config file not loaded or parsing failed
if (!configLoaded) {
    expectedColumns = {
        {"event_id",              "character varying", 255, false, "",       "PRIMARY KEY"},
        {"timestamp",             "timestamp without time zone", 0, false, "", ""},
        {"created_at",            "timestamp without time zone", 0, false, "", ""},
        {"device_id",             "character varying", 255, false, "",       ""},
        {"app_name",              "character varying", 255, false, "",       ""},
        {"window_title",          "text",              0,   true,  "",       ""},
        {"url",                   "text",              0,   true,  "",       ""},
        {"screen_content",        "text",              0,   true,  "",       ""},
        {"screen_content_hash",   "character varying", 64,  true,  "",       ""},
        {"similar_screen_content","text",              0,   true,  "",       ""},
        {"voice_transcription",   "text",              0,   true,  "",       ""},
        {"camera_description",    "text",              0,   true,  "",       ""},
        {"audio_content",         "text",              0,   true,  "",       ""},
        {"session_id",            "character varying", 255, true,  "",       ""},
        {"content_type",          "character varying", 50,  true,  "",       ""},
        {"domain",                "character varying", 50,  true,  "",       ""},
        {"interaction_count",     "integer",           0,   true,  "0",      ""},
        {"dwell_time_seconds",    "integer",           0,   true,  "0",      ""},
        {"compressed",            "boolean",           0,   true,  "false",  ""},
        {"summarized",            "boolean",           0,   true,  "false",  ""},
        {"mouse_events",          "jsonb",             0,   true,  "",       ""},
        {"system_info",           "jsonb",             0,   true,  "",       ""}
    };
    
    expectedIndexes = {
        {"timestamp",      "btree", "",        ""},
        {"app_name",       "btree", "",        ""},
        {"compressed",     "btree", "",        ""},
        {"summarized",     "btree", "",        ""},
        {"session_id",     "btree", "",        ""},
        {"screen_content", "gin",   "tsvector", "english"},
        {"screen_content", "gin",   "trgm",    ""},
        {"window_title",   "gin",   "trgm",    ""},
        {"app_name",       "gin",   "trgm",    ""},
        {"audio_content",  "gin",   "trgm",    ""}
    };
}
    
    // Helper lambda to build column definition string for CREATE TABLE
    // Note: Column names are quoted with double quotes to handle reserved words like "timestamp", "domain"
    auto buildColumnDef = [](const ColumnDef& col) -> std::string {
        std::string def = "\"" + col.name + "\" ";  // Quote column name
        
        if (col.dataType == "character varying" && col.maxLength > 0) {
            def += "VARCHAR(" + std::to_string(col.maxLength) + ")";
        } else if (col.dataType == "character varying") {
            def += "VARCHAR(255)";  // Default length
        } else if (col.dataType == "timestamp without time zone") {
            def += "TIMESTAMP";
        } else {
            // TEXT, INTEGER, BOOLEAN, JSONB, etc.
            std::string upperType = col.dataType;
            std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::toupper);
            def += upperType;
        }
        
        if (!col.extraConstraints.empty()) {
            def += " " + col.extraConstraints;
        }
        
        if (!col.nullable && col.extraConstraints.find("PRIMARY KEY") == std::string::npos) {
            def += " NOT NULL";
        }
        
        if (!col.defaultValue.empty()) {
            def += " DEFAULT " + col.defaultValue;
        }
        
        return def;
    };
    
    // Check if table exists
    bool tableExists = collectionExists(tableName);
    
    if (!tableExists) {
        // Create table with all fields
        std::ostringstream createTable;
        createTable << "CREATE TABLE IF NOT EXISTS public." << tableName << " (";
        
        for (size_t i = 0; i < expectedColumns.size(); ++i) {
            if (i > 0) createTable << ", ";
            createTable << buildColumnDef(expectedColumns[i]);
        }
        createTable << ");";
        
        if (!pImpl_->executeQuery(createTable.str())) {
            PE_ERROR("[PostgreSQL] Failed to create table: " << tableName);
            return false;
        }
        
        PE_INFO("[PostgreSQL] Created table: public." << tableName);
    } else {
        // Table exists - check schema and migrate if needed
        PE_INFO("[PostgreSQL] Table '" << tableName << "' exists, checking schema...");
        
        // Get detailed column information from the table
        std::string columnQuery = 
            "SELECT column_name, data_type, character_maximum_length, is_nullable, column_default "
            "FROM information_schema.columns "
            "WHERE table_name = '" + tableName + "' AND table_schema = 'public';";
        
        struct ExistingColumn {
            std::string dataType;
            int maxLength;
            bool nullable;
            std::string defaultValue;
        };
        
        PGresult* res = nullptr;
        std::map<std::string, ExistingColumn> existingColumns;
        
        if (pImpl_->executeQuery(columnQuery, &res)) {
            int rows = PQntuples(res);
            for (int i = 0; i < rows; ++i) {
                std::string colName = PQgetvalue(res, i, 0);
                ExistingColumn col;
                col.dataType = PQgetvalue(res, i, 1);
                col.maxLength = PQgetisnull(res, i, 2) ? 0 : std::atoi(PQgetvalue(res, i, 2));
                col.nullable = (std::string(PQgetvalue(res, i, 3)) == "YES");
                col.defaultValue = PQgetisnull(res, i, 4) ? "" : PQgetvalue(res, i, 4);
                existingColumns[colName] = col;
            }
            PQclear(res);
        }
        
        int addedColumns = 0;
        int modifiedColumns = 0;
        
        
        for (const auto& expected : expectedColumns) {
            auto it = existingColumns.find(expected.name);
            
            if (it == existingColumns.end()) {
                // Column doesn't exist - add it
                std::string colDef;
                
                if (expected.dataType == "character varying" && expected.maxLength > 0) {
                    colDef = "VARCHAR(" + std::to_string(expected.maxLength) + ")";
                } else if (expected.dataType == "character varying") {
                    colDef = "VARCHAR(255)";
                } else if (expected.dataType == "timestamp without time zone") {
                    colDef = "TIMESTAMP";
                } else {
                    std::string upperType = expected.dataType;
                    std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::toupper);
                    colDef = upperType;
                }
                
                // Add default if specified
                if (!expected.defaultValue.empty()) {
                    colDef += " DEFAULT " + expected.defaultValue;
                }
                
                // Skip PRIMARY KEY for ALTER TABLE (can't add via ALTER)
                // Skip NOT NULL for existing tables to avoid issues with existing NULL data
                
                // Quote column name to handle reserved words like "timestamp", "domain"
                std::string alterQuery = "ALTER TABLE " + tableName + 
                                        " ADD COLUMN IF NOT EXISTS \"" + expected.name + "\" " + colDef + ";";
                
                if (pImpl_->executeQuery(alterQuery)) {
                    PE_INFO("[PostgreSQL] Added missing column: " << expected.name << " (" << colDef << ")");
                    addedColumns++;
                } else {
                    PE_ERROR("[PostgreSQL] Failed to add column: " << expected.name);
                }
            } else {
                // Column exists - check if type/size/constraints match
                const ExistingColumn& existing = it->second;
                bool needsTypeChange = false;
                bool needsLengthChange = false;
                bool needsDefaultChange = false;
                
                // Check data type
                if (existing.dataType != expected.dataType) {
                    needsTypeChange = true;
                }
                
                // Check length for varchar
                if (expected.dataType == "character varying" && expected.maxLength > 0) {
                    if (existing.maxLength != expected.maxLength) {
                        needsLengthChange = true;
                    }
                }
                
                // Check default value (simplified comparison)
                // PostgreSQL stores defaults with type casts, e.g., "0" becomes "0" or "'0'::integer"
                if (!expected.defaultValue.empty() && existing.defaultValue.empty()) {
                    needsDefaultChange = true;
                }
                
                // Apply type/length changes
                if (needsTypeChange || needsLengthChange) {
                    std::string newType;
                    if (expected.dataType == "character varying" && expected.maxLength > 0) {
                        newType = "VARCHAR(" + std::to_string(expected.maxLength) + ")";
                    } else if (expected.dataType == "character varying") {
                        newType = "VARCHAR(255)";
                    } else if (expected.dataType == "timestamp without time zone") {
                        newType = "TIMESTAMP";
                    } else {
                        std::string upperType = expected.dataType;
                        std::transform(upperType.begin(), upperType.end(), upperType.begin(), ::toupper);
                        newType = upperType;
                    }
                    
                    // Use ALTER COLUMN ... TYPE with USING clause for safe conversion
                    // Quote column name to handle reserved words
                    std::string alterQuery = "ALTER TABLE " + tableName + 
                                            " ALTER COLUMN \"" + expected.name + "\"" +
                                            " TYPE " + newType + 
                                            " USING \"" + expected.name + "\"::" + newType + ";";
                    
                    if (pImpl_->executeQuery(alterQuery)) {
                        PE_INFO("[PostgreSQL] Modified column type: " << expected.name 
                               << " (" << existing.dataType << " -> " << newType << ")");
                        modifiedColumns++;
                    } else {
                        PE_ERROR("[PostgreSQL] Failed to modify column type: " << expected.name);
                    }
                }
                
                // Apply default value changes
                if (needsDefaultChange) {
                    // Quote column name to handle reserved words
                    std::string alterQuery = "ALTER TABLE " + tableName + 
                                            " ALTER COLUMN \"" + expected.name + "\"" +
                                            " SET DEFAULT " + expected.defaultValue + ";";
                    
                    if (pImpl_->executeQuery(alterQuery)) {
                        PE_INFO("[PostgreSQL] Set default value for column: " << expected.name 
                               << " = " << expected.defaultValue);
                        modifiedColumns++;
                    } else {
                        PE_ERROR("[PostgreSQL] Failed to set default for column: " << expected.name);
                    }
                }
                
                // Note: Changing nullable constraint is risky with existing data
                // We only log a warning if there's a mismatch
                if (existing.nullable != expected.nullable && !expected.nullable) {
                    PE_INFO("[PostgreSQL] Warning: Column '" << expected.name 
                           << "' should be NOT NULL but contains nullable data. Skipping constraint change.");
                }
            }
        }
        
        if (addedColumns > 0 || modifiedColumns > 0) {
            PE_INFO("[PostgreSQL] Schema migration complete. Added " << addedColumns 
                   << " column(s), modified " << modifiedColumns << " column(s).");
        } else {
            PE_INFO("[PostgreSQL] Schema is up to date.");
        }
        
        // Delete extra columns that are not in expectedColumns
        // Build a set of expected column names for quick lookup
        std::set<std::string> expectedColumnNames;
        for (const auto& col : expectedColumns) {
            expectedColumnNames.insert(col.name);
        }
        
        int deletedColumns = 0;
        for (const auto& existingCol : existingColumns) {
            if (expectedColumnNames.find(existingCol.first) == expectedColumnNames.end()) {
                // This column is not in the expected list - delete it
                std::string dropQuery = "ALTER TABLE " + tableName + 
                                       " DROP COLUMN IF EXISTS \"" + existingCol.first + "\";";
                
                if (pImpl_->executeQuery(dropQuery)) {
                    PE_INFO("[PostgreSQL] Deleted extra column: " << existingCol.first);
                    deletedColumns++;
                } else {
                    PE_ERROR("[PostgreSQL] Failed to delete column: " << existingCol.first);
                }
            }
        }
        
        if (deletedColumns > 0) {
            PE_INFO("[PostgreSQL] Deleted " << deletedColumns << " extra column(s).");
        }
    }
    
    // Build a set of expected index names for cleanup
    std::set<std::string> expectedIndexNames;
    
    // Build and execute index creation queries
    for (const auto& idx : expectedIndexes) {
        std::string indexName = "idx_" + tableName + "_" + idx.columnName;
        std::string indexQuery;
        std::string quotedColumn = "\"" + idx.columnName + "\"";  // Quote column name
        
        if (idx.indexMethod == "tsvector") {
            // Full-text search index
            indexName += "_fts";
            indexQuery = "CREATE INDEX IF NOT EXISTS " + indexName + 
                        " ON " + tableName + 
                        " USING " + idx.indexType + "(to_tsvector('" + idx.language + "', " + quotedColumn + "));";
        } else if (idx.indexMethod == "trgm") {
            // Trigram index for fuzzy search
            indexName += "_trgm";
            indexQuery = "CREATE INDEX IF NOT EXISTS " + indexName + 
                        " ON " + tableName + 
                        " USING " + idx.indexType + "(" + quotedColumn + " gin_trgm_ops);";
        } else {
            // Standard B-tree or other index
            if (idx.indexType == "btree") {
                indexQuery = "CREATE INDEX IF NOT EXISTS " + indexName + 
                            " ON " + tableName + "(" + quotedColumn + ");";
            } else {
                indexQuery = "CREATE INDEX IF NOT EXISTS " + indexName + 
                            " ON " + tableName + 
                            " USING " + idx.indexType + "(" + quotedColumn + ");";
            }
        }
        
        expectedIndexNames.insert(indexName);
        pImpl_->executeQuery(indexQuery);
    }
    
    // Delete extra indexes that are not in expectedIndexes
    // Query existing indexes for this table (excluding primary key and unique constraints)
    std::string indexListQuery = 
        "SELECT indexname FROM pg_indexes "
        "WHERE tablename = '" + tableName + "' "
        "AND schemaname = 'public' "
        "AND indexname LIKE 'idx_" + tableName + "_%';";
    
    PGresult* indexRes = nullptr;
    if (pImpl_->executeQuery(indexListQuery, &indexRes)) {
        int indexRows = PQntuples(indexRes);
        int deletedIndexes = 0;
        
        for (int i = 0; i < indexRows; ++i) {
            std::string existingIndexName = PQgetvalue(indexRes, i, 0);
            
            // Check if this index is in the expected list
            if (expectedIndexNames.find(existingIndexName) == expectedIndexNames.end()) {
                // This index is not in the expected list - delete it
                std::string dropIndexQuery = "DROP INDEX IF EXISTS \"" + existingIndexName + "\";";
                
                if (pImpl_->executeQuery(dropIndexQuery)) {
                    PE_INFO("[PostgreSQL] Deleted extra index: " << existingIndexName);
                    deletedIndexes++;
                } else {
                    PE_ERROR("[PostgreSQL] Failed to delete index: " << existingIndexName);
                }
            }
        }
        
        if (deletedIndexes > 0) {
            PE_INFO("[PostgreSQL] Deleted " << deletedIndexes << " extra index(es).");
        }
        
        PQclear(indexRes);
    }
    
    return true;
}

std::string PostgreSQLClient::indexDocument(const std::string& tableName, 
                                       const RawEvent& event) {
std::ostringstream query;
// Quote all column names to handle reserved words like "timestamp", "domain"
query << "INSERT INTO " << tableName << " ("
      << "\"event_id\", \"timestamp\", \"created_at\", \"device_id\", \"app_name\", "
      << "\"window_title\", \"url\", \"screen_content\", \"screen_content_hash\", \"similar_screen_content\", "
      << "\"voice_transcription\", \"camera_description\", \"audio_content\", \"session_id\", "
      << "\"content_type\", \"domain\", \"interaction_count\", \"dwell_time_seconds\", "
      << "\"compressed\", \"summarized\", \"mouse_events\", \"system_info\""
      << ") VALUES (";
    
    query << escapeString(pImpl_->conn_, event.eventId) << ", ";
    query << escapeString(pImpl_->conn_, timestampToPostgreSQL(event.timestamp)) << ", ";
    query << escapeString(pImpl_->conn_, timestampToPostgreSQL(event.createdAt)) << ", ";
    query << escapeString(pImpl_->conn_, event.deviceId) << ", ";
    query << escapeString(pImpl_->conn_, event.appName) << ", ";
    
    // Optional string fields
    query << (event.windowTitle ? escapeString(pImpl_->conn_, *event.windowTitle) : "NULL") << ", ";
    query << (event.url ? escapeString(pImpl_->conn_, *event.url) : "NULL") << ", ";
    query << (event.screenContent ? escapeString(pImpl_->conn_, *event.screenContent) : "NULL") << ", ";
    query << (event.screenContentHash ? escapeString(pImpl_->conn_, *event.screenContentHash) : "NULL") << ", ";
    query << (event.similarScreenContent ? escapeString(pImpl_->conn_, *event.similarScreenContent) : "NULL") << ", ";
    query << (event.voiceTranscription ? escapeString(pImpl_->conn_, *event.voiceTranscription) : "NULL") << ", ";
    query << (event.cameraDescription ? escapeString(pImpl_->conn_, *event.cameraDescription) : "NULL") << ", ";
    query << (event.audioContent ? escapeString(pImpl_->conn_, *event.audioContent) : "NULL") << ", ";
    query << (event.sessionId ? escapeString(pImpl_->conn_, *event.sessionId) : "NULL") << ", ";
    
    // Enum fields
    query << (event.contentType ? escapeString(pImpl_->conn_, contentTypeToString(*event.contentType)) : "NULL") << ", ";
    query << (event.domain ? escapeString(pImpl_->conn_, domainToString(*event.domain)) : "NULL") << ", ";
    
    // Integer and boolean fields
    query << event.interactionCount << ", ";
    query << event.dwellTimeSeconds << ", ";
    query << (event.compressed ? "TRUE" : "FALSE") << ", ";
    query << (event.summarized ? "TRUE" : "FALSE") << ", ";
    
    // Mouse events as JSONB
    if (!event.mouseEvents.empty()) {
        json mouseArray = json::array();
        for (const auto& me : event.mouseEvents) {
            mouseArray.push_back({
                {"timestamp", timestampToPostgreSQL(me.timestamp)},
                //{"event_type", me.eventType},
                {"selectedcontent", me.content}/*,
                {"pos_x", me.posX},
                {"pos_y", me.posY},
                {"element_type", me.elementType}*/
            });
        }
        query << escapeString(pImpl_->conn_, mouseArray.dump()) << ", ";
    } else {
        query << "NULL, ";
    }
    
    // System info as JSONB
    json sysInfo;
    if (event.systemInfo.batteryPercent) 
        sysInfo["battery_percent"] = *event.systemInfo.batteryPercent;
    sysInfo["is_charging"] = event.systemInfo.isCharging;
    sysInfo["network_type"] = event.systemInfo.networkType;
    if (event.systemInfo.locationLat) 
        sysInfo["location_lat"] = *event.systemInfo.locationLat;
    if (event.systemInfo.locationLon)
        sysInfo["location_lon"] = *event.systemInfo.locationLon;
    if (event.systemInfo.locationDescription)
        sysInfo["location_description"] = *event.systemInfo.locationDescription;
    if (event.systemInfo.cpuUsage) 
        sysInfo["cpu_usage"] = *event.systemInfo.cpuUsage;
    if (event.systemInfo.memoryUsage) 
        sysInfo["memory_usage"] = *event.systemInfo.memoryUsage;
    
    if (!sysInfo.empty()) {
        query << escapeString(pImpl_->conn_, sysInfo.dump());
    } else {
        query << "NULL";
    }
    
    query << ") ON CONFLICT (\"event_id\") DO UPDATE SET "
          << "\"timestamp\" = EXCLUDED.\"timestamp\", "
          << "\"screen_content\" = EXCLUDED.\"screen_content\";";
    
    if (pImpl_->executeQuery(query.str())) {
        return event.eventId;
    }
    return "";
}

bool PostgreSQLClient::bulkIndexDocuments(const std::string& tableName,
                                         const std::vector<RawEvent>& events) {
    if (events.empty()) return true;
    
    // Use transaction for bulk insert
    if (!pImpl_->executeQuery("BEGIN;")) {
        return false;
    }
    
    bool allSuccess = true;
    for (const auto& event : events) {
        if (indexDocument(tableName, event).empty()) {
            allSuccess = false;
            break;
        }
    }
    
    if (allSuccess) {
        return pImpl_->executeQuery("COMMIT;");
    } else {
        pImpl_->executeQuery("ROLLBACK;");
        return false;
    }
}

SearchResult PostgreSQLClient::search(const std::string& tableName,
                                      const std::string& query,
                                      int from,
                                      int size) {
    SearchResult result;

    auto searchStartTime = std::chrono::steady_clock::now();
    
    // Build SQL query from JSON query
    std::ostringstream sqlQuery;
    sqlQuery << "SELECT * FROM " << tableName;
    
    // NEW: Declare sortOrder outside try block so it's available for ORDER BY clause
    std::string sortOrder = "DESC";  // Default: newest first
    
    try {
        auto queryJson = json::parse(query);
        
        // Build WHERE clause
        std::vector<std::string> conditions;
        
        // ========================================
        // NEW: Handle simplified format
        // {"keyword": "...", "startTime": ..., "endTime": ..., "size": ...}
        // ========================================
        if (queryJson.contains("keyword") && !queryJson.contains("query")) {
            std::string keyword = queryJson["keyword"].get<std::string>();
            
            // DEBUG: Log keyword details
            PE_INFO("[DEBUG] Keyword search - Original keyword: " << keyword);
            PE_INFO("[DEBUG] Keyword length: " << keyword.length() << " bytes");
            
            // Apply multi-field search with case-insensitive matching for both ASCII and Unicode
            if (!keyword.empty()) {
                std::vector<std::string> searchFields = {
                    "\"screen_content\"",
                    "\"voice_transcription\"",
                    "\"camera_description\"",
                    "\"app_name\"",
                    "\"window_title\""
                };
                
                std::vector<std::string> fieldConditions;
                std::string escapedKeyword = escapeStringForSQL(pImpl_->conn_, keyword);
                
                // DEBUG: Log escaped keyword
                PE_INFO("[DEBUG] Escaped keyword: " << escapedKeyword);
                
                // Check if keyword contains non-ASCII characters (e.g., Chinese)
                bool hasNonASCII = false;
                for (char c : keyword) {
                    if (static_cast<unsigned char>(c) > 127) {
                        hasNonASCII = true;
                        break;
                    }
                }
                
                PE_INFO("[DEBUG] Has non-ASCII characters: " << (hasNonASCII ? "YES (Unicode/Chinese)" : "NO (ASCII)"));
                
                for (const auto& field : searchFields) {
                    if (hasNonASCII) {
                        // FIX: For Unicode (Chinese, etc.), use LIKE with original case
                        // This is more reliable for multi-byte characters
                        std::string likeCondition = "(" + field + " IS NOT NULL AND " + 
                                                   field + " LIKE '%" + escapedKeyword + "%')";
                        fieldConditions.push_back(likeCondition);
                    } else {
                        // For ASCII, use LOWER() for true case-insensitive matching
                        std::string lowerCondition = "(" + field + " IS NOT NULL AND " + 
                                                    "LOWER(" + field + ") LIKE LOWER('%" + escapedKeyword + "%'))";
                        fieldConditions.push_back(lowerCondition);
                    }
                }
                
                if (!fieldConditions.empty()) {
                    // Combine with OR (at least one field matches)
                    std::string combined = "(";
                    for (size_t i = 0; i < fieldConditions.size(); ++i) {
                        if (i > 0) combined += " OR ";
                        combined += fieldConditions[i];
                    }
                    combined += ")";
                    conditions.push_back(combined);
                    
                    // DEBUG: Log search condition
                    PE_INFO("[DEBUG] Search condition: " << combined.substr(0, 200) << "...");
                }
            }
            
            // Handle time range
            if (queryJson.contains("startTime") && queryJson.contains("endTime")) {
                long long startTime = queryJson["startTime"].get<long long>();
                long long endTime = queryJson["endTime"].get<long long>();
                
                // Convert milliseconds to seconds for to_timestamp()
                // Both storage and query use local time, so direct comparison works
                conditions.push_back("\"timestamp\" >= to_timestamp(" + std::to_string(startTime / 1000) + ")");
                conditions.push_back("\"timestamp\" <= to_timestamp(" + std::to_string(endTime / 1000) + ")");
                
                // DEBUG: Print time range for verification (in local time)
                std::time_t start_t = static_cast<std::time_t>(startTime / 1000);
                std::time_t end_t = static_cast<std::time_t>(endTime / 1000);
                
                char start_buf[100], end_buf[100];
                std::tm start_tm, end_tm;
                localtime_s(&start_tm, &start_t);
                localtime_s(&end_tm, &end_t);
                std::strftime(start_buf, sizeof(start_buf), "%Y-%m-%d %H:%M:%S", &start_tm);
                std::strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", &end_tm);
                
                PE_INFO("[DEBUG] Time range query (Local Time):");
                PE_INFO("  startTime: " << startTime << " ms -> " << start_buf);
                PE_INFO("  endTime:   " << endTime << " ms -> " << end_buf);
            }
            
            if (queryJson.contains("compressed")) {
                if(queryJson["compressed"].get<bool>()) {
                    conditions.push_back("\"compressed\" = TRUE");
                } else {
                    conditions.push_back("\"compressed\" = FALSE");
                }
            }
            
            if(queryJson.contains("summarized")) {
                if(queryJson["summarized"].get<bool>()) {
                    conditions.push_back("\"summarized\" = TRUE");
                } else {
                    conditions.push_back("\"summarized\" = FALSE");
                }
            }
            
            // Override size if specified in JSON
            if (queryJson.contains("size")) {
                size = queryJson["size"].get<int>();
            }
            
            // NEW: Handle sort order (default: DESC for newest first)
            if (queryJson.contains("sortOrder")) {
                std::string order = queryJson["sortOrder"].get<std::string>();
                // Convert to uppercase for comparison
                std::transform(order.begin(), order.end(), order.begin(), ::toupper);
                if (order == "ASC" || order == "ASCENDING") {
                    sortOrder = "ASC";  // Oldest first
                } else if (order == "DESC" || order == "DESCENDING") {
                    sortOrder = "DESC";  // Newest first
                }
            }
        }
        // ========================================
        // Existing: Handle full Elasticsearch DSL format
        // ========================================
        else if (queryJson.contains("query")) {
            auto q = queryJson["query"];
            
            // Handle match query
            if (q.contains("match")) {
                for (auto it = q["match"].begin(); it != q["match"].end(); ++it) {
                    std::string field = it.key();
                    std::string value = it.value().get<std::string>();
                    conditions.push_back(field + " ILIKE '%" + value + "%'");
                }
            }
            
            // Handle fuzzy query
            if (q.contains("fuzzy")) {
                for (auto it = q["fuzzy"].begin(); it != q["fuzzy"].end(); ++it) {
                    std::string field = it.key();
                    auto fuzzyConfig = it.value();
                    
                    std::string value;
                    double threshold = 0.3; // Default similar to Elasticsearch "AUTO"
                    
                    if (fuzzyConfig.is_string()) {
                        value = fuzzyConfig.get<std::string>();
                    } else if (fuzzyConfig.is_object()) {
                        if (fuzzyConfig.contains("value")) {
                            value = fuzzyConfig["value"].get<std::string>();
                        }
                        if (fuzzyConfig.contains("fuzziness")) {
                            auto fuzziness = fuzzyConfig["fuzziness"];
                            if (fuzziness.is_number()) {
                                threshold = fuzziness.get<double>();
                            } else if (fuzziness.is_string() && fuzziness.get<std::string>() == "AUTO") {
                                threshold = 0.3;
                            }
                        }
                    }
                    
                    if (!value.empty()) {
                        // Use similarity function with threshold
                        std::string escapedValue = escapeString(pImpl_->conn_, value);
                        conditions.push_back("similarity(" + field + ", " + escapedValue + ") > " + std::to_string(threshold));
                    }
                }
            }
            
            // Handle term query
            if (q.contains("term")) {
                for (auto it = q["term"].begin(); it != q["term"].end(); ++it) {
                    std::string field = it.key();
                    auto value = it.value();
                    if (value.is_boolean()) {
                        conditions.push_back(field + " = " + (value.get<bool>() ? "TRUE" : "FALSE"));
                    } else {
                        conditions.push_back(field + " = '" + value.get<std::string>() + "'");
                    }
                }
            }
            
            // Handle bool query
            if (q.contains("bool")) {
                auto boolQuery = q["bool"];
                
                if (boolQuery.contains("must")) {
                    for (const auto& mustClause : boolQuery["must"]) {
                        // Handle match
                        if (mustClause.contains("match")) {
                            for (auto it = mustClause["match"].begin(); it != mustClause["match"].end(); ++it) {
                                std::string field = it.key();
                                std::string value = it.value().get<std::string>();
                                conditions.push_back(field + " ILIKE '%" + value + "%'");
                            }
                        }
                        
                        // Handle multi_match
                        if (mustClause.contains("multi_match")) {
                            auto multiMatch = mustClause["multi_match"];
                            std::string queryText = multiMatch["query"].get<std::string>();
                            
                            if (multiMatch.contains("fields")) {
                                std::vector<std::string> fieldConditions;
                                
                                // Check if fuzzy matching is enabled
                                bool useFuzzy = false;
                                if (multiMatch.contains("fuzziness")) {
                                    auto fuzziness = multiMatch["fuzziness"];
                                    if (fuzziness.is_string() && fuzziness.get<std::string>() == "AUTO") {
                                        useFuzzy = true;
                                    } else if (fuzziness.is_number() && fuzziness.get<double>() > 0) {
                                        useFuzzy = true;
                                    }
                                }
                                
                                for (const auto& field : multiMatch["fields"]) {
                                    std::string fieldName = field.get<std::string>();
                                    
                                    if (useFuzzy) {
                                        // Use pg_trgm fuzzy matching with proper NULL handling
                                        std::string escapedQuery = escapeString(pImpl_->conn_, queryText);
                                        
                                        std::string fuzzyCondition = "(" + fieldName + " IS NOT NULL AND (";
                                        fuzzyCondition += fieldName + " % " + escapedQuery;
                                        fuzzyCondition += " OR word_similarity(" + escapedQuery + ", " + fieldName + ") > 0.3";
                                        fuzzyCondition += "))";
                                        fieldConditions.push_back(fuzzyCondition);
                                    } else {
                                        // Regular LIKE search (use LIKE instead of ILIKE for better Unicode support)
                                        std::string escapedQuery = escapeStringForSQL(pImpl_->conn_, queryText);
                                        
                                        // Check if query contains non-ASCII characters (e.g., Chinese)
                                        bool hasNonASCII = false;
                                        for (char c : queryText) {
                                            if (static_cast<unsigned char>(c) > 127) {
                                                hasNonASCII = true;
                                                break;
                                            }
                                        }
                                        
                                        if (hasNonASCII) {
                                            // Use LIKE for Unicode (case-sensitive but reliable)
                                            fieldConditions.push_back(
                                                "(" + fieldName + " IS NOT NULL AND " + 
                                                fieldName + " LIKE '%" + escapedQuery + "%')"
                                            );
                                        } else {
                                            // Use ILIKE for ASCII (case-insensitive)
                                            fieldConditions.push_back(
                                                "(" + fieldName + " IS NOT NULL AND " + 
                                                fieldName + " ILIKE '%" + escapedQuery + "%')"
                                            );
                                        }
                                    }
                                }
                                
                                if (!fieldConditions.empty()) {
                                    // Combine field conditions with OR
                                    std::string combined = "(";
                                    for (size_t i = 0; i < fieldConditions.size(); ++i) {
                                        if (i > 0) combined += " OR ";
                                        combined += fieldConditions[i];
                                    }
                                    combined += ")";
                                    conditions.push_back(combined);
                                }
                            }
                        }
                    }
                }
                
                if (boolQuery.contains("filter")) {
                    for (const auto& filterClause : boolQuery["filter"]) {
                        if (filterClause.contains("term")) {
                            for (auto it = filterClause["term"].begin(); it != filterClause["term"].end(); ++it) {
                                std::string field = it.key();
                                auto value = it.value();
                                if (value.is_boolean()) {
                                    conditions.push_back(field + " = " + (value.get<bool>() ? "TRUE" : "FALSE"));
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (!conditions.empty()) {
            sqlQuery << " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) sqlQuery << " AND ";
                sqlQuery << conditions[i];
            }
        }
    } catch (...) {
        // If JSON parsing fails, treat as raw SQL WHERE clause
        if (!query.empty()) {
            sqlQuery << " WHERE " << query;
        }
    }
    
    // Add ORDER BY and LIMIT
    // Use sortOrder variable determined earlier (default: DESC)
    sqlQuery << " ORDER BY \"timestamp\" " << sortOrder << " LIMIT " << size << " OFFSET " << from << ";";
    
    // DEBUG: Print generated SQL
    PE_INFO("[DEBUG] Generated SQL: " << sqlQuery.str());
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(sqlQuery.str(), &res)) {
        return result;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        result.events.push_back(pImpl_->resultToEvent(res, i));
    }
    result.totalHits = rows;

    auto searchEndTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEndTime - searchStartTime);
    result.searchTimeMs = static_cast<long>(duration.count());
    PE_INFO("[DEBUG] PostgreSQL Search completed in " << result.searchTimeMs << " ms");

    PQclear(res);
    return result;
}

std::vector<RawEvent> PostgreSQLClient::getUncompressedEvents(
const std::string& tableName, 
int max_count) {
    
std::ostringstream query;
query << "SELECT * FROM " << tableName 
      << " WHERE \"compressed\" = FALSE "
      << "ORDER BY \"timestamp\" ASC "
      << "LIMIT " << max_count << ";";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        return {};
    }
    
    std::vector<RawEvent> events;
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        events.push_back(pImpl_->resultToEvent(res, i));
    }
    
    PQclear(res);
    return events;
}

bool PostgreSQLClient::updateDocument(const std::string& tableName,
                                     const std::string& docId,
                                     const std::string& updateData) {
    // Parse update JSON
    try {
        auto updateJson = json::parse(updateData);
        
        if (!updateJson.contains("doc")) {
            return false;
        }
        
        auto doc = updateJson["doc"];
        std::ostringstream query;
        query << "UPDATE " << tableName << " SET ";
        
        std::vector<std::string> updates;
        for (auto it = doc.begin(); it != doc.end(); ++it) {
            std::string field = it.key();
            auto value = it.value();
            
            if (value.is_boolean()) {
                updates.push_back(field + " = " + (value.get<bool>() ? "TRUE" : "FALSE"));
            } else if (value.is_number()) {
                updates.push_back(field + " = " + std::to_string(value.get<double>()));
            } else if (value.is_string()) {
                updates.push_back(field + " = " + escapeString(pImpl_->conn_, value.get<std::string>()));
            }
        }
        
        if (updates.empty()) {
            return false;
        }
        
        for (size_t i = 0; i < updates.size(); ++i) {
            if (i > 0) query << ", ";
            query << updates[i];
        }
        
        query << " WHERE \"event_id\" = " << escapeString(pImpl_->conn_, docId) << ";";
        
        return pImpl_->executeQuery(query.str());
    } catch (...) {
        return false;
    }
}

bool PostgreSQLClient::markEventsAsCompressed(const std::string& tableName,
                                             const std::vector<std::string>& eventIds,
                                             const std::string& sessionId) {
    if (eventIds.empty()) return true;
    
    std::ostringstream query;
    query << "UPDATE " << tableName 
          << " SET \"compressed\" = TRUE, \"session_id\" = " << escapeString(pImpl_->conn_, sessionId)
          << " WHERE \"event_id\" IN (";
    
    for (size_t i = 0; i < eventIds.size(); ++i) {
        if (i > 0) query << ", ";
        query << escapeString(pImpl_->conn_, eventIds[i]);
    }
    
    query << ");";
    
    return pImpl_->executeQuery(query.str());
}

bool PostgreSQLClient::markEventsAsCompressedWithSimilarity(
    const std::string& tableName,
    const std::vector<std::string>& eventIds,
    const std::string& sessionId,
    const std::string& similarScreenContent) {
    
    if (eventIds.empty()) return true;
    
    std::ostringstream query;
    query << "UPDATE " << tableName 
          << " SET \"compressed\" = TRUE, "
          << "\"session_id\" = " << escapeString(pImpl_->conn_, sessionId) << ", "
          << "\"similar_screen_content\" = " << escapeString(pImpl_->conn_, similarScreenContent)
          << " WHERE \"event_id\" IN (";
    
    for (size_t i = 0; i < eventIds.size(); ++i) {
        if (i > 0) query << ", ";
        query << escapeString(pImpl_->conn_, eventIds[i]);
    }
    
    query << ");";
    
    return pImpl_->executeQuery(query.str());
}

int PostgreSQLClient::deleteOlderThan(const std::string& tableName,
                                     std::time_t cutoffTime) {
    std::ostringstream query;
    query << "DELETE FROM " << tableName
          << " WHERE \"timestamp\" < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
          << ";";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        return 0;
    }
    
    char* rowsAffected = PQcmdTuples(res);
    int deleted = std::atoi(rowsAffected);
    
    PQclear(res);
    return deleted;
}

int PostgreSQLClient::findOlderThan(std::string tableName,
    std::time_t cutoffTime, SearchResult& result)
{
    std::ostringstream query;
    query << "SELECT * FROM " << tableName
        << " WHERE \"timestamp\" < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
        << ";";
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        return 0;
    }

	int rows = PQntuples(res);
	for (int i = 0; i < rows; ++i) {
		result.events.push_back(pImpl_->resultToEvent(res, i));
	}
	result.totalHits = rows;

    return rows;
}

int PostgreSQLClient::countOlderThan(std::string tableName,
    std::time_t cutoffTime)
{
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM " << tableName
        << " WHERE \"timestamp\" < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
        << ";";
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        return 0;
    }

    int count = 0;
    if (PQntuples(res) > 0) {
        count = std::atoi(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return count;
}

bool PostgreSQLClient::refreshCollection(const std::string& tableName) {
    // PostgreSQL doesn't need explicit refresh like Elasticsearch
    // Just run ANALYZE to update statistics
    return pImpl_->executeQuery("ANALYZE " + tableName + ";");
}

CollectionStats PostgreSQLClient::getCollectionStats(const std::string& tableName) {
    CollectionStats stats;
    stats.collectionName = tableName;
    
    stats.documentCount = getDocumentCount(tableName);
    stats.uncompressedCount = getUncompressedCount(tableName);
    stats.compressedCount = stats.documentCount - stats.uncompressedCount;
    
    // Get table size
    std::string sizeQuery = "SELECT pg_total_relation_size('" + tableName + "') AS size;";
    PGresult* res = nullptr;
    if (pImpl_->executeQuery(sizeQuery, &res)) {
        if (PQntuples(res) > 0) {
            stats.sizeInBytes = std::atoll(PQgetvalue(res, 0, 0));
        }
        PQclear(res);
    }
    
    return stats;
}

int PostgreSQLClient::getDocumentCount(const std::string& tableName) {
    std::string query = "SELECT COUNT(*) FROM " + tableName + ";";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query, &res)) {
        return 0;
    }
    
    int count = 0;
    if (PQntuples(res) > 0) {
        count = std::atoi(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return count;
}

int PostgreSQLClient::getUncompressedCount(const std::string& tableName) {
std::string query = "SELECT COUNT(*) FROM " + tableName + " WHERE \"compressed\" = FALSE;";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query, &res)) {
        return 0;
    }
    
    int count = 0;
    if (PQntuples(res) > 0) {
        count = std::atoi(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return count;
}

bool PostgreSQLClient::deleteCollection(const std::string& tableName) {
    return pImpl_->executeQuery("DROP TABLE IF EXISTS " + tableName + " CASCADE;");
}

bool PostgreSQLClient::collectionExists(const std::string& tableName) {
    std::string query = "SELECT EXISTS (SELECT FROM information_schema.tables WHERE table_name = '" 
                       + tableName + "');";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query, &res)) {
        return false;
    }
    
    bool exists = false;
    if (PQntuples(res) > 0) {
        exists = (std::string(PQgetvalue(res, 0, 0)) == "t");
    }
    
    PQclear(res);
    return exists;
}

bool PostgreSQLClient::testConnection() {
    return pImpl_->conn_ != nullptr && PQstatus(pImpl_->conn_) == CONNECTION_OK;
}

std::string PostgreSQLClient::getServerInfo() {
    json info;
    
    if (!testConnection()) {
        info["status"] = "disconnected";
        return info.dump(2);
    }
    
    info["status"] = "connected";
    info["database_type"] = "PostgreSQL";
    info["server_version"] = PQparameterStatus(pImpl_->conn_, "server_version");
    info["client_encoding"] = PQparameterStatus(pImpl_->conn_, "client_encoding");
    info["database"] = PQdb(pImpl_->conn_);
    info["user"] = PQuser(pImpl_->conn_);
    info["host"] = PQhost(pImpl_->conn_);
    info["port"] = PQport(pImpl_->conn_);
    
    return info.dump(2);
}

RawEvent PostgreSQLClient::getDocumentByAppName(const std::string& tableName, 
                                                const std::string& appName) {
    std::string query = "SELECT * FROM " + tableName + 
                       " WHERE \"app_name\" = " + escapeString(pImpl_->conn_, appName) + 
                       " LIMIT 1;";
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query, &res)) {
        return RawEvent();
    }
    
    RawEvent event;
    if (PQntuples(res) > 0) {
        event = pImpl_->resultToEvent(res, 0);
    }
    
    PQclear(res);
    return event;
}

bool PostgreSQLClient::deleteDocumentByAppName(const std::string& tableName, 
                                              const std::string& appName) {
    std::string query = "DELETE FROM " + tableName + 
                       " WHERE \"app_name\" = " + escapeString(pImpl_->conn_, appName) + ";";
    
    return pImpl_->executeQuery(query);
}

SearchResult PostgreSQLClient::fuzzySearch(
    const std::string& tableName,
    const std::string& field,
    const std::string& searchValue,
    double similarityThreshold,
    int from,
    int size) {
    
    SearchResult result;

    auto searchStartTime = std::chrono::steady_clock::now();
    
    // Build fuzzy search query using pg_trgm similarity
    // Use both % operator and word_similarity for better results
    std::ostringstream query;
    query << "SELECT *, "
          << "GREATEST(similarity(" << field << ", " << escapeString(pImpl_->conn_, searchValue) << "), "
          << "word_similarity(" << escapeString(pImpl_->conn_, searchValue) << ", " << field << ")) AS sim_score "
          << "FROM " << tableName 
          << " WHERE " << field << " IS NOT NULL AND ("
          << field << " % " << escapeString(pImpl_->conn_, searchValue)
          << " OR word_similarity(" << escapeString(pImpl_->conn_, searchValue) << ", " << field << ") > " << similarityThreshold
          << ")"
          << " ORDER BY sim_score DESC"
          << " LIMIT " << size << " OFFSET " << from << ";";
    
    // DEBUG: Print generated SQL
    PE_INFO("[DEBUG fuzzySearch] SQL: " << query.str());
    
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        PE_ERROR("[ERROR fuzzySearch] Query failed!");
        return result;
    }
    
    int rows = PQntuples(res);
    PE_INFO("[DEBUG fuzzySearch] Returned " << rows << " rows");
    
    for (int i = 0; i < rows; ++i) {
        result.events.push_back(pImpl_->resultToEvent(res, i));
    }
    result.totalHits = rows;

    auto searchEndTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEndTime - searchStartTime);
    result.searchTimeMs = static_cast<long>(duration.count());
    PE_INFO("[DEBUG] PostgreSQL Search completed in " << result.searchTimeMs << " ms");

    PQclear(res);
    return result;
}

bool PostgreSQLClient::ensureDatabaseExists() {
    // Parse database name from connection string
    std::string dbName = pImpl_->parseDbName();
    
    if (dbName.empty()) {
        PE_ERROR("[PostgreSQL] Failed to parse database name from connection string");
        return false;
    }
    
    // If database is 'postgres', assume it exists
    if (dbName == "postgres") {
        PE_INFO("[PostgreSQL] Using default 'postgres' database");
        return true;
    }
    
    // Try to connect to the target database
    PE_INFO("[PostgreSQL] Attempting to connect to database: " << dbName);
    
    if (pImpl_->connect()) {
        PE_INFO("[PostgreSQL] ✓ Successfully connected to database: " << dbName);
        return true;
    }
    
    // Connection failed - database might not exist
    PE_INFO("\n========================================");
    PE_INFO("DATABASE '" << dbName << "' MAY NOT EXIST");
    PE_INFO("========================================");
    PE_INFO("\nAttempting to create database automatically...\n");
    
    // Build connection string to 'postgres' database (always exists)
    std::string postgresConnStr = pImpl_->buildPostgresConnStr();
    
    // Create temporary connection to 'postgres' database
    PE_INFO("[PostgreSQL] Connecting to 'postgres' database...");
    PGconn* tempConn = PQconnectdb(postgresConnStr.c_str());
    
    if (PQstatus(tempConn) != CONNECTION_OK) {
        PE_ERROR("[PostgreSQL] Failed to connect to 'postgres' database: " << PQerrorMessage(tempConn));
        PE_ERROR("\nPlease create the database manually:");
        PE_ERROR("  psql -h 127.0.0.1 -p 5432 -U postgres -d postgres");
        PE_ERROR("  CREATE DATABASE " << dbName << " WITH ENCODING 'UTF8';");
        PE_ERROR("  \\q");
        
        PQfinish(tempConn);
        return false;
    }
    
    PE_INFO("[PostgreSQL] ✓ Successfully connected to 'postgres' database");
    
    // Execute CREATE DATABASE command
    std::string createDbQuery = "CREATE DATABASE " + dbName + " WITH ENCODING 'UTF8'";
    PE_INFO("[PostgreSQL] Executing: " << createDbQuery);
    
    PGresult* res = PQexec(tempConn, createDbQuery.c_str());
    bool dbCreated = false;
    
    if (res) {
        ExecStatusType status = PQresultStatus(res);
        dbCreated = (status == PGRES_COMMAND_OK);
        
        if (!dbCreated) {
            std::string errorMsg = PQerrorMessage(tempConn);
            // Check if error is "database already exists"
            if (errorMsg.find("already exists") != std::string::npos) {
                PE_INFO("[PostgreSQL] Database already exists (created by another process)");
                dbCreated = true;
            } else {
                PE_ERROR("[PostgreSQL] Failed to create database: " << errorMsg);
            }
        }
        
        PQclear(res);
    }
    
    // Close temporary connection
    PQfinish(tempConn);
    
    if (!dbCreated) {
        PE_ERROR("\nPlease create the database manually:");
        PE_ERROR("  psql -h 127.0.0.1 -p 5432 -U postgres -d postgres -c \"CREATE DATABASE " << dbName << " WITH ENCODING 'UTF8';\"");
        return false;
    }
    
    PE_INFO("[PostgreSQL] ✓ Database '" << dbName << "' created successfully!");
    
    // Now reconnect to the newly created database
    PE_INFO("[PostgreSQL] Reconnecting to new database...");
    
    if (!pImpl_->connect()) {
        PE_ERROR("[PostgreSQL] Failed to reconnect to newly created database");
        return false;
    }
    
    PE_INFO("[PostgreSQL] ✓ Successfully reconnected to database: " << dbName);
    return true;
}

} // namespace database
