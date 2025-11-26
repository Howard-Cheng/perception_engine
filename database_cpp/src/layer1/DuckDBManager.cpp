#include "layer1/DuckDBManager.h"
#include "layer1/CompressionPipeline.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <duckdb.hpp>
#include <sstream>
#include <iomanip>

namespace perception {
namespace layer1 {

DuckDBManager::DuckDBManager(const std::string& dbPath)
    : dbPath_(dbPath) {
    
    LOG_INFO("Initializing DuckDB Manager");
    LOG_INFO("Database path: " + dbPath);
    
    try {
        // Create DuckDB database
        db_ = std::make_unique<duckdb::DuckDB>(dbPath.c_str());
        conn_ = std::make_unique<duckdb::Connection>(*db_);
        
        // Initialize schema
        initializeSchema();
        
        LOG_INFO("DuckDB Manager initialized successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize DuckDB: " + std::string(e.what()));
        throw;
    }
}

DuckDBManager::~DuckDBManager() {
    try {
        conn_.reset();
        db_.reset();
        LOG_INFO("DuckDB connection closed");
    } catch (...) {
        // Ignore exceptions in destructor
    }
}

void DuckDBManager::initializeSchema() {
    try {
        LOG_INFO("Creating compressed_sessions table schema");
        
        // Simpler CREATE TABLE statement without comments
        const std::string createTableSQL = 
            "CREATE TABLE IF NOT EXISTS compressed_sessions ("
            "session_id VARCHAR PRIMARY KEY, "
            "device_id VARCHAR NOT NULL, "
            "start_time TIMESTAMP NOT NULL, "
            "end_time TIMESTAMP NOT NULL, "
            "content_type VARCHAR, "
            "domain VARCHAR, "
            "app_name VARCHAR NOT NULL, "
            "window_title VARCHAR, "
            "url VARCHAR, "
            "engagement_score DOUBLE, "
            "interaction_count INTEGER, "
            "total_dwell_time INTEGER, "
            "has_copied BOOLEAN, "
            "has_selected BOOLEAN, "
            "compressed_summary VARCHAR, "
            "key_points VARCHAR, "
            "summary_token_count INTEGER, "
            "copied_content VARCHAR, "
            "selected_text VARCHAR, "
            "numbers VARCHAR, "
            "dates VARCHAR, "
            "urls VARCHAR, "
            "emails VARCHAR, "
            "metadata_json VARCHAR, "
            "avg_cpu_usage DOUBLE, "
            "avg_memory_usage DOUBLE, "
            "avg_battery_percent INTEGER, "
            "created_at TIMESTAMP, "
            "compressed_at TIMESTAMP"
            ")";
        
        LOG_INFO("Executing CREATE TABLE statement...");
        try {
            auto result = conn_->Query(createTableSQL);
            if (result->HasError()) {
                LOG_ERROR("CREATE TABLE failed: " + result->GetError());
                throw std::runtime_error("Failed to create table: " + result->GetError());
            }
            LOG_INFO("? Table created successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during CREATE TABLE: " + std::string(e.what()));
            throw;
        }
        
        // Create indexes one by one with error checking
        LOG_INFO("Creating indexes...");
        
        try {
            LOG_INFO("Creating idx_device_time...");
            auto result = conn_->Query("CREATE INDEX IF NOT EXISTS idx_device_time ON compressed_sessions(device_id, start_time)");
            if (result->HasError()) {
                LOG_WARNING("CREATE INDEX idx_device_time failed: " + result->GetError());
            } else {
                LOG_INFO("? idx_device_time created");
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Exception creating idx_device_time: " + std::string(e.what()));
        }
        
        try {
            LOG_INFO("Creating idx_content_type...");
            auto result = conn_->Query("CREATE INDEX IF NOT EXISTS idx_content_type ON compressed_sessions(content_type)");
            if (result->HasError()) {
                LOG_WARNING("CREATE INDEX idx_content_type failed: " + result->GetError());
            } else {
                LOG_INFO("? idx_content_type created");
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Exception creating idx_content_type: " + std::string(e.what()));
        }
        
        try {
            LOG_INFO("Creating idx_engagement...");
            auto result = conn_->Query("CREATE INDEX IF NOT EXISTS idx_engagement ON compressed_sessions(engagement_score)");
            if (result->HasError()) {
                LOG_WARNING("CREATE INDEX idx_engagement failed: " + result->GetError());
            } else {
                LOG_INFO("? idx_engagement created");
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Exception creating idx_engagement: " + std::string(e.what()));
        }
        
        try {
            LOG_INFO("Creating idx_compressed_at...");
            auto result = conn_->Query("CREATE INDEX IF NOT EXISTS idx_compressed_at ON compressed_sessions(compressed_at)");
            if (result->HasError()) {
                LOG_WARNING("CREATE INDEX idx_compressed_at failed: " + result->GetError());
            } else {
                LOG_INFO("? idx_compressed_at created");
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Exception creating idx_compressed_at: " + std::string(e.what()));
        }
        
        LOG_INFO("Schema initialized successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize schema: " + std::string(e.what()));
        throw;
    }
}

std::string DuckDBManager::insertCompressedSession(const CompressedSession& session) {
    try {
        std::ostringstream sql;
        sql << "INSERT INTO compressed_sessions VALUES (";
        sql << "'" << session.sessionId << "', ";  // session_id
        sql << "'" << session.deviceId << "', ";    // device_id
        sql << "TIMESTAMP '" << utils::timestampToString(session.startTime) << "', ";  // start_time
        sql << "TIMESTAMP '" << utils::timestampToString(session.endTime) << "', ";    // end_time
        sql << "'" << ContentTypeToString(session.contentType) << "', ";  // content_type
        sql << "'" << DomainToString(session.domain) << "', ";       // domain
        sql << "'" << session.appName << "', ";      // app_name
        
        // Handle optional fields
        if (session.windowTitle.has_value()) {
            sql << "'" << session.windowTitle.value() << "', ";
        } else {
            sql << "NULL, ";
        }
        
        if (session.url.has_value()) {
            sql << "'" << session.url.value() << "', ";
        } else {
            sql << "NULL, ";
        }
        
        // Engagement metrics
        sql << session.engagementScore << ", ";
        sql << session.interactionCount << ", ";
        sql << session.totalDwellTime << ", ";
        sql << (session.hasCopied ? "TRUE" : "FALSE") << ", ";
        sql << (session.hasSelected ? "TRUE" : "FALSE") << ", ";
        
        // Compressed content
        sql << "'" << session.compressedSummary << "', ";
        
        // Convert key_points to JSON array
        sql << "'[";
        for (size_t i = 0; i < session.keyPoints.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << "\"" << session.keyPoints[i] << "\"";
        }
        sql << "]', ";
        
        sql << session.summaryTokenCount << ", ";
        
        // Convert arrays to JSON
        auto arrayToJson = [](const std::vector<std::string>& arr) {
            std::ostringstream json;
            json << "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) json << ", ";
                json << "\"" << arr[i] << "\"";
            }
            json << "]";
            return json.str();
        };
        
        sql << "'" << arrayToJson(session.copiedContent) << "', ";
        sql << "'" << arrayToJson(session.selectedText) << "', ";
        sql << "'" << arrayToJson(session.numbers) << "', ";
        sql << "'" << arrayToJson(session.dates) << "', ";
        sql << "'" << arrayToJson(session.urls) << "', ";
        sql << "'" << arrayToJson(session.emails) << "', ";
        
        // Metadata JSON
        sql << "'" << session.metadataJson << "', ";
        
        // System info
        sql << session.avgCpuUsage << ", ";
        sql << session.avgMemoryUsage << ", ";
        sql << session.avgBatteryPercent << ", ";
        
        // Timestamps
        sql << "TIMESTAMP '" << utils::timestampToString(session.createdAt) << "', ";
        sql << "TIMESTAMP '" << utils::timestampToString(session.compressedAt) << "'";
        
        sql << ")";
        
        conn_->Query(sql.str());
        
        LOG_DEBUG("Inserted compressed session: " + session.sessionId);
        
        return session.sessionId;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to insert compressed session: " + std::string(e.what()));
        throw;
    }
}

std::vector<CompressedSession> DuckDBManager::querySessionsByTimeRange(
    const Timestamp& startTime,
    const Timestamp& endTime) {
    
    std::vector<CompressedSession> results;
    
    try {
        std::ostringstream sql;
        sql << "SELECT * FROM compressed_sessions WHERE ";
        sql << "start_time >= TIMESTAMP '" << utils::timestampToString(startTime) << "' AND ";
        sql << "start_time <= TIMESTAMP '" << utils::timestampToString(endTime) << "' ";
        sql << "ORDER BY start_time DESC";
        
        auto result = conn_->Query(sql.str());
        
        // TODO: Parse result into CompressedSession objects
        LOG_INFO("Query returned " + std::to_string(result->RowCount()) + " sessions");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to query sessions by time range: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<CompressedSession> DuckDBManager::querySessionsByContentType(
    const std::string& contentType,
    int limit) {
    
    std::vector<CompressedSession> results;
    
    try {
        std::ostringstream sql;
        sql << "SELECT * FROM compressed_sessions WHERE ";
        sql << "content_type = '" << contentType << "' ";
        sql << "ORDER BY start_time DESC LIMIT " << limit;
        
        auto result = conn_->Query(sql.str());
        
        // TODO: Parse result into CompressedSession objects
        LOG_INFO("Query returned " + std::to_string(result->RowCount()) + " sessions");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to query sessions by content type: " + std::string(e.what()));
    }
    
    return results;
}

std::vector<CompressedSession> DuckDBManager::querySessionsByEngagement(
    double minEngagement,
    int limit) {
    
    std::vector<CompressedSession> results;
    
    try {
        std::ostringstream sql;
        sql << "SELECT * FROM compressed_sessions WHERE ";
        sql << "engagement_score >= " << minEngagement << " ";
        sql << "ORDER BY engagement_score DESC LIMIT " << limit;
        
        auto result = conn_->Query(sql.str());
        
        // TODO: Parse result into CompressedSession objects
        LOG_INFO("Query returned " + std::to_string(result->RowCount()) + " sessions");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to query sessions by engagement: " + std::string(e.what()));
    }
    
    return results;
}

int DuckDBManager::getSessionCount() {
    try {
        auto result = conn_->Query("SELECT COUNT(*) FROM compressed_sessions");
        
        if (result && result->RowCount() > 0) {
            return result->GetValue(0, 0).GetValue<int>();
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get session count: " + std::string(e.what()));
    }
    
    return 0;
}

int DuckDBManager::deleteSessionsOlderThan(const Timestamp& cutoffTime) {
    try {
        std::ostringstream sql;
        sql << "DELETE FROM compressed_sessions WHERE ";
        sql << "compressed_at < TIMESTAMP '" << utils::timestampToString(cutoffTime) << "'";
        
        auto result = conn_->Query(sql.str());
        
        LOG_INFO("Deleted old compressed sessions");
        
        // TODO: Return actual count of deleted rows
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to delete old sessions: " + std::string(e.what()));
        return 0;
    }
}

} // namespace layer1
} // namespace perception
