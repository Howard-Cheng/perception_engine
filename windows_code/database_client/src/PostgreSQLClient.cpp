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
        }
        
        conn_ = PQconnectdb(connectionString_.c_str());
        
        if (PQstatus(conn_) != CONNECTION_OK) {
            PE_ERROR("PostgreSQL connection failed: " << PQerrorMessage(conn_));
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
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
                    if (meJson.contains("event_type")) {
                        me.eventType = meJson["event_type"].get<std::string>();
                    }
                    if (meJson.contains("content")) {
                        me.content = meJson["content"].get<std::string>();
                    }
                    if (meJson.contains("pos_x")) {
                        me.posX = meJson["pos_x"].get<int>();
                    }
                    if (meJson.contains("pos_y")) {
                        me.posY = meJson["pos_y"].get<int>();
                    }
                    if (meJson.contains("element_type")) {
                        me.elementType = meJson["element_type"].get<std::string>();
                    }
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
    // Enable pg_trgm extension for fuzzy search (if not already enabled)
    pImpl_->executeQuery("CREATE EXTENSION IF NOT EXISTS pg_trgm;");
    
    // Create table with all fields
    std::ostringstream createTable;
    createTable << "CREATE TABLE IF NOT EXISTS " << tableName << " ("
                << "event_id VARCHAR(255) PRIMARY KEY,"
                << "timestamp TIMESTAMP NOT NULL,"
                << "created_at TIMESTAMP NOT NULL,"
                << "device_id VARCHAR(255) NOT NULL,"
                << "app_name VARCHAR(255) NOT NULL,"
                << "window_title TEXT,"
                << "url TEXT,"
                << "screen_content TEXT,"
                << "screen_content_hash VARCHAR(64),"
                << "similar_screen_content TEXT,"
                << "voice_transcription TEXT,"
                << "camera_description TEXT,"
                << "session_id VARCHAR(255),"
                << "content_type VARCHAR(50),"
                << "domain VARCHAR(50),"
                << "interaction_count INTEGER DEFAULT 0,"
                << "dwell_time_seconds INTEGER DEFAULT 0,"
                << "compressed BOOLEAN DEFAULT FALSE,"
                << "summarized BOOLEAN DEFAULT FALSE,"
                << "mouse_events JSONB,"
                << "system_info JSONB"
                << ");";
    
    if (!pImpl_->executeQuery(createTable.str())) {
        return false;
    }
    
    // Create indexes for common queries
    std::vector<std::string> indexes = {
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_timestamp ON " + tableName + "(timestamp);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_app_name ON " + tableName + "(app_name);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_compressed ON " + tableName + "(compressed);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_summarized ON " + tableName + "(summarized);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_session_id ON " + tableName + "(session_id);",
        // Full-text search index
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_screen_content ON " + tableName + " USING gin(to_tsvector('english', screen_content));",
        // Trigram indexes for fuzzy search
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_screen_content_trgm ON " + tableName + " USING gin(screen_content gin_trgm_ops);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_window_title_trgm ON " + tableName + " USING gin(window_title gin_trgm_ops);",
        "CREATE INDEX IF NOT EXISTS idx_" + tableName + "_app_name_trgm ON " + tableName + " USING gin(app_name gin_trgm_ops);"
    };
    
    for (const auto& indexQuery : indexes) {
        pImpl_->executeQuery(indexQuery);
    }
    
    return true;
}

std::string PostgreSQLClient::indexDocument(const std::string& tableName, 
                                           const RawEvent& event) {
    std::ostringstream query;
    query << "INSERT INTO " << tableName << " ("
          << "event_id, timestamp, created_at, device_id, app_name, "
          << "window_title, url, screen_content, screen_content_hash, similar_screen_content, "
          << "voice_transcription, camera_description, session_id, "
          << "content_type, domain, interaction_count, dwell_time_seconds, "
          << "compressed, summarized, mouse_events, system_info"
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
                {"event_type", me.eventType},
                {"content", me.content},
                {"pos_x", me.posX},
                {"pos_y", me.posY},
                {"element_type", me.elementType}
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
    if (event.systemInfo.cpuUsage) 
        sysInfo["cpu_usage"] = *event.systemInfo.cpuUsage;
    if (event.systemInfo.memoryUsage) 
        sysInfo["memory_usage"] = *event.systemInfo.memoryUsage;
    
    if (!sysInfo.empty()) {
        query << escapeString(pImpl_->conn_, sysInfo.dump());
    } else {
        query << "NULL";
    }
    
    query << ") ON CONFLICT (event_id) DO UPDATE SET "
          << "timestamp = EXCLUDED.timestamp, "
          << "screen_content = EXCLUDED.screen_content;";
    
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
                    "screen_content",
                    "voice_transcription",
                    "camera_description",
                    "app_name",
                    "window_title"
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
                conditions.push_back("timestamp >= to_timestamp(" + std::to_string(startTime / 1000) + ")");
                conditions.push_back("timestamp <= to_timestamp(" + std::to_string(endTime / 1000) + ")");
                
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
            
            // Handle compressed filter (default: only uncompressed)
            if (!queryJson.contains("includeCompressed") || !queryJson["includeCompressed"].get<bool>()) {
                conditions.push_back("compressed = FALSE");
            }
            
            // NEW: Handle summarized filter (default: only unsummarized)
            if (!queryJson.contains("includeSummarized") || !queryJson["includeSummarized"].get<bool>()) {
                conditions.push_back("summarized = FALSE");
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
    sqlQuery << " ORDER BY timestamp " << sortOrder << " LIMIT " << size << " OFFSET " << from << ";";
    
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
          << " WHERE compressed = FALSE "
          << "ORDER BY timestamp ASC "
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
        
        query << " WHERE event_id = " << escapeString(pImpl_->conn_, docId) << ";";
        
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
          << " SET compressed = TRUE, session_id = " << escapeString(pImpl_->conn_, sessionId)
          << " WHERE event_id IN (";
    
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
          << " SET compressed = TRUE, "
          << "session_id = " << escapeString(pImpl_->conn_, sessionId) << ", "
          << "similar_screen_content = " << escapeString(pImpl_->conn_, similarScreenContent)
          << " WHERE event_id IN (";
    
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
          << " WHERE timestamp < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
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
    query << "SELECT FROM " << tableName
        << " WHERE timestamp < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
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
    query << "SELECT FROM " << tableName
        << " WHERE timestamp < " << escapeString(pImpl_->conn_, timestampToPostgreSQL(cutoffTime))
        << ";";
    PGresult* res = nullptr;
    if (!pImpl_->executeQuery(query.str(), &res)) {
        return 0;
    }

    int rows = PQntuples(res);
    

    return rows;
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
    std::string query = "SELECT COUNT(*) FROM " + tableName + " WHERE compressed = FALSE;";
    
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
                       " WHERE app_name = " + escapeString(pImpl_->conn_, appName) + 
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
                       " WHERE app_name = " + escapeString(pImpl_->conn_, appName) + ";";
    
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
