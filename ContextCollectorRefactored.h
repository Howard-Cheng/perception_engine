#pragma once

#include "third-party/include/json.hpp"
#include "WindowsAPIs.h"
#include "CompositeContextManager.h"
#include "DatabaseClientFactory.h"
#include "DatabaseTypes.h"
#include <chrono>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>
#include <thread>

/**
 * @brief Refactored ContextCollector
 * 
 * Uses CompositeContextManager to coordinate multiple context providers
 * Responsibilities:
 * - Coordinate multiple context providers
 * - Provide unified API
 * - Handle Elasticsearch storage
 * - Periodic updates
 */
class ContextCollectorRefactored {
public:
    ContextCollectorRefactored();
    ~ContextCollectorRefactored();
    
    // Disable copy
    ContextCollectorRefactored(const ContextCollectorRefactored&) = delete;
    ContextCollectorRefactored& operator=(const ContextCollectorRefactored&) = delete;
    
    /**
     * @brief Collect current context from all providers
     */
    Json CollectCurrentContext();
    
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
    // Elasticsearch Integration
    // ========================================
    
    /**
     * @brief Initialize Elasticsearch client
     */
    bool InitializeElasticsearch(const std::string& esHost = "http://localhost:9200",
                                 const std::string& indexName = "perception_context");
    
    /**
     * @brief Shutdown Elasticsearch client
     */
    void ShutdownElasticsearch();
    
    /**
     * @brief Query Elasticsearch data
     */
    Json GetESDBData(const std::string& keyword,
                    std::time_t startTime,
                    std::time_t endTime,
                    int maxResults = 100);
    
    /**
     * @brief Check if Elasticsearch is available
     */
    bool IsElasticsearchAvailable() const;
    
    /**
     * @brief Store context to Elasticsearch
     */
    void StoreContextToES(const Json& context);
    
    /**
     * @brief Window switch event callback
     */
    void OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record);
    
private:
    // Context manager (using composite pattern)
    CompositeContextManager contextManager_;
    
    // Device ID
    std::string deviceId_;
    
    // Periodic update thread
    std::atomic<bool> updateThreadRunning_{false};
    std::thread updateThread_;
    
    // Elasticsearch integration
    std::unique_ptr<database::IDatabaseClient> esClient_;
    std::atomic<bool> esStorageRunning_{false};
    mutable std::mutex esClientMutex_;
    std::string esIndexName_{"perception_context"};
    
    // Helper methods
    void updateCacheThread();
    database::RawEvent jsonContextToRawEvent(const Json& context);
};
