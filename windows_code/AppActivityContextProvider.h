#pragma once

#include "IContextProvider.h"
#include "MouseTracker.h"
#include "WindowsAPIs.h"
#include <mutex>
#include <memory>
#include <iostream>  // For std::cout, std::cerr

/**
 * @brief Application Activity Context Provider
 * 
 * Collects:
 * - Active application name
 * - Window title
 * - Application content
 * - Mouse interaction events
 * - Dwell time
 */
class AppActivityContextProvider : public IContextProvider {
public:
    // ? NEW: Callback type for window switch notifications
    using WindowSwitchCallback = std::function<void(const WindowsAPIs::ActiveAppRecord&)>;
    
    AppActivityContextProvider() = default;
    ~AppActivityContextProvider() override;
    
    bool initialize() override {
        // ? FIX: Initialize active app monitoring FIRST!
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
    
    void update() override {
        // App activity is event-driven, not polled
    }
    
    void collectContext(Json& context) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        context.set("activeApp", currentApp_);
        context.set("windowTitle", currentWindowTitle_);
        context.set("activeAppContent", currentContent_);
        context.set("duration", dwellTimeSeconds_);
        context.set("interactionCount", interactionCount_);
    }
    
    std::string getName() const override {
        return "AppActivityContext";
    }
    
    bool isAvailable() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return !currentApp_.empty();
    }
    
    void shutdown() override {
        std::cout << "[AppActivityContext] Shutting down..." << std::endl;
        
        // Clear callback
        WindowsAPIs::WindowsAPIsManager::GetInstance().ClearWindowSwitchCallback();
        
        // ? FIX: Cleanup active app monitoring
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
    
    // ========================================
    // ? NEW: Register upper-layer callback
    // ========================================
    
    /**
     * @brief Register window switch callback (for use by ContextCollector)
     */
    void registerWindowSwitchCallback(WindowSwitchCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        upperCallback_ = callback;
        std::cout << "[AppActivityContext] Upper-layer callback registered" << std::endl;
    }
    
    /**
     * @brief Clear window switch callback
     */
    void clearWindowSwitchCallback() {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        upperCallback_ = nullptr;
    }
    
    // Custom methods
    void onWindowSwitch(const WindowsAPIs::ActiveAppRecord& record) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            currentApp_ = record.appName;
            currentWindowTitle_ = record.windowTitle;
            currentContent_ = record.appContent;
            dwellTimeSeconds_ = record.durationSeconds;
            
            // Update interaction count
            if (mouseTracker_) {
                interactionCount_ = mouseTracker_->GetClickedCount();
            }
            
            std::cout << "[AppActivityContext] Window switched: " << currentApp_ << std::endl;
        }
        
        // ? NEW: Trigger upper-layer callback (outside mutex to avoid deadlock)
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex_);
            if (upperCallback_) {
                std::cout << "[AppActivityContext] Triggering upper-layer callback..." << std::endl;
                upperCallback_(record);
            }
        }
    }
    
    std::vector<database::MouseEvent> getMouseEvents() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (mouseTracker_) {
            return mouseTracker_->GetMouseEvents();
        }
        return {};
    }
    
    void resetMouseRecords() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (mouseTracker_) {
            mouseTracker_->ResetMouseRecords();
        }
    }
    
    int getInteractionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return interactionCount_;
    }
    
    std::string getCurrentApp() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentApp_;
    }
    
    std::string getCurrentContent() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentContent_;
    }
    
private:
    mutable std::mutex mutex_;
    mutable std::mutex callbackMutex_;  // ? NEW: Separate mutex for callback
    
    std::unique_ptr<MouseTracker> mouseTracker_;
    std::string currentApp_;
    std::string currentWindowTitle_;
    std::string currentContent_;
    int dwellTimeSeconds_ = 0;
    int interactionCount_ = 0;
    
    WindowSwitchCallback upperCallback_;  // ? NEW: Upper-layer callback
};

// Destructor implementation
inline AppActivityContextProvider::~AppActivityContextProvider() {
    shutdown();
}
