#pragma once
#include "third-party/include/json.hpp"
#include "WindowsAPIs.h"
#include <chrono>
#include <mutex>
#include <string>
#include <memory>  // For std::unique_ptr

// Forward declaration to avoid header conflicts
class MouseTracker;

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

    void UpdateCache();
    bool ShouldUpdateCache();

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

    // Generate fused context summary
    std::string GenerateFusedContext() const;
    std::string GenerateFusedContext(const std::string& voiceText) const;
};