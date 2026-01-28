#include "platform/WindowsAPIs.h"
#include "platform/WindowEventMonitor.h"
#include "platform/BrowserContentExtractor.h"
#include "utils/AsyncTaskQueue.h"  // ? NEW: Include async task queue
#include "pe_base/windows_helper.h" // For WideStringToUtf8
#include "pe_base/config_manager.h"
#include "pe_base/logger.h"  // NEW: Use logger instead of iostream
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include <windows.h>
#include <netlistmgr.h>
#include <comdef.h>
#include <wlanapi.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>
#include <wininet.h>
#include <pdh.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <mutex>
#include <memory>
#include <string>

// WinRT Headers for Geolocation
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Geolocation.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace WindowsAPIs {

    // Constants
    static const std::chrono::hours HISTORY_RETENTION_PERIOD{ 1 }; // 1 hour retention
    static const std::chrono::minutes LOCATION_CACHE_DURATION{ 30 }; // 30 minutes cache

#pragma region "WindowsAPIsManager Implementation"

    // Singleton instance
    WindowsAPIsManager& WindowsAPIsManager::GetInstance() {
        static WindowsAPIsManager instance;
        return instance;
    }

    // Constructor
    WindowsAPIsManager::WindowsAPIsManager()
        : m_extractorInitialized(false)
        , m_locationInitialized(false)
        , m_lastActiveAppContent(std::make_unique<BrowserContentInfo>())  // ? NEW: Initialize unique_ptr
    {
        m_lastAppStartTime = std::chrono::system_clock::now();
        m_lastLocationUpdate = std::chrono::steady_clock::now();
        
        // ? NEW: Create and start async task queue for callbacks
        m_callbackTaskQueue = std::make_unique<AsyncTaskQueue>("WindowSwitchCallbackQueue");
        m_callbackTaskQueue->Start();
        PE_INFO("[WindowsAPIsManager] Async task queue started");
    }

    // Destructor
    WindowsAPIsManager::~WindowsAPIsManager() {
        CleanupActiveAppMonitoring();
        
        // ? NEW: Stop async task queue
        if (m_callbackTaskQueue) {
            m_callbackTaskQueue->Stop();
            m_callbackTaskQueue.reset();
            PE_INFO("[WindowsAPIsManager] Async task queue stopped");
        }
    }

    // Initialize active app monitoring
    bool WindowsAPIsManager::InitializeActiveAppMonitoring() {
        try {
            std::lock_guard<std::mutex> lock(m_historyMutex);

            // Initialize monitor if not already created
            if (!m_eventMonitor) {
                m_eventMonitor = std::make_unique<WindowEventMonitor>();
            }
            //Store in unique_ptr
            if (!m_lastActiveAppContent) {
                m_lastActiveAppContent = std::make_unique<BrowserContentInfo>();
            }

            // Register callback for window events (using lambda to capture 'this')
            m_eventMonitor->RegisterCallback([this](const WindowInfo& info) {
                this->OnWindowEventInternal(info);
                });

            // Start monitoring
            bool success = m_eventMonitor->Start();
            if (success) {
                // Initialize with current active app and window title
                std::string currentApp = GetForegroundAppName();
                if (!currentApp.empty() && currentApp != "Unknown") {
                    m_lastActiveApp = currentApp;
                    m_lastActiveAppWindowTitle = currentApp;
                    m_lastAppStartTime = std::chrono::system_clock::now();
                }
            }

            return success;
        }
        catch (...) {
            return false;
        }
    }

    // Cleanup active app monitoring
    void WindowsAPIsManager::CleanupActiveAppMonitoring() {
        try {
            std::lock_guard<std::mutex> lock(m_historyMutex);

            // Stop monitoring
            if (m_eventMonitor) {
                m_eventMonitor->Stop();
                m_eventMonitor.reset();
            }

            // Clear history and current app info
            m_activeAppHistory.clear();
            m_lastActiveApp.clear();
            m_lastActiveAppWindowTitle.clear();
        }
        catch (...) {
            // Ignore cleanup errors
        }

        // Cleanup BrowserContentExtractor
        try {
            std::lock_guard<std::mutex> extractorLock(m_extractorMutex);
            m_contentExtractor.reset();
            m_extractorInitialized = false;
        }
        catch (...) {
            // Ignore cleanup errors
        }
    }

    // Register window switch callback
    void WindowsAPIsManager::RegisterWindowSwitchCallback(WindowSwitchCallback callback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_windowSwitchCallback = callback;
    }

    // Clear window switch callback
    void WindowsAPIsManager::ClearWindowSwitchCallback() {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_windowSwitchCallback = nullptr;
    }

    // Internal window event handler
    void WindowsAPIsManager::OnWindowEventInternal(const WindowInfo& info) {
        //std::cout << "  App: " << pe_base::WindowsHelper::ConvertToChar(info.processName.c_str()).ToString() << std::endl;
        //std::cout << "  Window: " << pe_base::WindowsHelper::ConvertToChar(info.windowTitle.c_str()).ToString() << std::endl;

        try {
            std::string appName = GetAppNameFromWindowInfo(info);
            std::string windowTitle = pe_base::WindowsHelper::ConvertToChar(info.windowTitle.c_str()).ToString();

            // ? NEW: Use ConfigManager to check blacklist instead of hardcoded list
            auto& config = pe_base::ConfigManager::GetInstance();
            if (appName.empty() || config.IsBlacklisted(appName)) {
                //std::cout << "  -> Skipped (invalid app or blacklisted)" << std::endl;
                return;
            }

            PE_DEBUG("[DEBUG] OnWindowEventInternal called!");
            PE_DEBUG("  Processed App: " << appName);
            PE_DEBUG("  Processed Window: " << windowTitle);

            auto now = std::chrono::system_clock::now();

            // Check if this is a different app OR different window title
            bool shouldRecord = false;
            if (!m_lastActiveApp.empty() &&
                (m_lastActiveApp != appName || m_lastActiveAppWindowTitle != windowTitle)) {
                shouldRecord = true;
                PE_DEBUG("  -> Should record: YES (different app/window)");
            }
            else {
                PE_DEBUG("  -> Should record: NO (same app/window or first time)");
            }

            // Record the previous app's duration if it's different
            if (shouldRecord) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAppStartTime);

                PE_DEBUG("  -> Duration: " << duration.count() << " seconds");

                // ? UPDATED: Check if textContent is not empty (using unique_ptr)
                if (duration.count() > 2 && m_lastActiveAppContent && !m_lastActiveAppContent->textContent.empty()) {
                    int durationSecs = static_cast<int>(duration.count());

                    // Generate unique key from appName + windowTitle
                    std::string key = MakeAppKey(m_lastActiveApp, m_lastActiveAppWindowTitle);

                    // App info to be inserted/updated
                    ActiveAppRecord record;
                    record.appName = m_lastActiveApp;
                    record.windowTitle = m_lastActiveAppWindowTitle;
                    record.timestamp = m_lastAppStartTime;
                    record.durationSeconds = durationSecs;
                    // ? UPDATED: Convert BrowserContentInfo to string for ActiveAppRecord
                    record.appContent = pe_base::WindowsHelper::ConvertToChar(m_lastActiveAppContent->textContent.c_str()).ToString();
                    //  Extract and store URL from BrowserContentInfo
                    if (!m_lastActiveAppContent->url.empty()) {
                        record.url = pe_base::WindowsHelper::ConvertToChar(m_lastActiveAppContent->url.c_str()).ToString();
                    } else {
                        record.url = "";  // Empty string for non-browser apps
                    }
                    m_activeAppHistory[key] = record;

                    PE_DEBUG("  -> RECORDED: " << m_lastActiveApp << " (" << durationSecs << "s)");

                    // Post callback to async task queue (non-blocking)
                    ProcessWindowSwitchAsync(record);
                }
                else {
                    PE_DEBUG("  -> Skipped (duration too short: " << duration.count() << "s)");
                }
            }

            BrowserContentInfo newAppContent = GetCurrentActiveAppContent();
            // Update current active app and window title
            m_lastActiveApp = appName;
            m_lastActiveAppWindowTitle = pe_base::WindowsHelper::ConvertToChar(newAppContent.title.c_str()).ToString();
            m_lastAppStartTime = now;
            *m_lastActiveAppContent = newAppContent;

            PE_DEBUG("  -> Updated current app: " << m_lastActiveApp);

            // Clean up old records periodically
            static auto lastCleanup = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - lastCleanup).count() >= 5) {
                CleanupOldRecords();
                lastCleanup = now;
            }
        }
        catch (const std::exception& e) {
            PE_ERROR("[ERROR] OnWindowEventInternal exception: " << e.what());
        }
        catch (...) {
            PE_ERROR("[ERROR] OnWindowEventInternal unknown exception");
        }
    }

    // ? NEW: Process window switch callback asynchronously
    void WindowsAPIsManager::ProcessWindowSwitchAsync(const ActiveAppRecord& record) {
        if (!m_callbackTaskQueue || !m_callbackTaskQueue->IsRunning()) {
            PE_ERROR("[WindowsAPIsManager] Callback task queue not running!");
            return;
        }
        
        // Post task to async queue (non-blocking)
        m_callbackTaskQueue->PostTask([this, record]() {
            try {
                std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
                if (m_windowSwitchCallback) {
                    PE_DEBUG("  -> ? ASYNC CALLBACK triggered!");
                    
                    // Execute callback in background thread
                    m_windowSwitchCallback(record);
                    
                    PE_DEBUG("  -> ? ASYNC CALLBACK completed!");
                } else {
                    PE_DEBUG("  -> ? No callback registered");
                }
            } catch (const std::exception& e) {
                PE_ERROR("[WindowSwitchCallback] Exception: " << e.what());
            } catch (...) {
                PE_ERROR("[WindowSwitchCallback] Unknown exception");
            }
        });
        
        PE_DEBUG("  -> Task posted to async queue (pending: " << m_callbackTaskQueue->GetPendingTaskCount() << ")");
    }

    // Helper function: Generate unique key
    std::string WindowsAPIsManager::MakeAppKey(const std::string& appName, const std::string& windowTitle) {
        return appName + "|" + windowTitle;
    }

    // Get app name from WindowInfo
    std::string WindowsAPIsManager::GetAppNameFromWindowInfo(const WindowInfo& info) {
        try {
            std::string processName = pe_base::WindowsHelper::ConvertToChar(info.processName.c_str()).ToString();

            if (!processName.empty() && processName != "Unknown") {
                size_t dotPos = processName.find_last_of('.');
                if (dotPos != std::string::npos) {
                    processName = processName.substr(0, dotPos);
                }
                return processName;
            }

            std::string windowTitle = pe_base::WindowsHelper::ConvertToChar(info.windowTitle.c_str()).ToString();
            if (!windowTitle.empty()) {
                return windowTitle;
            }

            return "Unknown";
        }
        catch (...) {
            return "Unknown";
        }
    }

    // Cleanup old records
    void WindowsAPIsManager::CleanupOldRecords() {
        try {
            auto now = std::chrono::system_clock::now();
            auto cutoff = now - HISTORY_RETENTION_PERIOD;

            for (auto it = m_activeAppHistory.begin(); it != m_activeAppHistory.end(); ) {
                if (it->second.timestamp < cutoff) {
                    it = m_activeAppHistory.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
        catch (...) {
            // Ignore cleanup errors
        }
    }

    // Get recent active app list
    std::vector<ActiveAppRecord> WindowsAPIsManager::GetRecentPeriodActiveAppList() {
        try {
            std::lock_guard<std::mutex> lock(m_historyMutex);

            CleanupOldRecords();

            std::vector<ActiveAppRecord> result;
            result.reserve(m_activeAppHistory.size() + 1);

            for (const auto& pair : m_activeAppHistory) {
                result.push_back(pair.second);
            }

            // Add current active app
            auto now = std::chrono::system_clock::now();
            if (!m_lastActiveApp.empty() && m_lastActiveApp != "Unknown" && m_lastActiveApp != "Desktop") {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAppStartTime);
                if (duration.count() > 0) {
                    int durationSecs = static_cast<int>(duration.count());

                    std::string key = MakeAppKey(m_lastActiveApp, m_lastActiveAppWindowTitle);

                    auto it = m_activeAppHistory.find(key);
                    if (it != m_activeAppHistory.end()) {
                        for (auto& record : result) {
                            if (record.appName == m_lastActiveApp &&
                                record.windowTitle == m_lastActiveAppWindowTitle) {
                                record.durationSeconds += durationSecs;
                                record.timestamp = m_lastAppStartTime;
                                break;
                            }
                        }
                    }
                    else {
                        ActiveAppRecord currentRecord;
                        currentRecord.appName = m_lastActiveApp;
                        currentRecord.windowTitle = m_lastActiveAppWindowTitle;
                        currentRecord.timestamp = m_lastAppStartTime;
                        currentRecord.durationSeconds = durationSecs;
                        result.push_back(currentRecord);
                    }
                }
            }

            return result;
        }
        catch (...) {
            return std::vector<ActiveAppRecord>();
        }
    }

    //  Get current active app content - now returns BrowserContentInfo
    BrowserContentInfo WindowsAPIsManager::GetCurrentActiveAppContent() {
        // Reentrancy guard - if we're already extracting content, return empty
        // This prevents deadlock when COM calls during UI Automation trigger recursive window events
        if (m_isExtractingContent.exchange(true)) {
            PE_DEBUG("[GetCurrentActiveAppContent] Skipping - already extracting (reentrancy guard)");
            return BrowserContentInfo();  // ? UPDATED: Return empty BrowserContentInfo
        }
        
        // RAII guard to reset the flag when we exit
        struct ReentrancyGuard {
            std::atomic<bool>& flag;
            ~ReentrancyGuard() { flag.store(false); }
        } guard{m_isExtractingContent};
        
        try {
            std::lock_guard<std::mutex> lock(m_extractorMutex);

            if (!m_extractorInitialized) {
                m_contentExtractor = std::make_unique<BrowserContentExtractor>();
                m_extractorInitialized = true;
            }

            HWND hwnd = GetForegroundWindow();
            if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
                PE_DEBUG("[GetCurrentActiveAppContent] Invalid or invisible window, returning empty");
                return BrowserContentInfo();  // ? UPDATED: Return empty BrowserContentInfo
            }

            BrowserContentInfo info;
            bool success = false;
            
            // Add exception handling around the call
            try {
                success = m_contentExtractor->GetBrowserContentByHWND(hwnd, info);
            }
            catch (const std::exception& e) {
                PE_ERROR("[GetCurrentActiveAppContent] Exception during content extraction: " << e.what());
                return BrowserContentInfo();
            }
            catch (...) {
                PE_ERROR("[GetCurrentActiveAppContent] Unknown exception during content extraction");
                return BrowserContentInfo();
            }

            if (!success || info.textContent.empty()) {
                return BrowserContentInfo();  // ? UPDATED: Return empty BrowserContentInfo
            }

            return info;  // ? UPDATED: Return full BrowserContentInfo object
        }
        catch (const std::exception& e) {
            PE_ERROR("[GetCurrentActiveAppContent] Outer exception: " << e.what());
            return BrowserContentInfo();  // ? UPDATED: Return empty BrowserContentInfo
        }
        catch (...) {
            PE_ERROR("[GetCurrentActiveAppContent] Unknown outer exception");
            return BrowserContentInfo();  // ? UPDATED: Return empty BrowserContentInfo
        }
    }

    // Get foreground app name (member function)
    std::string WindowsAPIsManager::GetForegroundAppName() {
        // Call the namespace function
        return WindowsAPIs::GetForegroundAppName();
    }

#pragma endregion "WindowsAPIsManager Implementation"

#pragma region "Compatibility Layer - Namespace Functions"

    // Forward to singleton instance
    std::string GetForegroundAppName() {
        // Keep original implementation for standalone use
        try {
            HWND hwnd = GetForegroundWindow();
            if (hwnd) {
                wchar_t buffer[512] = { 0 };
                int result = GetWindowTextW(hwnd, buffer, sizeof(buffer) / sizeof(wchar_t) - 1);

                if (result > 0 && wcslen(buffer) > 0) {
                    std::string title = pe_base::WindowsHelper::ConvertToChar(buffer).ToString();

                    if (title != "Program Manager" &&
                        title != "Desktop" &&
                        title != "" &&
                        title.find("Windows Default Lock Screen") == std::string::npos) {
                        return title;
                    }
                }
            }

            DWORD processId = 0;
            if (hwnd) {
                GetWindowThreadProcessId(hwnd, &processId);
            }

            if (processId > 0) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
                if (hProcess) {
                    wchar_t processName[MAX_PATH] = { 0 };
                    DWORD size = sizeof(processName) / sizeof(wchar_t);

                    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
                        std::wstring fullPath(processName);
                        size_t lastSlash = fullPath.find_last_of(L"\\/");
                        if (lastSlash != std::wstring::npos) {
                            std::wstring exeName = fullPath.substr(lastSlash + 1);

                            size_t dotPos = exeName.find_last_of(L'.');
                            if (dotPos != std::wstring::npos) {
                                exeName = exeName.substr(0, dotPos);
                            }

                            std::string exeNameUtf8 = pe_base::WindowsHelper::ConvertToChar(exeName.c_str()).ToString();

                            if (exeNameUtf8 != "dwm" &&
                                exeNameUtf8 != "winlogon" &&
                                exeNameUtf8 != "csrss" &&
                                exeNameUtf8 != "explorer" &&
                                !exeNameUtf8.empty()) {
                                CloseHandle(hProcess);
                                return exeNameUtf8;
                            }
                        }
                    }
                    CloseHandle(hProcess);
                }
            }

            return "Unknown";
        }
        catch (...) {
            return "Unknown";
        }
    }

    bool InitializeActiveAppMonitoring() {
        return WindowsAPIsManager::GetInstance().InitializeActiveAppMonitoring();
    }

    void CleanupActiveAppMonitoring() {
        WindowsAPIsManager::GetInstance().CleanupActiveAppMonitoring();
    }

    std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList() {
        return WindowsAPIsManager::GetInstance().GetRecentPeriodActiveAppList();
    }

    // ? UPDATED: Backward compatibility - converts BrowserContentInfo to string
    std::string GetCurrentActiveAppContent() {
        BrowserContentInfo info = WindowsAPIsManager::GetInstance().GetCurrentActiveAppContent();
        // Convert textContent to UTF-8 string
        if (!info.textContent.empty()) {
            return pe_base::WindowsHelper::ConvertToChar(info.textContent.c_str()).ToString();
        }
        return "";
    }

    int GetBatteryPercentage() {
        try {
            SYSTEM_POWER_STATUS status = { 0 };
            if (GetSystemPowerStatus(&status)) {
                if (status.BatteryLifePercent == 255) {
                    return -1;
                }
                return static_cast<int>(status.BatteryLifePercent);
            }
            return -1;
        }
        catch (...) {
            return -1;
        }
    }

    bool IsCharging() {
        try {
            SYSTEM_POWER_STATUS status = { 0 };
            if (GetSystemPowerStatus(&status)) {
                return (status.ACLineStatus == 1);
            }
            return false;
        }
        catch (...) {
            return false;
        }
    }

    double GetCPUUsage() {
        try {
            static FILETIME s_ftPrevSysIdle = { 0 };
            static FILETIME s_ftPrevSysKernel = { 0 };
            static FILETIME s_ftPrevSysUser = { 0 };
            static bool s_firstCall = true;

            FILETIME ftSysIdle, ftSysKernel, ftSysUser;

            if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
                return -1.0;
            }

            if (s_firstCall) {
                s_ftPrevSysIdle = ftSysIdle;
                s_ftPrevSysKernel = ftSysKernel;
                s_ftPrevSysUser = ftSysUser;
                s_firstCall = false;
                return 0.0;
            }

            ULARGE_INTEGER sysIdle, sysKernel, sysUser;
            ULARGE_INTEGER prevSysIdle, prevSysKernel, prevSysUser;

            sysIdle.LowPart = ftSysIdle.dwLowDateTime;
            sysIdle.HighPart = ftSysIdle.dwHighDateTime;
            sysKernel.LowPart = ftSysKernel.dwLowDateTime;
            sysKernel.HighPart = ftSysKernel.dwHighDateTime;
            sysUser.LowPart = ftSysUser.dwLowDateTime;
            sysUser.HighPart = ftSysUser.dwHighDateTime;

            prevSysIdle.LowPart = s_ftPrevSysIdle.dwLowDateTime;
            prevSysIdle.HighPart = s_ftPrevSysIdle.dwHighDateTime;
            prevSysKernel.LowPart = s_ftPrevSysKernel.dwLowDateTime;
            prevSysKernel.HighPart = s_ftPrevSysKernel.dwHighDateTime;
            prevSysUser.LowPart = s_ftPrevSysUser.dwLowDateTime;
            prevSysUser.HighPart = s_ftPrevSysUser.dwHighDateTime;

            ULONGLONG idleDiff = sysIdle.QuadPart - prevSysIdle.QuadPart;
            ULONGLONG kernelDiff = sysKernel.QuadPart - prevSysKernel.QuadPart;
            ULONGLONG userDiff = sysUser.QuadPart - prevSysUser.QuadPart;

            ULONGLONG systemDiff = kernelDiff + userDiff;
            ULONGLONG totalDiff = systemDiff;

            double cpuUsage = 0.0;
            if (totalDiff > 0) {
                cpuUsage = (double)(totalDiff - idleDiff) * 100.0 / (double)totalDiff;
            }

            s_ftPrevSysIdle = ftSysIdle;
            s_ftPrevSysKernel = ftSysKernel;
            s_ftPrevSysUser = ftSysUser;

            if (cpuUsage < 0.0) cpuUsage = 0.0;
            if (cpuUsage > 100.0) cpuUsage = 100.0;

            return cpuUsage;
        }
        catch (...) {
            return -1.0;
        }
    }

    double GetMemoryUsage() {
        try {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                return static_cast<double>(memInfo.dwMemoryLoad);
            }
            return -1.0;
        }
        catch (...) {
            return -1.0;
        }
    }

    double GetMemoryUsed() {
        try {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                double usedMemoryBytes = static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys);
                double usedMemoryGB = usedMemoryBytes / (1024.0 * 1024.0 * 1024.0);
                return usedMemoryGB;
            }
            return -1.0;
        }
        catch (...) {
            return -1.0;
        }
    }

    double GetTotalMemory() {
        try {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                double totalMemoryBytes = static_cast<double>(memInfo.ullTotalPhys);
                double totalMemoryGB = totalMemoryBytes / (1024.0 * 1024.0 * 1024.0);
                return totalMemoryGB;
            }
            return -1.0;
        }
        catch (...) {
            return -1.0;
        }
    }

    bool IsNetworkConnected() {
        try {
            HRESULT hr = CoInitialize(NULL);
            if (FAILED(hr)) return false;

            INetworkListManager* pNetworkListManager = nullptr;
            hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL,
                IID_INetworkListManager, (LPVOID*)&pNetworkListManager);

            if (SUCCEEDED(hr) && pNetworkListManager) {
                NLM_CONNECTIVITY connectivity;
                hr = pNetworkListManager->GetConnectivity(&connectivity);
                pNetworkListManager->Release();

                CoUninitialize();

                if (SUCCEEDED(hr)) {
                    return (connectivity & NLM_CONNECTIVITY_IPV4_INTERNET) ||
                        (connectivity & NLM_CONNECTIVITY_IPV6_INTERNET);
                }
            }

            CoUninitialize();
            return false;
        }
        catch (...) {
            return false;
        }
    }

    std::string GetNetworkType() {
        try {
            HANDLE hClient = nullptr;
            DWORD dwMaxClient = 2;
            DWORD dwCurVersion = 0;
            DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);

            if (dwResult == ERROR_SUCCESS) {
                PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
                dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);

                if (dwResult == ERROR_SUCCESS && pIfList) {
                    for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                        WLAN_INTERFACE_INFO* pIfInfo = &pIfList->InterfaceInfo[i];
                        if (pIfInfo->isState == wlan_interface_state_connected) {
                            WlanFreeMemory(pIfList);
                            WlanCloseHandle(hClient, NULL);
                            return "WiFi";
                        }
                    }
                    WlanFreeMemory(pIfList);
                }
                WlanCloseHandle(hClient, NULL);
            }

            if (IsNetworkConnected()) {
                return "Ethernet";
            }

            return "None";
        }
        catch (...) {
            return "Unknown";
        }
    }

    double GetNetworkSpeed() {
        // Stub implementation - returning 0.0 for now
        return 0.0;
    }


    Location WindowsAPIsManager::GetLocation() {
        using namespace winrt;
        using namespace Windows::Devices::Geolocation;
        using namespace Windows::Foundation;

        Location loc;
        loc.latitude = 0.0;
        loc.longitude = 0.0;
        loc.valid = false;

        try {
            auto now = std::chrono::steady_clock::now();

            // Optimization: Skip location request in first 30 seconds after startup
            static auto startupTime = std::chrono::steady_clock::now();
            if ((now - startupTime) < std::chrono::seconds(30)) {
                if (m_locationInitialized) {
                    return m_cachedLocation;
                }
                else {
                    m_cachedLocation = loc;
                    m_lastLocationUpdate = now;
                    m_locationInitialized = true;
                    return loc;
                }
            }

            // Return cached location if still valid
            if (m_locationInitialized && m_cachedLocation.valid &&
                (now - m_lastLocationUpdate) < LOCATION_CACHE_DURATION) {
                return m_cachedLocation;
            }

            // Initialize WinRT if not done
            static bool winrtInitialized = false;
            if (!winrtInitialized) {
                try {
                    init_apartment(apartment_type::single_threaded);
                    winrtInitialized = true;
                }
                catch (...) {
                    // If already initialized, continue
                }
            }

            // Request location access
            GeolocationAccessStatus access;
            try {
                access = Geolocator::RequestAccessAsync().get();
            }
            catch (...) {
                // Permission request failed, update cache and return invalid location
                m_cachedLocation = loc;
                m_lastLocationUpdate = now;
                m_locationInitialized = true;
                return loc;
            }

            if (access != GeolocationAccessStatus::Allowed) {
                // Access denied, update cache and return invalid location
                m_cachedLocation = loc;
                m_lastLocationUpdate = now;
                m_locationInitialized = true;
                return loc;
            }

            // Create geolocator
            Geolocator locator;
            locator.DesiredAccuracyInMeters(100); // 100 meter accuracy

            // Get position with timeout
            try {
                Geoposition pos = locator.GetGeopositionAsync().get();
                auto basicPos = pos.Coordinate().Point().Position();

                loc.latitude = basicPos.Latitude;
                loc.longitude = basicPos.Longitude;
                loc.valid = true;

                // Cache the successful result
                m_cachedLocation = loc;
                m_lastLocationUpdate = now;
                m_locationInitialized = true;

                return loc;
            }
            catch (winrt::hresult_error const&) {
                // Location request failed, cache invalid result for short time
                m_cachedLocation = loc;
                m_lastLocationUpdate = now;
                m_locationInitialized = true;
                return loc;
            }
        }
        catch (...) {
            // Any other error, return invalid location
            auto now = std::chrono::steady_clock::now();
            m_cachedLocation = loc;
            m_lastLocationUpdate = now;
            m_locationInitialized = true;
            return loc;
        }
    }

    size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total = size * nmemb;
        output->append(static_cast<char*>(contents), total);
        return total;
    }

    std::string curlHttpsGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;
        if (!curl) {
            return "";
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        if (curl_easy_perform(curl) != CURLE_OK) {
            response = "";
        }
        curl_easy_cleanup(curl);
        return response;
    }

    Location WindowsAPIsManager::GetOnlineLocation()
    {
        Location loc;
        using namespace winrt;
        using namespace Windows::Devices::Geolocation;
        using namespace Windows::Foundation;

        Geolocator locator;
        locator.DesiredAccuracy(PositionAccuracy::High);
        locator.ReportInterval(1000);
        PE_INFO("Getting high-precision latitude and longitude... (Please ensure location services are enabled and authorized)");

        std::optional<Geoposition> pos_opt;
        bool location_success = false;
        for (int retry = 0; retry < 3; ++retry) {
            try {
                pos_opt = locator.GetGeopositionAsync().get();
                location_success = true;
                break;
            }
            catch (const hresult_error&) {
                PE_WARN("Location attempt " << (retry + 1) << " failed, retrying...");
                return loc;
            }
        }

        if (!location_success || !pos_opt) {
            return loc;
        }

        // 4. Parse latitude and longitude
        const Geoposition& pos = *pos_opt;
        double lat = pos.Coordinate().Point().Position().Latitude;
        double lon = pos.Coordinate().Point().Position().Longitude;

        loc.latitude = lat;
        loc.longitude = lon;
        loc.valid = true;

        // Get configuration from ConfigManager
        auto& config = pe_base::ConfigManager::GetInstance();
        std::string baseUrl = config.GetOnlineLocationBaseUrl();
        std::string format = config.GetOnlineLocationFormat();
        int addressDetails = config.GetOnlineLocationAddressDetails();
        int extraTags = config.GetOnlineLocationExtraTags();
        int zoom = config.GetOnlineLocationZoom();
        std::string email = config.GetOnlineLocationEmail();
        std::string acceptLanguage = config.GetOnlineLocationAcceptLanguage();

        // Build URL with configuration parameters
        std::stringstream url_ss;
        url_ss << baseUrl << "?"
            << "lat=" << lat
            << "&lon=" << lon
            << "&format=" << format
            << "&addressdetails=" << addressDetails
            << "&extratags=" << extraTags
            << "&zoom=" << zoom;
        
        if (!email.empty()) {
            url_ss << "&email=" << email;
        }
        
        if (!acceptLanguage.empty()) {
            url_ss << "&accept-language=" << acceptLanguage;
        }
        
        std::string url = url_ss.str();
        std::string response = curlHttpsGet(url);
        if (!response.empty()) {
            try {
                auto json = nlohmann::json::parse(response);
                std::string display_name = json.value("display_name", "");
                loc.description = display_name;
            }
            catch (...) {
                // ? FIX: Use empty string instead of nullptr
                loc.description = "";
            }
        }
        else {
            // ? FIX: Use empty string instead of nullptr
            loc.description = "";
        }
        return loc;
    }

    std::string GetCurrentTimestamp() {
        try {
            auto now = std::chrono::system_clock::now();
            auto time_t_val = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::ostringstream oss;

            struct tm timeinfo;
            if (localtime_s(&timeinfo, &time_t_val) == 0) {
                oss << std::put_time(&timeinfo, "%Y-%m-%dT%H:%M:%S");
                oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

                char tz_offset[16];
                strftime(tz_offset, sizeof(tz_offset), "%z", &timeinfo);
                std::string tz_str(tz_offset);
                if (tz_str.length() >= 5) {
                    tz_str = tz_str.substr(0, 3) + ":" + tz_str.substr(3);
                }
                else {
                    tz_str = "+00:00";
                }
                oss << tz_str;
            }
            else {
                return "1970-01-01T00:00:00.000+00:00";
            }

            return oss.str();
        }
        catch (...) {
            return "1970-01-01T00:00:00.000+00:00";
        }
    }

#pragma endregion "Compatibility Layer - Namespace Functions"

} // namespace WindowsAPIs