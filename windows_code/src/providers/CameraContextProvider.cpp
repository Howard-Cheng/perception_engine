#include "providers/CameraContextProvider.h"

bool CameraContextProvider::initialize() {
    // Camera context is passive (updated via Python client)
    return true;
}

void CameraContextProvider::update() {
    // Camera context is updated via updateDescription(), not via polling
}

void CameraContextProvider::collectContext(nlohmann::json& context) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!description_.empty()) {
        context["cameraDescription"] = description_;
        context["cameraLatency"] = static_cast<int>(latencyMs_);
    } else {
        context["cameraDescription"] = nullptr;
        context["cameraLatency"] = 0;
    }
}

std::string CameraContextProvider::getName() const {
    return "CameraContext";
}

bool CameraContextProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !description_.empty();
}

void CameraContextProvider::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    description_.clear();
    latencyMs_ = 0.0f;
}

void CameraContextProvider::updateDescription(const std::string& description, float latencyMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    description_ = description;
    latencyMs_ = latencyMs;
}

std::string CameraContextProvider::getDescription() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return description_;
}
