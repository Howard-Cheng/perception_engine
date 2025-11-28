#pragma once

#include "providers/IContextProvider.h"
#include "providers/SystemContextProvider.h"
#include "providers/VoiceContextProvider.h"
#include "providers/CameraContextProvider.h"
#include "providers/AppActivityContextProvider.h"
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>

/**
 * @brief Composite Context Manager
 * 
 * Uses Composite Pattern to manage all context providers
 * Provides unified interface to collect, update and manage all contexts
 */
class CompositeContextManager {
public:
    CompositeContextManager() = default;
    ~CompositeContextManager();
    
    bool initialize();
    void updateAll();
    pe_base::Json collectAllContext();
    
    // Get specific provider (type-safe)
    std::shared_ptr<SystemContextProvider> getSystemProvider() const;
    std::shared_ptr<VoiceContextProvider> getVoiceProvider() const;
    std::shared_ptr<CameraContextProvider> getCameraProvider() const;
    std::shared_ptr<AppActivityContextProvider> getAppActivityProvider() const;
    
    // Get provider (generic)
    ContextProviderPtr getProvider(const std::string& name) const;
    
    // Check if cache should be updated
    bool shouldUpdate(int intervalSeconds = 1) const;
    
    // Shutdown all providers
    void shutdown();
    
    // Get status of all providers
    std::map<std::string, bool> getProviderStatus() const;
    
private:
    mutable std::mutex mutex_;
    
    // Generic provider map
    std::map<std::string, ContextProviderPtr> providers_;
    
    // Typed references for convenience
    std::shared_ptr<SystemContextProvider> systemProvider_;
    std::shared_ptr<VoiceContextProvider> voiceProvider_;
    std::shared_ptr<CameraContextProvider> cameraProvider_;
    std::shared_ptr<AppActivityContextProvider> appActivityProvider_;
    
    std::chrono::steady_clock::time_point lastUpdate_;
};
