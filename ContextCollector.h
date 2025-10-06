#pragma once
#include "third-party/include/json.hpp"
#include "WindowsAPIs.h"
#include <chrono>
#include <mutex>

class ContextCollector {
private:
    Json cachedContext;
    std::chrono::steady_clock::time_point lastUpdate;
    std::mutex cacheMutex;
    
    void UpdateCache();
    bool ShouldUpdateCache();
    
public:
    ContextCollector();
    ~ContextCollector(); // Add destructor
    
    Json CollectCurrentContext();
    void StartPeriodicUpdate(); // Start background thread for periodic updates
    void StopPeriodicUpdate();
};