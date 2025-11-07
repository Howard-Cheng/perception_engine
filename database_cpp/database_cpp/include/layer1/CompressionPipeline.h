#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/DatabaseConfig.h"
#include "layer0/DataIngestion.h"
#include "layer1/DuckDBManager.h"

// Forward declarations
namespace perception {
namespace layer1 {
    class SessionDetector;
    class EngagementCalculator;
    class ContentExtractor;
    class ContentClassifier;
}
}

namespace perception {
namespace layer1 {

// Compression pipeline statistics
struct CompressionStatistics {
    int sessionsCompressed = 0;
    int eventsProcessed = 0;
    double averageCompressionRatio = 0.0;
    int64_t originalSize = 0;
    int64_t compressedSize = 0;
};

// Compression pipeline - Layer 0 to Layer 1
class CompressionPipeline {
public:
    // Constructor
    CompressionPipeline(
        const std::string& sqlitePath,
        const std::string& duckdbPath,
        const SessionConfig& config
    );
    
    // Destructor
    ~CompressionPipeline();
    
    // Delete copy constructor and assignment
    CompressionPipeline(const CompressionPipeline&) = delete;
    CompressionPipeline& operator=(const CompressionPipeline&) = delete;
    
    // Main processing method
    // Returns number of sessions compressed
    int processUncompressedEvents();
    
    // Get compression statistics
    CompressionStatistics getStatistics() const;
    
    // Cleanup old compressed sessions (returns number deleted)
    int cleanupOldCompressedSessions(int retentionDays);
    
private:
    // Fetch uncompressed events from SQLite
    std::vector<layer0::RawEvent> fetchUncompressedEvents();
    
    // Compress a single session
    CompressedSession compressSession(
        const std::vector<layer0::RawEvent>& sessionEvents
    );
    
    // LLM compression (placeholder)
    std::string compressContentWithLLM(
        const std::vector<layer0::RawEvent>& sessionEvents,
        double engagementScore
    );
    
    // Extract key points
    std::vector<std::string> extractKeyPoints(
        const std::vector<layer0::RawEvent>& sessionEvents
    );
    
    // Convert metadata to JSON
    std::string metadataToJson(const ContentMetadata& metadata) const;
    
    // Mark events as compressed in SQLite
    void markEventsAsCompressed(const std::vector<layer0::RawEvent>& events);
    
    // Paths
    std::string sqlitePath_;
    std::string duckdbPath_;
    
    // Configuration
    SessionConfig config_;
    
    // Components
    std::unique_ptr<SessionDetector> detector_;
    std::unique_ptr<EngagementCalculator> calculator_;
    std::unique_ptr<ContentExtractor> extractor_;
    std::unique_ptr<ContentClassifier> classifier_;
    std::unique_ptr<DuckDBManager> duckdb_;
    
    // Statistics
    CompressionStatistics stats_;
};

} // namespace layer1
} // namespace perception
