#pragma once

#include "providers/IContextProvider.h"
#include <mutex>
#include <nlohmann/json.hpp>

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
    
    bool initialize() override;
    void update() override;
    void collectContext(nlohmann::json& context) const override;
    std::string getName() const override;
    bool isAvailable() const override;
    void shutdown() override;
    
    // Custom methods for camera context
    void updateDescription(const std::string& description, float latencyMs = 0.0f);
    std::string getDescription() const;
    
private:
    mutable std::mutex mutex_;
    std::string description_;
    float latencyMs_ = 0.0f;
};
