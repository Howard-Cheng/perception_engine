#include "providers/AppActivityContextProvider.h"
#include "pe_base/logger.h"
#include "pe_base/time_util.h"

// Windows headers
#include <Windows.h>
#include <iostream>
// UI Automation headers
#include <UIAutomationClient.h>
#include <UIAutomationCore.h>

#include <thread>

AppActivityContextProvider::~AppActivityContextProvider() {
    shutdown();
}

bool AppActivityContextProvider::initialize() {
    // Initialize active app monitoring
    std::cout << "[AppActivityContext] Initializing active app monitoring..." << std::endl;
    if (WindowsAPIs::InitializeActiveAppMonitoring()) {
        std::cout << "[AppActivityContext] Active app monitoring initialized successfully!" << std::endl;
    } else {
        std::cerr << "[AppActivityContext] Failed to initialize active app monitoring!" << std::endl;
        return false;
    }
    
    // Initialize MouseTracker asynchronously
    std::thread([this]() {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            mouseTracker_ = std::make_unique<MouseTracker>();
            if (mouseTracker_->Initialize()) {
                mouseTracker_->Start();
                std::cout << "[AppActivityContext] MouseTracker initialized" << std::endl;
            } else {
                std::cerr << "[AppActivityContext] Failed to initialize MouseTracker" << std::endl;
                mouseTracker_.reset();
            }
        } catch (...) {
            std::cerr << "[AppActivityContext] Exception in MouseTracker init" << std::endl;
        }
    }).detach();
    
    // Register window switch callback
    std::cout << "[AppActivityContext] Registering window switch callback..." << std::endl;
    WindowsAPIs::WindowsAPIsManager::GetInstance().RegisterWindowSwitchCallback(
        [this](const WindowsAPIs::ActiveAppRecord& record) {
            this->onWindowSwitch(record);
        }
    );
    std::cout << "[AppActivityContext] Callback registered successfully!" << std::endl;
    
    return true;
}

void AppActivityContextProvider::update() {
    // App activity is event-driven, not polled
}

void AppActivityContextProvider::collectContext(pe_base::Json& context) const {
    std::lock_guard<std::mutex> lock(mutex_);
    context.set("activeApp", currentApp_);
    context.set("windowTitle", currentWindowTitle_);
    context.set("activeAppContent", currentContent_);
    context.set("duration", dwellTimeSeconds_);
    context.set("interactionCount", static_cast<long long>(interactionCount_));  // Explicit cast to long long
    context.set("startTime", startSwitchTime_);
}

std::string AppActivityContextProvider::getName() const {
    return "AppActivityContext";
}

bool AppActivityContextProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !currentApp_.empty();
}

void AppActivityContextProvider::shutdown() {
    std::cout << "[AppActivityContext] Shutting down..." << std::endl;
    
    // Clear callback
    WindowsAPIs::WindowsAPIsManager::GetInstance().ClearWindowSwitchCallback();
    
    // Cleanup active app monitoring
    WindowsAPIs::CleanupActiveAppMonitoring();
    std::cout << "[AppActivityContext] Active app monitoring cleaned up" << std::endl;
    
    // Stop MouseTracker
    std::lock_guard<std::mutex> lock(mutex_);
    if (mouseTracker_) {
        mouseTracker_->Stop();
        mouseTracker_.reset();
        std::cout << "[AppActivityContext] MouseTracker stopped" << std::endl;
    }
}

void AppActivityContextProvider::registerWindowSwitchCallback(WindowSwitchCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    upperCallback_ = callback;
    std::cout << "[AppActivityContext] Upper-layer callback registered" << std::endl;
}

void AppActivityContextProvider::clearWindowSwitchCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    upperCallback_ = nullptr;
}

void AppActivityContextProvider::onWindowSwitch(const WindowsAPIs::ActiveAppRecord& record) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        currentApp_ = record.appName;
        currentWindowTitle_ = record.windowTitle;
        currentContent_ = record.appContent;
        dwellTimeSeconds_ = record.durationSeconds;
        startSwitchTime_ = pe_base::TimeUtil::TimestampMs();
        
        // Update interaction count
        if (mouseTracker_) {
            interactionCount_ = mouseTracker_->GetClickedCount();
        }
        
        std::cout << "[AppActivityContext] Window switched: " << currentApp_ << std::endl;
    }
    
    // Trigger upper-layer callback (outside mutex to avoid deadlock)
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        if (upperCallback_) {
            std::cout << "[AppActivityContext] Triggering upper-layer callback..." << std::endl;
            upperCallback_(record);
        }
    }
}

std::vector<database::MouseEvent> AppActivityContextProvider::getMouseEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mouseTracker_) {
        return mouseTracker_->GetMouseEvents();
    }
    return {};
}

void AppActivityContextProvider::resetMouseRecords() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (mouseTracker_) {
        mouseTracker_->ResetMouseRecords();
    }
}

UINT64 AppActivityContextProvider::getInteractionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interactionCount_;
}

std::string AppActivityContextProvider::getCurrentApp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentApp_;
}

std::string AppActivityContextProvider::getCurrentContent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentContent_;
}
