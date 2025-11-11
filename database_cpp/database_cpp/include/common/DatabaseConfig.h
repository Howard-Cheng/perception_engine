#pragma once

#include <string>
#include <filesystem>
#include <map>
#include "Types.h"

namespace perception {

// Database configuration
struct DatabaseConfig {
    // Paths
    std::filesystem::path baseDir;
    std::string sqliteDb;
    std::string duckdbDb;
    std::string qdrantPath;
    
    // Retention policies (seconds)
    int rawRetentionHours;
    int compressedRetentionDays;
    
    // Session detection thresholds
    int idleThresholdSeconds;
    int minSessionDurationSeconds;
    
    // Compression settings
    int compressionDelaySeconds;
    std::map<std::string, int> maxTokensPerContentType;
    
    // Embedding settings
    std::string embeddingModel;
    int embeddingDimension;
    
    // Default constructor
    DatabaseConfig();
    
    // Constructor with custom base directory
    explicit DatabaseConfig(const std::filesystem::path& baseDir);
    
    // Initialize directories
    void initializeDirectories();
    
    // Get full database paths
    std::filesystem::path getSqlitePath() const;
    std::filesystem::path getDuckdbPath() const;
    std::filesystem::path getQdrantPath() const;
};

// Session detection configuration
struct SessionConfig {
    int idleThresholdSeconds;
    bool domainChangeTriggers;
    bool appTypeChangeTriggers;
    bool contentTypeChangeTriggers;
    int workSessionIdleThresholdMinutes;
    double workSessionOverlapThreshold;
    
    // Default constructor with sensible defaults
    SessionConfig();
};

} // namespace perception
