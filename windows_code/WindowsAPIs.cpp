#include "WindowsAPIs.h"
#include "WindowEventMonitor.h"
#include "BrowserContentExtractor.h"
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
#include <iphlpapi.h>  // For GetIfTable
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <mutex>
#include <memory>
#include <string>  // For std::string

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
#pragma comment(lib, "iphlpapi.lib")  // For network interface APIs

namespace WindowsAPIs {

// Active App Monitoring - Global variables
static std::unique_ptr<WindowEventMonitor> g_eventMonitor;
static std::unordered_map<std::string, ActiveAppRecord> g_activeAppHistory;  // Key: appName + "|" + windowTitle
static std::mutex g_historyMutex;
static std::string g_lastActiveApp;
static std::string g_lastActiveAppWindowTitle;
static std::chrono::system_clock::time_point g_lastAppStartTime;
static const std::chrono::hours HISTORY_RETENTION_PERIOD{1}; // 1 hour retention

// Helper function: Generate unique key from appName and windowTitle
inline std::string MakeAppKey(const std::string& appName, const std::string& windowTitle) {
    return appName + "|" + windowTitle;  // Use "|" as separator
}

// Event callback function for window monitoring
void OnWindowEvent(const WindowInfo& info);

// Cleanup old records (older than 1 hour)
void CleanupOldRecords();

// Convert WindowInfo to app name
std::string GetAppNameFromWindowInfo(const WindowInfo& info);

std::string GetForegroundAppName() {
    try {
        // Method 1: Get foreground window title (strongest - supports Unicode)
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            // Use Unicode API version to correctly handle international characters
            wchar_t buffer[512] = {0};  // Unicode buffer
            int result = GetWindowTextW(hwnd, buffer, sizeof(buffer)/sizeof(wchar_t) - 1);
            
            if (result > 0 && wcslen(buffer) > 0) {
                // Convert Unicode to UTF-8
                std::string title = WideStringToUtf8(buffer);
                
                // Filter out some system window titles
                if (title != "Program Manager" && 
                    title != "Desktop" && 
                    title != "" &&
                    title.find("Windows Default Lock Screen") == std::string::npos) {
                    return title;
                }
            }
        }
        
        // Method 2: Get process information (strong)
        DWORD processId = 0;
        if (hwnd) {
            GetWindowThreadProcessId(hwnd, &processId);
        }
        
        // Method 2.1: Fallback to GetFocus if no processId
        if (processId == 0) {
            HWND focusWindow = GetFocus();
            if (focusWindow) {
                GetWindowThreadProcessId(focusWindow, &processId);
            }
        }
        
        // Method 2.2: Fallback to GetActiveWindow
        if (processId == 0) {
            HWND activeWindow = GetActiveWindow();
            if (activeWindow) {
                GetWindowThreadProcessId(activeWindow, &processId);
            }
        }
        
        if (processId > 0) {
            // Enhanced process information retrieval
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (hProcess) {
                wchar_t processName[MAX_PATH] = {0};
                DWORD size = sizeof(processName)/sizeof(wchar_t);
                
                // Prefer QueryFullProcessImageNameW (Unicode version)
                if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
                    CloseHandle(hProcess);
                    
                    std::wstring fullPath(processName);
                    size_t lastSlash = fullPath.find_last_of(L"\\/");
                    if (lastSlash != std::wstring::npos) {
                        std::wstring exeName = fullPath.substr(lastSlash + 1);
                        
                        // Remove .exe extension and format
                        size_t dotPos = exeName.find_last_of(L'.');
                        if (dotPos != std::wstring::npos) {
                            exeName = exeName.substr(0, dotPos);
                        }
                        
                        std::string exeNameUtf8 = WideStringToUtf8(exeName);
                        
                        // Exclude system processes
                        if (exeNameUtf8 != "dwm" && 
                            exeNameUtf8 != "winlogon" && 
                            exeNameUtf8 != "csrss" &&
                            exeNameUtf8 != "explorer" &&
                            !exeNameUtf8.empty()) {
                            return exeNameUtf8;
                        }
                    }
                } else {
                    // Fallback to GetModuleBaseNameW
                    wchar_t baseName[MAX_PATH] = {0};
                    if (GetModuleBaseNameW(hProcess, NULL, baseName, sizeof(baseName)/sizeof(wchar_t))) {
                        CloseHandle(hProcess);
                        
                        std::wstring name(baseName);
                        size_t dotPos = name.find_last_of(L'.');
                        if (dotPos != std::wstring::npos) {
                            name = name.substr(0, dotPos);
                        }
                        
                        std::string nameUtf8 = WideStringToUtf8(name);
                        if (!nameUtf8.empty() && nameUtf8 != "explorer") {
                            return nameUtf8;
                        }
                    }
                }
                CloseHandle(hProcess);
            }
        }
        
        // Method 3: Enhanced window enumeration (smarter scoring + Unicode support)
        struct WindowInfo {
            HWND bestWindow = NULL;
            std::string title;
            DWORD processId = 0;
            int score = 0;  // Window priority score
        };
        
        WindowInfo info;
        
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            WindowInfo* pInfo = reinterpret_cast<WindowInfo*>(lParam);
            
            // Stricter window checks
            if (IsWindowVisible(hwnd) && 
                !IsIconic(hwnd) &&  // Not minimized
                !(GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
                
                wchar_t title[512] = {0};
                int titleLen = GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t) - 1);
                
                if (titleLen > 0) {
                    std::string titleStr = WideStringToUtf8(title);
                    
                    // Calculate window priority score
                    int score = 0;
                    
                    // Exclude system windows
                    if (titleStr == "Program Manager" || 
                        titleStr == "Desktop" ||
                        titleStr.find("Windows Default Lock Screen") != std::string::npos) {
                        return TRUE; // Continue enumeration
                    }
                    
                    // Add score
                    if (titleStr.find(" - ") != std::string::npos) score += 10;  // Has app name separator
                    if (titleLen > 10) score += 5;  // Title is long enough
                    
                    // Get window size, bonus points for larger windows
                    RECT rect;
                    if (GetWindowRect(hwnd, &rect)) {
                        int width = rect.right - rect.left;
                        int height = rect.bottom - rect.top;
                        if (width > 200 && height > 200) score += 5;
                    }
                    
                    // If this window has higher score, select it
                    if (score > pInfo->score) {
                        pInfo->bestWindow = hwnd;
                        pInfo->title = titleStr;
                        pInfo->score = score;
                        GetWindowThreadProcessId(hwnd, &pInfo->processId);
                    }
                }
            }
            return TRUE; // Continue enumeration
        }, reinterpret_cast<LPARAM>(&info));
        
        if (!info.title.empty() && info.score > 0) {
            return info.title;
        }
        
        // Method 4: Last resort process enumeration (fallback)
        // If all methods fail, try to find common user applications
        try {
            DWORD processes[1024];
            DWORD bytesReturned;
            
            if (EnumProcesses(processes, sizeof(processes), &bytesReturned)) {
                DWORD processCount = bytesReturned / sizeof(DWORD);
                
                for (DWORD i = 0; i < processCount; i++) {
                    if (processes[i] != 0) {
                        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
                        if (hProcess) {
                            wchar_t processName[MAX_PATH] = {0};
                            if (GetModuleBaseNameW(hProcess, NULL, processName, sizeof(processName)/sizeof(wchar_t))) {
                                std::wstring name(processName);
                                
                                // Find common user applications
                                if (name.find(L"notepad") != std::wstring::npos ||
                                    name.find(L"calc") != std::wstring::npos ||
                                    name.find(L"chrome") != std::wstring::npos ||
                                    name.find(L"firefox") != std::wstring::npos ||
                                    name.find(L"code") != std::wstring::npos ||
                                    name.find(L"devenv") != std::wstring::npos) {
                                    
                                    CloseHandle(hProcess);
                                    
                                    // Remove .exe extension
                                    size_t dotPos = name.find_last_of(L'.');
                                    if (dotPos != std::wstring::npos) {
                                        name = name.substr(0, dotPos);
                                    }
                                    return WideStringToUtf8(name);
                                }
                            }
                            CloseHandle(hProcess);
                        }
                    }
                }
            }
        } catch (...) {
            // Ignore process enumeration errors
        }
        
        // Method 5: Final fallback (improved)
        // Check if we're in desktop state
        HWND desktopWindow = GetDesktopWindow();
        HWND shellWindow = GetShellWindow();
        
        if (hwnd == desktopWindow || hwnd == shellWindow || hwnd == NULL) {
            // In desktop state, provide better information
            return "Desktop";
        }
        
        // If all methods fail, return enhanced unknown state
        return "Unknown";
    }
    catch (...) {
        return "Unknown";
    }
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
        SYSTEM_POWER_STATUS status = {0};
        if (GetSystemPowerStatus(&status)) {
            if (status.BatteryLifePercent == 255) {
                return -1; // Unknown status
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
        SYSTEM_POWER_STATUS status = {0};
        if (GetSystemPowerStatus(&status)) {
            return (status.ACLineStatus == 1);
        }
        return false;
    }
    catch (...) {
        return false;
    }
}

// CPU usage calculation
double GetCPUUsage() {
    try {
        // Use system timer for CPU usage calculation
        static FILETIME s_ftPrevSysIdle = {0};
        static FILETIME s_ftPrevSysKernel = {0};
        static FILETIME s_ftPrevSysUser = {0};
        static bool s_firstCall = true;
        
        FILETIME ftSysIdle, ftSysKernel, ftSysUser;
        
        // Get system times
        if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
            return -1.0;
        }
        
        // First call: save current values and return 0
        if (s_firstCall) {
            s_ftPrevSysIdle = ftSysIdle;
            s_ftPrevSysKernel = ftSysKernel;
            s_ftPrevSysUser = ftSysUser;
            s_firstCall = false;
            return 0.0;
        }
        
        // Convert to 64-bit values
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
        
        // Calculate time difference
        ULONGLONG idleDiff = sysIdle.QuadPart - prevSysIdle.QuadPart;
        ULONGLONG kernelDiff = sysKernel.QuadPart - prevSysKernel.QuadPart;
        ULONGLONG userDiff = sysUser.QuadPart - prevSysUser.QuadPart;
        
        // Note: kernelDiff includes idle time, need to subtract it
        ULONGLONG systemDiff = kernelDiff + userDiff;
        ULONGLONG totalDiff = systemDiff;
        
        double cpuUsage = 0.0;
        if (totalDiff > 0) {
            // CPU usage = (total time - idle time) / total time * 100
            cpuUsage = (double)(totalDiff - idleDiff) * 100.0 / (double)totalDiff;
        }
        
        // Save current values for next call
        s_ftPrevSysIdle = ftSysIdle;
        s_ftPrevSysKernel = ftSysKernel;
        s_ftPrevSysUser = ftSysUser;
        
        // Ensure value is within valid range
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
            // Return memory usage percentage
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
            // Calculate used memory and convert to GB
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
            // Convert bytes to GB
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
        // Check if connected to WiFi
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
        
        // If not WiFi, assume Ethernet if connected
        if (IsNetworkConnected()) {
            return "Ethernet";
        }
        
        return "None";
    }
    catch (...) {
        return "Unknown";
    }
}

// Network speed calculation using Performance Counters
double GetNetworkSpeed() {
    try {
        // Use traditional GetIfTable API (more reliable)
        static ULONGLONG s_prevBytesReceived = 0;
        static ULONGLONG s_prevBytesSent = 0;
        static std::chrono::steady_clock::time_point s_prevTime;
        static bool s_firstCall = true;
        
        ULONGLONG currentBytesReceived = 0;
        ULONGLONG currentBytesSent = 0;
        auto currentTime = std::chrono::steady_clock::now();
        
        // Use GetIfTable to get network interface information (better compatibility)
        PMIB_IFTABLE pIfTable = nullptr;
        DWORD dwSize = 0;
        
        // Get required buffer size
        DWORD dwRetVal = GetIfTable(pIfTable, &dwSize, 0);
        if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
            pIfTable = (MIB_IFTABLE*)malloc(dwSize);
            if (pIfTable == nullptr) {
                return -1.0;
            }
        } else {
            return -1.0;
        }
        
        // Get interface table
        dwRetVal = GetIfTable(pIfTable, &dwSize, 0);
        if (dwRetVal != NO_ERROR) {
            free(pIfTable);
            return -1.0;
        }
        
        // Iterate through all interfaces, accumulate interface traffic
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW* pIfRow = &pIfTable->table[i];
            
            // Only count active network interfaces (exclude loopback, etc.)
            if (pIfRow->dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL &&
                pIfRow->dwType != MIB_IF_TYPE_LOOPBACK &&
                (pIfRow->dwType == MIB_IF_TYPE_ETHERNET || 
                 pIfRow->dwType == IF_TYPE_IEEE80211 ||
                 pIfRow->dwType == IF_TYPE_GIGABITETHERNET ||
                 pIfRow->dwType == MIB_IF_TYPE_PPP)) {
                
                currentBytesReceived += pIfRow->dwInOctets;
                currentBytesSent += pIfRow->dwOutOctets;
            }
        }
        
        free(pIfTable);
        
        // First call: save current values and return 0
        if (s_firstCall) {
            s_prevBytesReceived = currentBytesReceived;
            s_prevBytesSent = currentBytesSent;
            s_prevTime = currentTime;
            s_firstCall = false;
            return 0.0;
        }
        
        // Calculate time difference
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - s_prevTime);
        double timeSeconds = timeDiff.count() / 1000.0;
        
        if (timeSeconds <= 0.0) {
            return -1.0; // Invalid time
        }
        
        // Calculate traffic difference (handle counter overflow)
        ULONGLONG bytesDiffReceived = 0;
        ULONGLONG bytesDiffSent = 0;
        
        if (currentBytesReceived >= s_prevBytesReceived) {
            bytesDiffReceived = currentBytesReceived - s_prevBytesReceived;
        }
        if (currentBytesSent >= s_prevBytesSent) {
            bytesDiffSent = currentBytesSent - s_prevBytesSent;
        }
        
        ULONGLONG totalBytesDiff = bytesDiffReceived + bytesDiffSent;
        
        // Calculate speed: bytes/sec -> Mbps
        // 1 Mbps = 1,000,000 bits/sec = 125,000 bytes/sec
        double bytesPerSecond = static_cast<double>(totalBytesDiff) / timeSeconds;
        double mbps = (bytesPerSecond * 8.0) / (1000.0 * 1000.0); // Convert to Mbps
        
        // Save current values for next call
        s_prevBytesReceived = currentBytesReceived;
        s_prevBytesSent = currentBytesSent;
        s_prevTime = currentTime;
        
        // Ensure value is within valid range (max 10Gbps)
        if (mbps < 0.0) mbps = 0.0;
        if (mbps > 10000.0) mbps = 10000.0;
        
        return mbps;
    }
    catch (...) {
        return -1.0;
    }
}

// Active App Monitoring Implementation
bool InitializeActiveAppMonitoring() {
    try {
        std::lock_guard<std::mutex> lock(g_historyMutex);
        
        // Initialize monitor if not already created
        if (!g_eventMonitor) {
            g_eventMonitor = std::make_unique<WindowEventMonitor>();
        }
        
        // Register callback for window events
        g_eventMonitor->RegisterCallback(OnWindowEvent);
        
        // Start monitoring
        bool success = g_eventMonitor->Start();
        if (success) {
            // Initialize with current active app and window title
            std::string currentApp = GetForegroundAppName();
            if (!currentApp.empty() && currentApp != "Unknown") {
                g_lastActiveApp = currentApp;
                g_lastActiveAppWindowTitle = currentApp; // Initialize window title
                g_lastAppStartTime = std::chrono::system_clock::now();
            }
        }
        
        return success;
    }
    catch (...) {
        return false;
    }
}

void CleanupActiveAppMonitoring() {
    try {
        std::lock_guard<std::mutex> lock(g_historyMutex);
        
        // Stop monitoring
        if (g_eventMonitor) {
            g_eventMonitor->Stop();
            g_eventMonitor.reset();
        }
        
        // Clear history and current app info
        g_activeAppHistory.clear();
        g_lastActiveApp.clear();
        g_lastActiveAppWindowTitle.clear(); // Clear window title too
    }
    catch (...) {
        // Ignore cleanup errors
    }
}

void OnWindowEvent(const WindowInfo& info) {
    try {
        std::string appName = GetAppNameFromWindowInfo(info);
        std::string windowTitle = WideStringToUtf8(info.windowTitle);
        
        // Skip empty, invalid, or system app names
        if (appName.empty() || appName == "Unknown" || appName == "Desktop" || appName == "csc_ui") {
            return;
        }
        
        std::lock_guard<std::mutex> lock(g_historyMutex);
        
        auto now = std::chrono::system_clock::now();
        
        // Check if this is a different app OR different window title (for tab switching)
        bool shouldRecord = false;
        if (!g_lastActiveApp.empty() && 
            (g_lastActiveApp != appName || g_lastActiveAppWindowTitle != windowTitle)) {
            shouldRecord = true;
        }
        
        // Record the previous app's duration if it's different
        if (shouldRecord) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastAppStartTime);
            
            // Only record if the app was active for more than 1 second
            if (duration.count() > 0) {
                int durationSecs = static_cast<int>(duration.count());
                
                // Generate unique key from appName + windowTitle
                std::string key = MakeAppKey(g_lastActiveApp, g_lastActiveAppWindowTitle);
                
                // Check if this exact combination already exists (O(1) lookup)
                auto it = g_activeAppHistory.find(key);
                if (it != g_activeAppHistory.end()) {
                    // Exact match (appName + windowTitle): accumulate duration
                    it->second.durationSeconds += durationSecs;
                    it->second.timestamp = g_lastAppStartTime;  // Update to latest session start time
                } else {
                    // New combination: create new record
                    ActiveAppRecord record;
                    record.appName = g_lastActiveApp;
                    record.windowTitle = g_lastActiveAppWindowTitle;
                    record.timestamp = g_lastAppStartTime;
                    record.durationSeconds = durationSecs;
                    
                    g_activeAppHistory[key] = record;
                }
            }
        }
        
        // Update current active app and window title
        g_lastActiveApp = appName;
        g_lastActiveAppWindowTitle = windowTitle;
        g_lastAppStartTime = now;
        
        // Clean up old records periodically
        static auto lastCleanup = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::minutes>(now - lastCleanup).count() >= 5) {
            CleanupOldRecords();
            lastCleanup = now;
        }
    }
    catch (...) {
        // Ignore event processing errors
    }
}

std::string GetAppNameFromWindowInfo(const WindowInfo& info) {
    try {
        // Try to get a clean app name
        std::string processName = WideStringToUtf8(info.processName);
        
        if (!processName.empty() && processName != "Unknown") {
            // Remove .exe extension if present
            size_t dotPos = processName.find_last_of('.');
            if (dotPos != std::string::npos) {
                processName = processName.substr(0, dotPos);
            }
            return processName;
        }
        
        // Fallback to window title if process name is not available
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

void CleanupOldRecords() {
    try {
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - HISTORY_RETENTION_PERIOD;
        
        // Remove records older than 1 hour (iterate through unordered_map)
        for (auto it = g_activeAppHistory.begin(); it != g_activeAppHistory.end(); ) {
            if (it->second.timestamp < cutoff) {
                it = g_activeAppHistory.erase(it);  // Erase returns iterator to next element
            } else {
                ++it;
            }
        }
    }
    catch (...) {
        // Ignore cleanup errors
    }
}

std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList() {
    try {
        std::lock_guard<std::mutex> lock(g_historyMutex);

        // Clean up old records first (older than 1 hour)
        CleanupOldRecords();

        // Convert unordered_map to vector for return
        std::vector<ActiveAppRecord> result;
        result.reserve(g_activeAppHistory.size() + 1);  // Reserve space for efficiency
        
        for (const auto& pair : g_activeAppHistory) {
            result.push_back(pair.second);
        }

        // Add current active app if it has been running for some time
        auto now = std::chrono::system_clock::now();
        if (!g_lastActiveApp.empty() && g_lastActiveApp != "Unknown" && g_lastActiveApp != "Desktop") {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastAppStartTime);
            if (duration.count() > 0) {
                int durationSecs = static_cast<int>(duration.count());
                
                // Generate key for current app
                std::string key = MakeAppKey(g_lastActiveApp, g_lastActiveAppWindowTitle);
                
                // Check if current app+windowTitle combination already exists in history
                auto it = g_activeAppHistory.find(key);
                if (it != g_activeAppHistory.end()) {
                    // Update existing record in result vector
                    for (auto& record : result) {
                        if (record.appName == g_lastActiveApp && 
                            record.windowTitle == g_lastActiveAppWindowTitle) {
                            record.durationSeconds += durationSecs;
                            record.timestamp = g_lastAppStartTime;
                            break;
                        }
                    }
                } else {
                    // Add new record for current app+windowTitle
                    ActiveAppRecord currentRecord;
                    currentRecord.appName = g_lastActiveApp;
                    currentRecord.windowTitle = g_lastActiveAppWindowTitle;
                    currentRecord.timestamp = g_lastAppStartTime;
                    currentRecord.durationSeconds = durationSecs;
                    result.push_back(currentRecord);
                }
            }
        }

        // NOTE: No limiting here - just return all records within the 1-hour window
        // Limiting to top 10 apps by totalSeconds is now done in ContextCollector::UpdateCache()

        return result;
    }
    catch (...) {
        return std::vector<ActiveAppRecord>();
    }
}

// Global location cache
static Location g_cachedLocation;
static std::chrono::steady_clock::time_point g_lastLocationUpdate;
static const std::chrono::minutes LOCATION_CACHE_DURATION{30}; // 30 minutes cache
static bool g_locationInitialized = false;

Location GetLocation() {
    using namespace winrt;
    using namespace Windows::Devices::Geolocation;
    using namespace Windows::Foundation;
    
    Location loc;
    loc.latitude = 0.0;
    loc.longitude = 0.0;
    loc.valid = false;
    
    try {
        auto now = std::chrono::steady_clock::now();
        
        // Return cached location if still valid
        if (g_locationInitialized && g_cachedLocation.valid && 
            (now - g_lastLocationUpdate) < LOCATION_CACHE_DURATION) {
            return g_cachedLocation;
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
            g_cachedLocation = loc;
            g_lastLocationUpdate = now;
            g_locationInitialized = true;
            return loc;
        }
        
        if (access != GeolocationAccessStatus::Allowed) {
            // Access denied, update cache and return invalid location
            g_cachedLocation = loc;
            g_lastLocationUpdate = now;
            g_locationInitialized = true;
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
            g_cachedLocation = loc;
            g_lastLocationUpdate = now;
            g_locationInitialized = true;
            
            return loc;
        }
        catch (winrt::hresult_error const& e) {
            // Location request failed, cache invalid result for short time
            g_cachedLocation = loc;
            g_lastLocationUpdate = now;
            g_locationInitialized = true;
            return loc;
        }
    }
    catch (...) {
        // Any other error, return invalid location
        auto now = std::chrono::steady_clock::now();
        g_cachedLocation = loc;
        g_lastLocationUpdate = now;
        g_locationInitialized = true;
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
        
        // Use localtime_s instead of gmtime_s for local time
        struct tm timeinfo;
        if (localtime_s(&timeinfo, &time_t_val) == 0) {
            oss << std::put_time(&timeinfo, "%Y-%m-%dT%H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            
            // Add timezone offset
            char tz_offset[16];
            strftime(tz_offset, sizeof(tz_offset), "%z", &timeinfo);
            std::string tz_str(tz_offset);
            if (tz_str.length() >= 5) {
                // Convert +0800 to +08:00 format
                tz_str = tz_str.substr(0, 3) + ":" + tz_str.substr(3);
            } else {
                tz_str = "+00:00"; // fallback
            }
            oss << tz_str;
        } else {
            return "1970-01-01T00:00:00.000+00:00";
        }
        
        return oss.str();
    }
    catch (...) {
        return "1970-01-01T00:00:00.000+00:00";
    }
}

// NEW: Get current active app content using UIA technology
std::string GetCurrentActiveAppContent() {
    try {
        // Get the foreground window handle
        HWND hwnd = GetForegroundWindow();
        if (!hwnd) {
            return "";
        }
        
        // Create BrowserContentExtractor instance
        BrowserContentExtractor extractor;
        
        // Get the content using UIA
        BrowserContentInfo info;
        bool success = extractor.GetBrowserContentByHWND(hwnd, info);
        
        if (!success || info.textContent.empty()) {
            return "";
        }
        
        // Convert wstring to UTF-8 string
        std::string contentUtf8 = WideStringToUtf8(info.textContent);
        
        return contentUtf8;
    }
    catch (...) {
        return "";
    }
}

} // namespace WindowsAPIs