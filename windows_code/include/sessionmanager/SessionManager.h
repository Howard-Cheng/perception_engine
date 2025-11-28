#pragma once

#include "pe_base/json.hpp"
#include "DatabaseTypes.h"
#include "IDatabaseClient.h"
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>

namespace sessionmanager {

/**
 * @brief SessionManager - Manages session grouping and compression
 * 
 * Responsibilities:
 * - Monitor uncompressed record count
 * - Group similar consecutive records into sessions
 * - Assign unique session IDs
 * - Update database with compression status
 * 
 * Features:
 * - Background processing (non-blocking)
 * - Configurable similarity threshold
 * - Thread-safe operations
 * - Automatic triggering based on record count
 */
class SessionManager {
public:
    /**
     * @brief Configuration for session management
     */
    struct Config {
        int compressionThreshold = 10;      // Trigger when uncompressed count > this
        int similarityThreshold = 60;       // Merge records if similarity > this (0-100)
        int batchSize = 100;                // Max records to process per batch
        bool enabled = true;                // Enable/disable compression
        
        Config() = default;
    };
    
    /**
     * @brief Similarity algorithm type
     */
    enum class SimilarityAlgorithm {
        SIMPLE,         // App name + window title only
        CONTENT_BASED,  // Include screen content similarity
        TIME_AWARE,     // Consider time intervals
        ML_BASED        // Machine learning model (future)
    };
    
    /**
     * @brief Constructor
     * @param dbClient Database client for operations
     * @param indexName Index/collection name
     * @param config Configuration settings
     */
    SessionManager(
        std::shared_ptr<database::IDatabaseClient> dbClient,
        const std::string& indexName,
        const Config& config = Config()
    );
    
    /**
     * @brief Destructor - stops worker thread
     */
    ~SessionManager();
    
    // Delete copy and move
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = delete;
    SessionManager& operator=(SessionManager&&) = delete;
    
    /**
     * @brief Start the session manager worker thread
     */
    void Start();
    
    /**
     * @brief Stop the session manager worker thread
     */
    void Stop();
    
    /**
     * @brief Check if compression is needed and trigger if necessary
     * @return true if compression was triggered
     */
    bool CheckAndTriggerCompression();
    
    /**
     * @brief Get current configuration
     */
    Config GetConfig() const;
    
    /**
     * @brief Update configuration
     */
    void UpdateConfig(const Config& config);
    
    /**
     * @brief Get uncompressed record count
     */
    int GetUncompressedCount();
    
    /**
     * @brief Get compression statistics
     */
    struct Statistics {
        int totalSessionsCreated = 0;
        int totalRecordsCompressed = 0;
        int lastBatchSize = 0;
        int currentUncompressedCount = 0;
        std::chrono::steady_clock::time_point lastCompressionTime;
        
        Statistics() : lastCompressionTime(std::chrono::steady_clock::now()) {}
    };
    
    Statistics GetStatistics() const;
    
    /**
     * @brief Set similarity algorithm
     */
    void SetSimilarityAlgorithm(SimilarityAlgorithm algorithm);
    
    /**
     * @brief Check if manager is running
     */
    bool IsRunning() const { return running_.load(); }
    
private:
    /**
     * @brief Session content - holds event info and similarity data
     */
    struct SessionContent {
        std::string eventId;
        std::string similarScreenContent;
        
        SessionContent() = default;
        SessionContent(const std::string& id) : eventId(id) {}
        SessionContent(const std::string& id, const std::string& similarity)
            : eventId(id), similarScreenContent(similarity) {}
    };
    
    /**
     * @brief Worker thread function
     */
    void WorkerThread();
    
    /**
     * @brief Process one batch of compression
     */
    void ProcessCompressionBatch();
    
    /**
     * @brief Post compression task to worker thread
     */
    void PostCompressionTask();
    
    /**
     * @brief Compare content similarity between two records
     * @return Similarity score (0-100)
     */
    int CompareContent(const pe_base::Json& record1, const pe_base::Json& record2);
    
    /**
     * @brief Simple similarity (app + window)
     */
    int CompareContentSimple(const pe_base::Json& record1, const pe_base::Json& record2);

    /**
     * @brief Content-based similarity (screen content)
     */
    int CompareContentMLBased(const pe_base::Json& record1, const pe_base::Json& record2);
    
    /**
     * @brief Content-based similarity (includes screen content)
     */
    int CompareContentWithText(const pe_base::Json& record1, const pe_base::Json& record2);
    
    /**
     * @brief Time-aware similarity (considers time intervals)
     */
    int CompareContentWithTime(const pe_base::Json& record1, const pe_base::Json& record2);
    
    /**
     * @brief Generate unique session ID
     */
    std::string GenerateSessionId();
    
    /**
     * @brief Mark records as compressed with session ID and similarity info
     */
    bool MarkRecordsCompressed(
        const std::vector<SessionContent>& sessionContents,
        const std::string& sessionId
    );
    
    /**
     * @brief Convert RawEvent to pe_base::Json for comparison
     */
    pe_base::Json ConvertEventToJson(const database::RawEvent& event);
    
    // Configuration
    Config config_;
    std::string indexName_;
    std::string deviceId_;  // For session ID generation
    
    // Database client
    std::shared_ptr<database::IDatabaseClient> dbClient_;
    mutable std::mutex dbMutex_;
    
    // Worker thread
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    std::atomic<bool> taskPending_{false};
    std::mutex taskMutex_;
    std::condition_variable taskCV_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    Statistics stats_;
    
    // Similarity algorithm
    SimilarityAlgorithm algorithm_ = SimilarityAlgorithm::ML_BASED;
    
    // Store last similarity summary for current comparison
    std::string lastSimilaritySummary_;
    
    // Store separated content A and B from last comparison
    std::string lastContentA_;
    std::string lastContentB_;
};

} // namespace sessionmanager
