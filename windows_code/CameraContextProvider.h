#pragma once

#include "IContextProvider.h"
#include <mutex>

/**
 * @brief Camera Vision Context Provider
 * 
 * Collects:
 * - Camera scene description
 * - Processing latency
 */
class CameraContextProvider : public IContextProvider {
public:
    CameraContextProvider() = default;
    ~CameraContextProvider() override = default;
    
    bool initialize() override {
        // Camera context is passive (updated via Python client)
        return true;
    }
    
    void update() override {
        // Camera context is updated via updateDescription(), not via polling
    }
    
    void collectContext(Json& context) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!description_.empty()) {
            context.set("cameraDescription", description_);
            context.set("cameraLatency", static_cast<int>(latencyMs_));
        } else {
            context.setRaw("cameraDescription", "null");
            context.set("cameraLatency", 0);
        }
    }
    
    std::string getName() const override {
        return "CameraContext";
    }
    
    bool isAvailable() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return !description_.empty();
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        description_.clear();
        latencyMs_ = 0.0f;
    }
    
    // Custom methods for camera context
    void updateDescription(const std::string& description, float latencyMs = 0.0f) {
        std::lock_guard<std::mutex> lock(mutex_);
        description_ = description;
        latencyMs_ = latencyMs;
    }
    
    std::string getDescription() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return description_;
    }
    
private:
    mutable std::mutex mutex_;
    std::string description_;
    float latencyMs_ = 0.0f;
};
