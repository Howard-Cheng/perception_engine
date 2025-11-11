#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "common/Types.h"

// Forward declarations to avoid including DuckDB headers
namespace duckdb {
    class DuckDB;
    class Connection;
}

namespace perception {
namespace layer1 {

// Compressed Session structure for Layer 1 storage
struct CompressedSession {
    // Basic identification
    std::string sessionId;
    std::string deviceId;
    
    // Time bounds
    Timestamp startTime;
    Timestamp endTime;
    int durationSeconds;
    
    // Classification
    ContentType contentType;
    Domain domain;
    
    // Application context
    std::string appName;
    std::optional<std::string> windowTitle;
    std::optional<std::string> url;
    
    // Engagement metrics
    double engagementScore;
    int interactionCount;
    int totalDwellTime;
    bool hasCopied;
    bool hasSelected;
    
    // Compressed content
    std::string compressedSummary;      // LLM-generated summary
    std::vector<std::string> keyPoints;  // Extracted key points
    int summaryTokenCount;
    
    // High-attention content (for future retrieval)
    std::vector<std::string> copiedContent;
    std::vector<std::string> selectedText;
    std::vector<std::string> numbers;   // Phone numbers, important numbers
    std::vector<std::string> dates;     // Important dates
    std::vector<std::string> urls;      // Important URLs
    std::vector<std::string> emails;    // Email addresses
    
    // Metadata
    std::string metadataJson;  // Additional metadata as JSON
    
    // System information (averaged)
    double avgCpuUsage;
    double avgMemoryUsage;
    double avgBatteryPercent;
    
    // Timestamps
    Timestamp createdAt;     // When this session was created
    Timestamp compressedAt;  // When the compression occurred
};

// DuckDB Manager for Layer 1 compressed session storage
class DuckDBManager {
public:
    // Constructor - opens/creates DuckDB database
    explicit DuckDBManager(const std::string& dbPath);
    
    // Destructor
    ~DuckDBManager();
    
    // Delete copy constructor and assignment
    DuckDBManager(const DuckDBManager&) = delete;
    DuckDBManager& operator=(const DuckDBManager&) = delete;
    
    // Initialize database schema
    void initializeSchema();
    
    // Insert a compressed session
    // Returns the session_id
    std::string insertCompressedSession(const CompressedSession& session);
    
    // Query compressed sessions
    std::vector<CompressedSession> querySessionsByTimeRange(
        const Timestamp& startTime,
        const Timestamp& endTime
    );
    
    std::vector<CompressedSession> querySessionsByContentType(
        const std::string& contentType,
        int limit = 100
    );
    
    std::vector<CompressedSession> querySessionsByEngagement(
        double minEngagementScore,
        int limit = 100
    );
    
    // Statistics
    int getSessionCount();
    
    // Cleanup old sessions
    int deleteSessionsOlderThan(const Timestamp& cutoffTime);
    
private:
    std::string dbPath_;
    std::unique_ptr<duckdb::DuckDB> db_;
    std::unique_ptr<duckdb::Connection> conn_;
};

} // namespace layer1
} // namespace perception
