#include "common/DatabaseConfig.h"
#include "common/Logger.h"

namespace perception {

DatabaseConfig::DatabaseConfig()
    : baseDir("./perception_data")
    , sqliteDb("raw_events.db")
    , duckdbDb("compressed_context.duckdb")
    , qdrantPath("vector_store")
    , rawRetentionHours(24)
    , compressedRetentionDays(7)
    , idleThresholdSeconds(300)
    , minSessionDurationSeconds(3)
    , compressionDelaySeconds(60)
    , embeddingModel("sentence-transformers/all-MiniLM-L6-v2")
    , embeddingDimension(384) {
    
    // Initialize default max tokens per content type
    maxTokensPerContentType["email"] = 300;
    maxTokensPerContentType["chat"] = 250;
    maxTokensPerContentType["web_article"] = 200;
    maxTokensPerContentType["web_page"] = 150;
    maxTokensPerContentType["code"] = 400;
    maxTokensPerContentType["document"] = 250;
    maxTokensPerContentType["meeting"] = 400;
    maxTokensPerContentType["video"] = 100;
    maxTokensPerContentType["default"] = 200;
    
    initializeDirectories();
}

DatabaseConfig::DatabaseConfig(const std::filesystem::path& baseDir)
    : DatabaseConfig() {
    this->baseDir = baseDir;
    initializeDirectories();
}

void DatabaseConfig::initializeDirectories() {
    try {
        if (!std::filesystem::exists(baseDir)) {
            std::filesystem::create_directories(baseDir);
            LOG_INFO("Created base directory: " + baseDir.string());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create base directory: " + std::string(e.what()));
    }
}

std::filesystem::path DatabaseConfig::getSqlitePath() const {
    return baseDir / sqliteDb;
}

std::filesystem::path DatabaseConfig::getDuckdbPath() const {
    return baseDir / duckdbDb;
}

std::filesystem::path DatabaseConfig::getQdrantPath() const {
    return baseDir / qdrantPath;
}

SessionConfig::SessionConfig()
    : idleThresholdSeconds(300)
    , domainChangeTriggers(true)
    , appTypeChangeTriggers(true)
    , contentTypeChangeTriggers(true)
    , workSessionIdleThresholdMinutes(15)
    , workSessionOverlapThreshold(0.5) {
}

} // namespace perception
