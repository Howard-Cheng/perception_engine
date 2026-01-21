#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

// Forward declarations
class WindowEventMonitor;
class BrowserContentExtractor;
class AsyncTaskQueue;
struct WindowInfo;

// Windows APIs wrapper functions
namespace WindowsAPIs {
    // Active Application History - New functionality
    struct ActiveAppRecord {
        std::string appName;
        std::string appContent;
        std::string windowTitle;
        std::chrono::system_clock::time_point timestamp;
        int durationSeconds;

        ActiveAppRecord() : durationSeconds(0) {}
        ActiveAppRecord(const std::string& name, const std::string& content, const std::string& title)
            : appName(name), windowTitle(title), timestamp(std::chrono::system_clock::now()), durationSeconds(1) {}
    };

    // Location structure
    struct Location {
        double latitude = 0.0;
        double longitude = 0.0;
        bool valid = false;
        std::string description="";
    };

    // ⚡ NEW: WindowsAPIsManager - Singleton class for managing Windows APIs
    class WindowsAPIsManager {
    public:
        // Callback type for window switch events - ⚡ UPDATED: Now passes ActiveAppRecord
        using WindowSwitchCallback = std::function<void(const ActiveAppRecord&)>;

        // Get singleton instance
        static WindowsAPIsManager& GetInstance();

        // Delete copy and move constructors
        WindowsAPIsManager(const WindowsAPIsManager&) = delete;
        WindowsAPIsManager& operator=(const WindowsAPIsManager&) = delete;
        WindowsAPIsManager(WindowsAPIsManager&&) = delete;
        WindowsAPIsManager& operator=(WindowsAPIsManager&&) = delete;

        // Initialize and start active app monitoring
        bool InitializeActiveAppMonitoring();

        // Stop active app monitoring and cleanup
        void CleanupActiveAppMonitoring();

        // Register callback for window switch events
        void RegisterWindowSwitchCallback(WindowSwitchCallback callback);

        // Clear window switch callback
        void ClearWindowSwitchCallback();

        // Get recent active apps within the specified time period
        std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList();

        // Get current active app content using UIA
        std::string GetCurrentActiveAppContent();

        // Get foreground app name
        std::string GetForegroundAppName();

        // Location (optional)
        Location GetLocation();
		Location GetOnlineLocation();

    private:
        WindowsAPIsManager();
        ~WindowsAPIsManager();

        // Event callback function for window monitoring (internal)
        void OnWindowEventInternal(const WindowInfo& info);

        // ✅ NEW: Process window switch event asynchronously
        void ProcessWindowSwitchAsync(const ActiveAppRecord& record);

        // Cleanup old records (older than 1 hour)
        void CleanupOldRecords();

        // Helper function: Generate unique key from appName and windowTitle
        std::string MakeAppKey(const std::string& appName, const std::string& windowTitle);

        // Convert WindowInfo to app name
        std::string GetAppNameFromWindowInfo(const WindowInfo& info);

        // Member variables
        std::unique_ptr<WindowEventMonitor> m_eventMonitor;
        std::unordered_map<std::string, ActiveAppRecord> m_activeAppHistory;
        std::mutex m_historyMutex;
        std::string m_lastActiveApp;
        std::string m_lastActiveAppWindowTitle;
        std::string m_lastActiveAppContent;
        std::chrono::system_clock::time_point m_lastAppStartTime;
        
        std::unique_ptr<BrowserContentExtractor> m_contentExtractor;
        std::mutex m_extractorMutex;
        bool m_extractorInitialized;

        // Window switch callback
        WindowSwitchCallback m_windowSwitchCallback;
        std::mutex m_callbackMutex;
        
        // ✅ NEW: Async task queue for callback execution
        std::unique_ptr<AsyncTaskQueue> m_callbackTaskQueue;
        
        // Global location cache
        Location m_cachedLocation;
        std::chrono::steady_clock::time_point m_lastLocationUpdate;
        const std::chrono::minutes LOCATION_CACHE_DURATION{ 30 }; // 30 minutes cache
        bool m_locationInitialized = false;
    };

    // ⚡ Compatibility layer: Keep original namespace functions for backward compatibility
    
    // Active Application
    std::string GetForegroundAppName();

    // Active Application Content
    std::string GetCurrentActiveAppContent();

    // Time utilities
    std::string FormatTimestampUTC(const std::chrono::system_clock::time_point& timepoint);
    std::string FormatTimestampLocal(const std::chrono::system_clock::time_point& timepoint);

    // Initialize and start active app monitoring
    bool InitializeActiveAppMonitoring();

    // Stop active app monitoring and cleanup
    void CleanupActiveAppMonitoring();

    // Get recent active apps within the specified time period
    std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList();

    // Battery Status
    int GetBatteryPercentage();
    bool IsCharging();

    // System Performance
    double GetCPUUsage();
    double GetMemoryUsage();
    double GetMemoryUsed();
    double GetTotalMemory();

    // Network Status
    bool IsNetworkConnected();
    std::string GetNetworkType();
    double GetNetworkSpeed();

    // Timestamp
    std::string GetCurrentTimestamp();
}