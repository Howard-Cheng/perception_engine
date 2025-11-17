#include "WindowsAPIs.h"
#include "WindowEventMonitor.h"
#include "BrowserContentExtractor.h"
#include "AsyncTaskQueue.h"  // ✅ NEW: Include async task queue
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
    {
        m_lastAppStartTime = std::chrono::system_clock::now();
        m_lastLocationUpdate = std::chrono::steady_clock::now();
        
        // ✅ NEW: Create and start async task queue for callbacks
        m_callbackTaskQueue = std::make_unique<AsyncTaskQueue>("WindowSwitchCallbackQueue");
        m_callbackTaskQueue->Start();
        std::cout << "[WindowsAPIsManager] Async task queue started" << std::endl;
    }

    // Destructor
    WindowsAPIsManager::~WindowsAPIsManager() {
        CleanupActiveAppMonitoring();
        
        // ✅ NEW: Stop async task queue
        if (m_callbackTaskQueue) {
            m_callbackTaskQueue->Stop();
            m_callbackTaskQueue.reset();
            std::cout << "[WindowsAPIsManager] Async task queue stopped" << std::endl;
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
        // ✅ Debug logs
        std::cout << "[DEBUG] OnWindowEventInternal called!" << std::endl;
        std::cout << "  App: " << WideStringToUtf8(info.processName) << std::endl;
        std::cout << "  Window: " << WideStringToUtf8(info.windowTitle) << std::endl;

        try {
            std::string appName = GetAppNameFromWindowInfo(info);
            std::string windowTitle = WideStringToUtf8(info.windowTitle);

            std::cout << "  Processed App: " << appName << std::endl;
            std::cout << "  Processed Window: " << windowTitle << std::endl;

            // Skip empty, invalid, or system app names
            if (appName.empty() || appName == "Unknown" || appName == "Desktop" || appName == "csc_ui") {
                std::cout << "  -> Skipped (invalid app)" << std::endl;
                return;
            }

            std::lock_guard<std::mutex> lock(m_historyMutex);

            auto now = std::chrono::system_clock::now();

            // Check if this is a different app OR different window title
            bool shouldRecord = false;
            if (!m_lastActiveApp.empty() &&
                (m_lastActiveApp != appName || m_lastActiveAppWindowTitle != windowTitle)) {
                shouldRecord = true;
                std::cout << "  -> Should record: YES (different app/window)" << std::endl;
            }
            else {
                std::cout << "  -> Should record: NO (same app/window or first time)" << std::endl;
            }

            // Record the previous app's duration if it's different
            if (shouldRecord) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAppStartTime);

                std::cout << "  -> Duration: " << duration.count() << " seconds" << std::endl;

                // Only record if the app was active for more than 2 seconds
                if (duration.count() > 2) {
                    int durationSecs = static_cast<int>(duration.count());

                    // Generate unique key from appName + windowTitle
                    std::string key = MakeAppKey(m_lastActiveApp, m_lastActiveAppWindowTitle);

                    // App info to be inserted/updated
                    ActiveAppRecord record;
                    record.appName = m_lastActiveApp;
                    record.windowTitle = m_lastActiveAppWindowTitle;
                    record.timestamp = m_lastAppStartTime;
                    record.durationSeconds = durationSecs;
                    record.appContent = m_lastActiveAppContent;
                    m_activeAppHistory[key] = record;

                    std::cout << "  -> ✅ RECORDED: " << m_lastActiveApp << " (" << durationSecs << "s)" << std::endl;

                    // ✅ NEW: Post callback to async task queue (non-blocking)
                    ProcessWindowSwitchAsync(record);
                }
                else {
                    std::cout << "  -> Skipped (duration too short: " << duration.count() << "s)" << std::endl;
                }
            }

            // Update current active app and window title
            m_lastActiveApp = appName;
            m_lastActiveAppWindowTitle = windowTitle;
            m_lastAppStartTime = now;
            m_lastActiveAppContent = GetCurrentActiveAppContent();

            std::cout << "  -> Updated current app: " << m_lastActiveApp << std::endl;

            // Clean up old records periodically
            static auto lastCleanup = std::chrono::system_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - lastCleanup).count() >= 5) {
                CleanupOldRecords();
                lastCleanup = now;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[ERROR] OnWindowEventInternal exception: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "[ERROR] OnWindowEventInternal unknown exception" << std::endl;
        }
    }

    // ✅ NEW: Process window switch callback asynchronously
    void WindowsAPIsManager::ProcessWindowSwitchAsync(const ActiveAppRecord& record) {
        if (!m_callbackTaskQueue || !m_callbackTaskQueue->IsRunning()) {
            std::cerr << "[WindowsAPIsManager] Callback task queue not running!" << std::endl;
            return;
        }
        
        // Post task to async queue (non-blocking)
        m_callbackTaskQueue->PostTask([this, record]() {
            try {
                std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
                if (m_windowSwitchCallback) {
                    std::cout << "  -> ✅ ASYNC CALLBACK triggered!" << std::endl;
                    
                    // Execute callback in background thread
                    m_windowSwitchCallback(record);
                    
                    std::cout << "  -> ✅ ASYNC CALLBACK completed!" << std::endl;
                } else {
                    std::cout << "  -> ❌ No callback registered" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[WindowSwitchCallback] Exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[WindowSwitchCallback] Unknown exception" << std::endl;
            }
        });
        
        std::cout << "  -> Task posted to async queue (pending: " 
                  << m_callbackTaskQueue->GetPendingTaskCount() << ")" << std::endl;
    }

    // Helper function: Generate unique key
    std::string WindowsAPIsManager::MakeAppKey(const std::string& appName, const std::string& windowTitle) {
        return appName + "|" + windowTitle;
    }

    // Get app name from WindowInfo
    std::string WindowsAPIsManager::GetAppNameFromWindowInfo(const WindowInfo& info) {
        try {
            std::string processName = WideStringToUtf8(info.processName);

            if (!processName.empty() && processName != "Unknown") {
                size_t dotPos = processName.find_last_of('.');
                if (dotPos != std::string::npos) {
                    processName = processName.substr(0, dotPos);
                }
                return processName;
            }

            std::string windowTitle = WideStringToUtf8(info.windowTitle);
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

    // Get current active app content
    std::string WindowsAPIsManager::GetCurrentActiveAppContent() {
        try {
            std::lock_guard<std::mutex> lock(m_extractorMutex);

            if (!m_extractorInitialized) {
                m_contentExtractor = std::make_unique<BrowserContentExtractor>();
                m_extractorInitialized = true;
            }

            HWND hwnd = GetForegroundWindow();
            if (!hwnd) {
                return "";
            }

            BrowserContentInfo info;
            bool success = m_contentExtractor->GetBrowserContentByHWND(hwnd, info);

            if (!success || info.textContent.empty()) {
                return "";
            }

            return WideStringToUtf8(info.textContent);
        }
        catch (...) {
            return "";
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
                    std::string title = WideStringToUtf8(buffer);

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

                            std::string exeNameUtf8 = WideStringToUtf8(exeName);

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

    std::string GetCurrentActiveAppContent() {
        return WindowsAPIsManager::GetInstance().GetCurrentActiveAppContent();
    }

    // Helper function: Convert Unicode string to UTF-8
    std::string WideStringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();

        try {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string strTo(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
            return strTo;
        }
        catch (...) {
            return "Unknown";
        }
    }

    // Overload version: Convert wchar_t* to UTF-8
    std::string WideStringToUtf8(const wchar_t* wstr) {
        if (!wstr || wcslen(wstr) == 0) return std::string();
        return WideStringToUtf8(std::wstring(wstr));
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
            catch (winrt::hresult_error const& e) {
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