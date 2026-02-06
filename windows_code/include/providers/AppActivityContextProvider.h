#pragma once

#include "providers/IContextProvider.h"
#include "platform/MouseTracker.h"
#include "platform/WindowsAPIs.h"
#include <mutex>
#include <memory>
#include <ctime>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

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
    // Callback type for window switch notifications
    using WindowSwitchCallback = std::function<void(const WindowsAPIs::ActiveAppRecord&)>;
    
    AppActivityContextProvider() = default;
    ~AppActivityContextProvider() override;
    
    bool initialize() override;
    void update() override;
    void collectContext(nlohmann::json& context) const override;
    std::string getName() const override;
    bool isAvailable() const override;
    void shutdown() override;
    
    // Register upper-layer callback
    void registerWindowSwitchCallback(WindowSwitchCallback callback);
    void clearWindowSwitchCallback();
    
    // Custom methods
    void onWindowSwitch(const WindowsAPIs::ActiveAppRecord& record);
    std::vector<database::MouseEvent> getMouseEvents() const;
    void resetMouseRecords();
    UINT64 getInteractionCount() const;  // Changed from int to UINT64
    std::string getCurrentApp() const;
    std::string getCurrentContent() const;
    
private:
    mutable std::mutex mutex_;
    mutable std::mutex callbackMutex_;
    
    std::unique_ptr<MouseTracker> mouseTracker_;
    std::string currentApp_;
    std::string currentWindowTitle_;
    std::string currentContent_;
    std::string currentUrl_;  // Store current URL
    std::string currentContentHash_;
    int dwellTimeSeconds_ = 0;
    UINT64 interactionCount_ = 0;  // Changed from int to UINT64
    long long startSwitchTime_ = 0;
    
    WindowSwitchCallback upperCallback_;
};
