#include "WindowsAPIs.h"
#include "WindowEventMonitor.h"
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
#include <mutex>
#include <memory>

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
static std::vector<ActiveAppRecord> g_activeAppHistory;
static std::mutex g_historyMutex;
static std::string g_lastActiveApp;
static std::string g_lastActiveAppWindowTitle;
static std::chrono::system_clock::time_point g_lastAppStartTime;
static const std::chrono::hours HISTORY_RETENTION_PERIOD{1}; // 1 hour retention

// Event callback function for window monitoring
void OnWindowEvent(const WindowInfo& info);

// Cleanup old records (older than 1 hour)
void CleanupOldRecords();

// Convert WindowInfo to app name
std::string GetAppNameFromWindowInfo(const WindowInfo& info);

std::string GetForegroundAppName() {
    try {
        // 方法1: 尝试获取前台窗口标题 (增强版 - 支持Unicode)
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            // 使用Unicode版本的API来正确处理中文字符
            wchar_t buffer[512] = {0};  // Unicode缓冲区
            int result = GetWindowTextW(hwnd, buffer, sizeof(buffer)/sizeof(wchar_t) - 1);
            
            if (result > 0 && wcslen(buffer) > 0) {
                // 将Unicode转换为UTF-8
                std::string title = WideStringToUtf8(buffer);
                
                // 过滤掉一些无意义的标题
                if (title != "Program Manager" && 
                    title != "Desktop" && 
                    title != "" &&
                    title.find("Windows Default Lock Screen") == std::string::npos) {
                    return title;
                }
            }
        }
        
        // 方法2: 进程信息获取 (增强版)
        DWORD processId = 0;
        if (hwnd) {
            GetWindowThreadProcessId(hwnd, &processId);
        }
        
        // 方法2.1: 尝试GetFocus作为后备
        if (processId == 0) {
            HWND focusWindow = GetFocus();
            if (focusWindow) {
                GetWindowThreadProcessId(focusWindow, &processId);
            }
        }
        
        // 方法2.2: 尝试GetActiveWindow
        if (processId == 0) {
            HWND activeWindow = GetActiveWindow();
            if (activeWindow) {
                GetWindowThreadProcessId(activeWindow, &processId);
            }
        }
        
        if (processId > 0) {
            // 增强的进程信息获取
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (hProcess) {
                wchar_t processName[MAX_PATH] = {0};
                DWORD size = sizeof(processName)/sizeof(wchar_t);
                
                // 优先使用QueryFullProcessImageNameW (Unicode版本)
                if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
                    CloseHandle(hProcess);
                    
                    std::wstring fullPath(processName);
                    size_t lastSlash = fullPath.find_last_of(L"\\/");
                    if (lastSlash != std::wstring::npos) {
                        std::wstring exeName = fullPath.substr(lastSlash + 1);
                        
                        // 去掉.exe扩展名并格式化
                        size_t dotPos = exeName.find_last_of(L'.');
                        if (dotPos != std::wstring::npos) {
                            exeName = exeName.substr(0, dotPos);
                        }
                        
                        std::string exeNameUtf8 = WideStringToUtf8(exeName);
                        
                        // 排除系统进程
                        if (exeNameUtf8 != "dwm" && 
                            exeNameUtf8 != "winlogon" && 
                            exeNameUtf8 != "csrss" &&
                            exeNameUtf8 != "explorer" &&
                            !exeNameUtf8.empty()) {
                            return exeNameUtf8;
                        }
                    }
                } else {
                    // 回退到GetModuleBaseNameW
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
        
        // 方法3: 增强的窗口枚举 (更智能的过滤 + Unicode支持)
        struct WindowInfo {
            HWND bestWindow = NULL;
            std::string title;
            DWORD processId = 0;
            int score = 0;  // 窗口质量评分
        };
        
        WindowInfo info;
        
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            WindowInfo* pInfo = reinterpret_cast<WindowInfo*>(lParam);
            
            // 更严格的窗口检查
            if (IsWindowVisible(hwnd) && 
                !IsIconic(hwnd) &&  // 不是最小化
                !(GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
                
                wchar_t title[512] = {0};
                int titleLen = GetWindowTextW(hwnd, title, sizeof(title)/sizeof(wchar_t) - 1);
                
                if (titleLen > 0) {
                    std::string titleStr = WideStringToUtf8(title);
                    
                    // 计算窗口质量评分
                    int score = 0;
                    
                    // 排除系统窗口
                    if (titleStr == "Program Manager" || 
                        titleStr == "Desktop" ||
                        titleStr.find("Windows Default Lock Screen") != std::string::npos) {
                        return TRUE; // 跳过
                    }
                    
                    // 加分项
                    if (titleStr.find(" - ") != std::string::npos) score += 10;  // 有应用名
                    if (titleLen > 10) score += 5;  // 标题较长
                    
                    // 获取窗口大小，大窗口加分
                    RECT rect;
                    if (GetWindowRect(hwnd, &rect)) {
                        int width = rect.right - rect.left;
                        int height = rect.bottom - rect.top;
                        if (width > 200 && height > 200) score += 5;
                    }
                    
                    // 如果这个窗口评分更高，选择它
                    if (score > pInfo->score) {
                        pInfo->bestWindow = hwnd;
                        pInfo->title = titleStr;
                        pInfo->score = score;
                        GetWindowThreadProcessId(hwnd, &pInfo->processId);
                    }
                }
            }
            return TRUE; // 继续枚举
        }, reinterpret_cast<LPARAM>(&info));
        
        if (!info.title.empty() && info.score > 0) {
            return info.title;
        }
        
        // 方法4: 最后的进程枚举后备 (新增)
        // 如果窗口方法都失败，尝试找最活跃的进程
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
                                
                                // 寻找常见的用户应用程序
                                if (name.find(L"notepad") != std::wstring::npos ||
                                    name.find(L"calc") != std::wstring::npos ||
                                    name.find(L"chrome") != std::wstring::npos ||
                                    name.find(L"firefox") != std::wstring::npos ||
                                    name.find(L"code") != std::wstring::npos ||
                                    name.find(L"devenv") != std::wstring::npos) {
                                    
                                    CloseHandle(hProcess);
                                    
                                    // 去掉.exe扩展名
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
            // 忽略进程枚举错误
        }
        
        // 方法5: 智能默认值 (改进)
        // 检查是否真的是桌面状态
        HWND desktopWindow = GetDesktopWindow();
        HWND shellWindow = GetShellWindow();
        
        if (hwnd == desktopWindow || hwnd == shellWindow || hwnd == NULL) {
            // 真的是桌面状态，但提供更有用的信息
            return "Desktop";
        }
        
        // 如果所有方法都失败，返回增强的未知状态
        return "Unknown";
    }
    catch (...) {
        return "Unknown";
    }
}

// 辅助函数：将Unicode字符串转换为UTF-8
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

// 重载版本：从wchar_t*转换
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
        // 使用系统整体CPU使用率计算
        static FILETIME s_ftPrevSysIdle = {0};
        static FILETIME s_ftPrevSysKernel = {0};
        static FILETIME s_ftPrevSysUser = {0};
        static bool s_firstCall = true;
        
        FILETIME ftSysIdle, ftSysKernel, ftSysUser;
        
        // 获取系统时间
        if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
            return -1.0;
        }
        
        // 第一次调用，保存当前值并返回0
        if (s_firstCall) {
            s_ftPrevSysIdle = ftSysIdle;
            s_ftPrevSysKernel = ftSysKernel;
            s_ftPrevSysUser = ftSysUser;
            s_firstCall = false;
            return 0.0;
        }
        
        // 转换为64位值
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
        
        // 计算时间差
        ULONGLONG idleDiff = sysIdle.QuadPart - prevSysIdle.QuadPart;
        ULONGLONG kernelDiff = sysKernel.QuadPart - prevSysKernel.QuadPart;
        ULONGLONG userDiff = sysUser.QuadPart - prevSysUser.QuadPart;
        
        // 注意：kernelDiff包含了idle time，所以需要减去
        ULONGLONG systemDiff = kernelDiff + userDiff;
        ULONGLONG totalDiff = systemDiff;
        
        double cpuUsage = 0.0;
        if (totalDiff > 0) {
            // CPU使用率 = (总时间 - 空闲时间) / 总时间 * 100
            cpuUsage = (double)(totalDiff - idleDiff) * 100.0 / (double)totalDiff;
        }
        
        // 保存当前值供下次使用
        s_ftPrevSysIdle = ftSysIdle;
        s_ftPrevSysKernel = ftSysKernel;
        s_ftPrevSysUser = ftSysUser;
        
        // 确保返回值在合理范围内
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
            // 返回内存使用百分比
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
            // 计算已用内存并转换为GB
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
            // 将字节转换为GB
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
        // 使用传统的GetIfTable API (更好的兼容性)
        static ULONGLONG s_prevBytesReceived = 0;
        static ULONGLONG s_prevBytesSent = 0;
        static std::chrono::steady_clock::time_point s_prevTime;
        static bool s_firstCall = true;
        
        ULONGLONG currentBytesReceived = 0;
        ULONGLONG currentBytesSent = 0;
        auto currentTime = std::chrono::steady_clock::now();
        
        // 使用GetIfTable获取网络接口信息 (兼容性更好)
        PMIB_IFTABLE pIfTable = nullptr;
        DWORD dwSize = 0;
        
        // 获取所需缓冲区大小
        DWORD dwRetVal = GetIfTable(pIfTable, &dwSize, 0);
        if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
            pIfTable = (MIB_IFTABLE*)malloc(dwSize);
            if (pIfTable == nullptr) {
                return -1.0;
            }
        } else {
            return -1.0;
        }
        
        // 获取接口表
        dwRetVal = GetIfTable(pIfTable, &dwSize, 0);
        if (dwRetVal != NO_ERROR) {
            free(pIfTable);
            return -1.0;
        }
        
        // 遍历所有网络接口，累计活动接口的流量
        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++) {
            MIB_IFROW* pIfRow = &pIfTable->table[i];
            
            // 只统计活动的物理网络接口 (跳过环回接口等)
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
        
        // 第一次调用，保存当前值并返回0
        if (s_firstCall) {
            s_prevBytesReceived = currentBytesReceived;
            s_prevBytesSent = currentBytesSent;
            s_prevTime = currentTime;
            s_firstCall = false;
            return 0.0;
        }
        
        // 计算时间间隔
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - s_prevTime);
        double timeSeconds = timeDiff.count() / 1000.0;
        
        if (timeSeconds <= 0.0) {
            return -1.0; // 避免除零
        }
        
        // 处理计数器回绕的情况
        ULONGLONG bytesDiffReceived = 0;
        ULONGLONG bytesDiffSent = 0;
        
        if (currentBytesReceived >= s_prevBytesReceived) {
            bytesDiffReceived = currentBytesReceived - s_prevBytesReceived;
        }
        if (currentBytesSent >= s_prevBytesSent) {
            bytesDiffSent = currentBytesSent - s_prevBytesSent;
        }
        
        ULONGLONG totalBytesDiff = bytesDiffReceived + bytesDiffSent;
        
        // 计算速度: 字节/秒 -> Mbps
        // 1 Mbps = 1,000,000 bits/sec = 125,000 bytes/sec
        double bytesPerSecond = static_cast<double>(totalBytesDiff) / timeSeconds;
        double mbps = (bytesPerSecond * 8.0) / (1000.0 * 1000.0); // 转换为Mbps
        
        // 保存当前值供下次使用
        s_prevBytesReceived = currentBytesReceived;
        s_prevBytesSent = currentBytesSent;
        s_prevTime = currentTime;
        
        // 确保返回值合理 (限制最大值为10Gbps)
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
        
        // Skip empty or invalid app names
        if (appName.empty() || appName == "Unknown" || appName == "Desktop") {
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
                ActiveAppRecord record;
                record.appName = g_lastActiveApp;
                record.timestamp = g_lastAppStartTime;
                record.durationSeconds = static_cast<int>(duration.count());
                
                // Use the stored window title from when this session started
                record.windowTitle = g_lastActiveAppWindowTitle;
                
                g_activeAppHistory.push_back(record);
            }
        }
        
        // Update current active app and window title
        g_lastActiveApp = appName;
        g_lastActiveAppWindowTitle = windowTitle; // Store current window title
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
        
        // Remove records older than 1 hour
        g_activeAppHistory.erase(
            std::remove_if(g_activeAppHistory.begin(), g_activeAppHistory.end(),
                [cutoff](const ActiveAppRecord& record) {
                    return record.timestamp < cutoff;
                }),
            g_activeAppHistory.end()
        );
    }
    catch (...) {
        // Ignore cleanup errors
    }
}

std::vector<ActiveAppRecord> GetRecentPeriodActiveAppList() {
    try {
        std::lock_guard<std::mutex> lock(g_historyMutex);
        
        // Clean up old records first
        CleanupOldRecords();
        
        // Add current active app if it has been running for some time
        auto now = std::chrono::system_clock::now();
        if (!g_lastActiveApp.empty() && g_lastActiveApp != "Unknown" && g_lastActiveApp != "Desktop") {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - g_lastAppStartTime);
            if (duration.count() > 0) {
                ActiveAppRecord currentRecord;
                currentRecord.appName = g_lastActiveApp;
                currentRecord.timestamp = g_lastAppStartTime;
                currentRecord.durationSeconds = static_cast<int>(duration.count());
                currentRecord.windowTitle = g_lastActiveAppWindowTitle; // Use stored window title
                
                // Add to temporary result
                std::vector<ActiveAppRecord> result = g_activeAppHistory;
                result.push_back(currentRecord);
                return result;
            }
        }
        
        return g_activeAppHistory;
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

} // namespace WindowsAPIs