#pragma once

#include "IContextProvider.h"
#include "SystemContextProvider.h"
#include "VoiceContextProvider.h"
#include "CameraContextProvider.h"
#include "AppActivityContextProvider.h"
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
    ~CompositeContextManager() {
        shutdown();
    }
    
    /**
     * @brief Initialize all context providers
     */
    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Create and register all providers
        auto systemProvider = std::make_shared<SystemContextProvider>();
        auto voiceProvider = std::make_shared<VoiceContextProvider>();
        auto cameraProvider = std::make_shared<CameraContextProvider>();
        auto appActivityProvider = std::make_shared<AppActivityContextProvider>();
        
        // Initialize all providers
        bool success = true;
        
        if (systemProvider->initialize()) {
            providers_["system"] = systemProvider;
            systemProvider_ = systemProvider; // Keep typed reference
        } else {
            success = false;
        }
        
        if (voiceProvider->initialize()) {
            providers_["voice"] = voiceProvider;
            voiceProvider_ = voiceProvider;
        } else {
            success = false;
        }
        
        if (cameraProvider->initialize()) {
            providers_["camera"] = cameraProvider;
            cameraProvider_ = cameraProvider;
        } else {
            success = false;
        }
        
        if (appActivityProvider->initialize()) {
            providers_["appActivity"] = appActivityProvider;
            appActivityProvider_ = appActivityProvider;
        } else {
            success = false;
        }
        
        return success;
    }
    
    /**
     * @brief Update all context providers (called periodically)
     */
    void updateAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [name, provider] : providers_) {
            if (provider && provider->isAvailable()) {
                provider->update();
            }
        }
        
        lastUpdate_ = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Collect all contexts into a single Json object
     */
    Json collectAllContext() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Json context;
        
        for (auto& [name, provider] : providers_) {
            if (provider && provider->isAvailable()) {
                provider->collectContext(context);
            }
        }
        
        return context;
    }
    
    /**
     * @brief Get specific provider (type-safe)
     */
    std::shared_ptr<SystemContextProvider> getSystemProvider() const {
        return systemProvider_;
    }
    
    std::shared_ptr<VoiceContextProvider> getVoiceProvider() const {
        return voiceProvider_;
    }
    
    std::shared_ptr<CameraContextProvider> getCameraProvider() const {
        return cameraProvider_;
    }
    
    std::shared_ptr<AppActivityContextProvider> getAppActivityProvider() const {
        return appActivityProvider_;
    }
    
    /**
     * @brief Get provider (generic)
     */
    ContextProviderPtr getProvider(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = providers_.find(name);
        return (it != providers_.end()) ? it->second : nullptr;
    }
    
    /**
     * @brief Check if cache should be updated
     */
    bool shouldUpdate(int intervalSeconds = 1) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate_);
        return elapsed.count() >= intervalSeconds;
    }
    
    /**
     * @brief Shutdown all providers
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [name, provider] : providers_) {
            if (provider) {
                provider->shutdown();
            }
        }
        
        providers_.clear();
        systemProvider_.reset();
        voiceProvider_.reset();
        cameraProvider_.reset();
        appActivityProvider_.reset();
    }
    
    /**
     * @brief Get status of all providers
     */
    std::map<std::string, bool> getProviderStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::map<std::string, bool> status;
        for (const auto& [name, provider] : providers_) {
            status[name] = provider && provider->isAvailable();
        }
        return status;
    }
    
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
