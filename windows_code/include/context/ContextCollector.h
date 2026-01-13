#pragma once

#include <nlohmann/json.hpp>
#include "platform/WindowsAPIs.h"
#include "providers/CompositeContextManager.h"
#include "IDatabaseClient.h"
#include "DatabaseTypes.h"
#include "sessionmanager/SessionManager.h"  // Add SessionManager
#include "VectorStore.h"  // Add VectorStore (from vectordb_client)
#include "providers/AppActivityContextProvider.h" // Include this to use AppActivityType in this file
#include <chrono>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <windows.h>  // For threadpool timer

/**
 * @brief ContextCollector
 * 
 * Uses CompositeContextManager to coordinate multiple context providers
 * Responsibilities:
 * - Coordinate multiple context providers
 * - Provide unified API
 * - Handle Elasticsearch storage
 * - Periodic updates
 */
class ContextCollector {
public:
    ContextCollector();
    ~ContextCollector();
    
    // Disable copy
    ContextCollector(const ContextCollector&) = delete;
    ContextCollector& operator=(const ContextCollector&) = delete;
    
    /**
     * @brief Collect current context from all providers
     */
    nlohmann::json CollectCurrentContext();
    
    /**
     * @brief Start periodic update thread
     */
    void StartPeriodicUpdate();
    
    /**
     * @brief Stop periodic update thread
     */
    void StopPeriodicUpdate();
    
    // ========================================
    // Delegation methods - Provide legacy API compatibility
    // ========================================
    
    /**
     * @brief Update voice context
     */
    void UpdateVoiceContext(const std::string& transcription, float latencyMs = 0.0f) {
        if (auto provider = contextManager_.getVoiceProvider()) {
            provider->updateTranscription(transcription, latencyMs);
        }
    }
    
    /**
     * @brief Update camera context
     */
    void UpdateCameraContext(const std::string& description, float latencyMs = 0.0f) {
        if (auto provider = contextManager_.getCameraProvider()) {
            provider->updateDescription(description, latencyMs);
        }
    }
    
    /**
     * @brief Generate fused context summary
     */
    std::string GenerateFusedContext() const;
    
    // ========================================
    // Database Integration (PostgreSQL)
    // ========================================
    
    /**
     * @brief Initialize Database client (PostgreSQL)
     */
    bool InitializeDatabase(const std::string& connectionString = "host=127.0.0.1 port=5432 dbname=perception_engine user=postgres",
                                 const std::string& tableName = "perception_context");
    
    /**
     * @brief Shutdown Database client
     */
    void ShutdownDatabase();
    /**
     * @brief Query Database data
     */
    nlohmann::json GetESDBData(const std::string& keyword,
                    std::time_t startTime,
                    std::time_t endTime,
                    int maxResults = 100);
    
    /**
     * @brief Query VectorDB (Qdrant) session summaries data
     * 
     * Performs semantic search over session summaries stored in vector database.
     * 
     * @param keyword Query text for semantic search (will be embedded)
     * @param startTime Start time filter (Unix timestamp in seconds)
     * @param endTime End time filter (Unix timestamp in seconds)
     * @param maxResults Maximum number of results to return (default: 100)
     * @return JSON object containing search results with metadata
     */
    nlohmann::json GetVectorDBData(const std::string& keyword,
                    std::time_t startTime,
                    std::time_t endTime,
                    int maxResults = 100);
    
    /**
     * @brief Check if Database is available
     */
    bool IsElasticsearchAvailable() const;
    
    /**
     * @brief Store context to Database
     */
    void StoreContextToES(const nlohmann::json& context);
    
    /**
     * @brief Window switch event callback
     */
    void OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record);
    
private:
    /**
     * @brief Periodic update thread function
     */
    void updateCacheThread();
    
    /**
     * @brief Convert nlohmann::json context to RawEvent for Elasticsearch
     */
    database::RawEvent jsonContextToRawEvent(const nlohmann::json& context);
    
    /**
     * @brief Start compression timer (runs every 1 minute)
     */
    void StartCompressionTimer();
    
    /**
     * @brief Stop compression timer
     */
    void StopCompressionTimer();
    
    /**
     * @brief Timer callback for compression check (static)
     */
    static VOID CALLBACK CompressionTimerCallback(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID Context,
        PTP_TIMER Timer
    );
    
    /**
     * @brief Timer callback implementation (non-static)
     */
    void OnCompressionTimerTick();
    
    // ========================================
    // Core Components
    // ========================================
    
    CompositeContextManager contextManager_;
    
    // Device ID
    std::string deviceId_;
    
    // Periodic update thread
    std::atomic<bool> updateThreadRunning_{false};
    std::thread updateThread_;
    
    // ========================================
    // Database Storage (Generic - PostgreSQL/Elasticsearch compatible)
    // ========================================
    
    std::shared_ptr<database::IDatabaseClient> dbClient_;
    std::string dbCollectionName_;
    std::atomic<bool> dbStorageRunning_{false};
    mutable std::mutex dbClientMutex_;
    
    // ========================================
    // Session Management (Replaces old compression code)
    // ========================================
    
    std::unique_ptr<sessionmanager::SessionManager> sessionManager_;
    
    // ========================================
    // Vector Database (Qdrant for session summaries)
    // ========================================
    
    std::unique_ptr<vectordb::VectorStore> vectorStore_;
    
    // ========================================
    // Compression Timer (Windows Threadpool Timer)
    // ========================================
    
    PTP_TIMER compressionTimer_{nullptr};
    std::atomic<bool> compressionTimerRunning_{false};
    std::atomic<bool> compressionTaskRunning_{false};  // Prevent re-entry
};
