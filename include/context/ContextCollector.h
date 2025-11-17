#pragma once
#include "utils/json.hpp"
#include "platform/WindowsAPIs.h"
#include <chrono>
#include <mutex>
#include <string>
#include <memory>  // For std::unique_ptr
#include <atomic>
#include <thread>

// Forward declarations to avoid header conflicts
class MouseTracker;

namespace database {
    class IDatabaseClient;
}

class ContextCollector {
private:
    Json cachedContext;
    std::chrono::steady_clock::time_point lastUpdate;
    mutable std::mutex cacheMutex;

    // Voice transcription context
    std::string latestVoiceTranscription;
    mutable std::mutex voiceMutex;

    // Camera vision context
    std::string latestCameraDescription;
    float latestCameraLatency;
    mutable std::mutex cameraMutex;

    // Performance metrics (latencies in milliseconds)
    float latestVoiceLatency;
    float latestContextUpdateLatency;
    mutable std::mutex metricsMutex;

    // Mouse tracker for capturing mouse operations
    std::unique_ptr<MouseTracker> mouseTracker;
    mutable std::mutex mouseTrackerMutex;

    // Elasticsearch integration
    std::unique_ptr<database::IDatabaseClient> esClient;
    std::atomic<bool> esStorageRunning{false};
    std::thread esStorageThread;
    mutable std::mutex esClientMutex;
    std::string esIndexName{"perception_context"};  // Default index name
    std::string deviceId;  // Device identifier

    void UpdateCache();
    bool ShouldUpdateCache();
    
    // Convert Json context to Elasticsearch RawEvent
    void StoreContextToES(const Json& context);



public:
    ContextCollector();
    ~ContextCollector(); // Add destructor

    Json CollectCurrentContext();
    void StartPeriodicUpdate(); // Start background thread for periodic updates
    void StopPeriodicUpdate();

    // Voice transcription update
    void UpdateVoiceContext(const std::string& transcription);
    void UpdateVoiceContext(const std::string& transcription, float latencyMs);

    // Camera vision update
    void UpdateCameraContext(const std::string& description, float latencyMs);

    // Window switch callback - Called when user switches window/tab
    /**
     * @brief Called when user switches to a different window or browser tab
     * 
     * This function will be called by WindowsAPIsManager when:
     * - User switches applications (e.g., Chrome -> VSCode)
     * - User switches browser tabs
     * - Any window activation event occurs
     * 
     * @param record ActiveAppRecord containing info about the new active app
     */
    void OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record);

    // Generate fused context summary
    std::string GenerateFusedContext() const;
    std::string GenerateFusedContext(const std::string& voiceText) const;
    
    // ? NEW: Elasticsearch integration APIs
    
    /**
     * @brief Initialize Elasticsearch client and start background storage thread
     * 
     * @param esHost Elasticsearch host URL (default: "http://localhost:9200")
     * @param indexName Index name to use (default: "perception_context")
     * @return true if initialization successful
     */
    bool InitializeElasticsearch(const std::string& esHost = "http://localhost:9200",
                                 const std::string& indexName = "perception_context");
    
    /**
     * @brief Stop Elasticsearch storage and cleanup
     */
    void ShutdownElasticsearch();
    
    /**
     * @brief Query Elasticsearch database with keyword and time range
     * 
     * @param keyword Search keyword (searches in screenContent, voiceTranscription, cameraDescription, appName, windowTitle)
     * @param startTime Start time (Unix timestamp)
     * @param endTime End time (Unix timestamp)
     * @param maxResults Maximum number of results to return (default: 100)
     * @return Json array of matching context entries
     */
    Json GetESDBData(const std::string& keyword,
                    std::time_t startTime,
                    std::time_t endTime,
                    int maxResults = 100);
    
    /**
     * @brief Check if Elasticsearch is connected and operational
     * 
     * @return true if ES is available
     */
    bool IsElasticsearchAvailable() const;
};