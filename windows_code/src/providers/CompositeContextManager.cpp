#include "providers/CompositeContextManager.h"

CompositeContextManager::~CompositeContextManager() {
    shutdown();
}

bool CompositeContextManager::initialize() {
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

void CompositeContextManager::updateAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [name, provider] : providers_) {
        if (provider && provider->isAvailable()) {
            provider->update();
        }
    }
    
    lastUpdate_ = std::chrono::steady_clock::now();
}

pe_base::Json CompositeContextManager::collectAllContext() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    pe_base::Json context;
    
    for (auto& [name, provider] : providers_) {
        if (provider && provider->isAvailable()) {
            provider->collectContext(context);
        }
    }
    
    return context;
}

std::shared_ptr<SystemContextProvider> CompositeContextManager::getSystemProvider() const {
    return systemProvider_;
}

std::shared_ptr<VoiceContextProvider> CompositeContextManager::getVoiceProvider() const {
    return voiceProvider_;
}

std::shared_ptr<CameraContextProvider> CompositeContextManager::getCameraProvider() const {
    return cameraProvider_;
}

std::shared_ptr<AppActivityContextProvider> CompositeContextManager::getAppActivityProvider() const {
    return appActivityProvider_;
}

ContextProviderPtr CompositeContextManager::getProvider(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(name);
    return (it != providers_.end()) ? it->second : nullptr;
}

bool CompositeContextManager::shouldUpdate(int intervalSeconds) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate_);
    return elapsed.count() >= intervalSeconds;
}

void CompositeContextManager::shutdown() {
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

std::map<std::string, bool> CompositeContextManager::getProviderStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, bool> status;
    for (const auto& [name, provider] : providers_) {
        status[name] = provider && provider->isAvailable();
    }
    return status;
}
