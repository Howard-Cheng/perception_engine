#ifndef WINDOW_EVENT_MONITOR_H
#define WINDOW_EVENT_MONITOR_H

#include <Windows.h>
#include <Psapi.h>
#include <oleacc.h>
#include <UIAutomation.h>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <map>
#include <set>
#include <mutex>
#include <comdef.h>
#include <atlbase.h>

// Window event types
enum class WindowEventType {
    WINDOW_ACTIVATED,      // Window activated/gained focus
    WINDOW_CREATED,        // New window created
    WINDOW_DESTROYED,      // Window destroyed
    APPLICATION_STARTED,   // Application started
    APPLICATION_ENDED,     // Application ended
    WINDOW_MINIMIZED,      // Window minimized
    WINDOW_RESTORED,       // Window restored
    WINDOW_MAXIMIZED,      // Window maximized
    TAB_ACTIVATED,         // Browser tab switched
    TAB_CREATED,           // Tab created
    TAB_CLOSED             // Tab closed
};

// Window information structure
struct WindowInfo {
    HWND hwnd;                    // Window handle
    DWORD processId;               // Process ID
    DWORD threadId;                // Thread ID
    std::wstring windowTitle;      // Window title
    std::wstring className;        // Window class name
    std::wstring processName;      // Process name
    std::wstring processPath;      // Full process path
    WindowEventType eventType;     // Event type
    std::chrono::system_clock::time_point timestamp; // Event timestamp

    // For Chrome and other browsers' tab information
    std::wstring tabTitle;            // Tab title (if available)
    std::wstring tabUrl;              // Tab URL (if available via UIA/Accessibility)
    
    WindowInfo() : hwnd(nullptr), processId(0), threadId(0), 
                   eventType(WindowEventType::WINDOW_ACTIVATED),
                   timestamp(std::chrono::system_clock::now()) {}
};

// Event callback function type
using EventCallback = std::function<void(const WindowInfo&)>;

// Windows event monitor class
class WindowEventMonitor {
public:
    WindowEventMonitor();
    ~WindowEventMonitor();

    // Start monitoring
    bool Start();
    
    // Stop monitoring
    void Stop();
    
    // Register event callback
    void RegisterCallback(EventCallback callback);
    
    // Clear all callbacks
    void ClearCallbacks();
    
    // Get current active window information
    WindowInfo GetActiveWindowInfo();
    
    // Get all top-level windows list
    std::vector<WindowInfo> GetAllWindows();
    
    // Check if monitor is running
    bool IsRunning() const { return m_isRunning; }
    
    // Get error information
    std::wstring GetLastError() const { return m_lastError; }

private:
    // Hook callback functions (static)
    static LRESULT CALLBACK WindowEventProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                      HWND hwnd, LONG idObject, LONG idChild,
                                      DWORD eventThread, DWORD eventTime);

    // UI Automation event handling (for Chrome tabs and other controls)
    void InitUIAutomation();
    void CleanupUIAutomation();
    void SubscribeUIAEvents(HWND topLevelHwnd);
    void UnsubscribeUIAEvents();
    static void CALLBACK WinEventObjectNameChange(HWINEVENTHOOK hWinEventHook, DWORD event,
                                                  HWND hwnd, LONG idObject, LONG idChild,
                                                  DWORD eventThread, DWORD eventTime);
    void HandleObjectNameChange(HWND hwnd);
    bool IsChromeWindow(HWND hwnd);

    // Try to get tab info via UI Automation
    bool TryGetChromeTabInfo(HWND hwnd, std::wstring& tabTitle);
    
    // Helper: Get accessible object name from child window
    static std::wstring GetAccNameFromObject(HWND hwnd);

    // UIA objects
    CComPtr<IUIAutomation> m_uia;
    
    // Get detailed window information
    static WindowInfo GetWindowInfo(HWND hwnd);
    
    // Get process name
    static std::wstring GetProcessName(DWORD processId);
    
    // Get full process path
    static std::wstring GetProcessPath(DWORD processId);
    
    // Trigger event callbacks
    void TriggerCallbacks(const WindowInfo& info);
    
    // Message loop thread
    void MessageLoopThread();
    
    // Enumerate all windows callback function
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

private:
    static WindowEventMonitor* s_instance;  // Singleton instance
    HWINEVENTHOOK m_hook;                   // Windows event Hook handle
    HHOOK m_shellHook;                      // Shell Hook handle
    std::vector<EventCallback> m_callbacks; // Event callback list
    std::atomic<bool> m_isRunning;          // Running status flag
    std::thread m_messageThread;            // Message loop thread
    std::wstring m_lastError;               // Last error information
    HWND m_messageWindow;                   // Message window handle
    
    // Last event tracking (simplified - no time-based debounce)
    struct LastEvent {
        HWND hwnd;
        std::wstring windowTitle;
    };
    
    LastEvent m_lastEvent;
    std::mutex m_lastEventMutex;
    
    // Helper: Check if event should be triggered (skip if same as last)
    bool ShouldTriggerEvent(HWND hwnd, WindowEventType eventType, const std::wstring& title);
};

// Helper function: Convert event type to string
inline std::wstring EventTypeToString(WindowEventType type) {
    switch (type) {
        case WindowEventType::WINDOW_ACTIVATED: return L"Window Activated";
        case WindowEventType::WINDOW_CREATED: return L"Window Created";
        case WindowEventType::WINDOW_DESTROYED: return L"Window Destroyed";
        case WindowEventType::APPLICATION_STARTED: return L"Application Started";
        case WindowEventType::APPLICATION_ENDED: return L"Application Ended";
        case WindowEventType::WINDOW_MINIMIZED: return L"Window Minimized";
        case WindowEventType::WINDOW_RESTORED: return L"Window Restored";
        case WindowEventType::WINDOW_MAXIMIZED: return L"Window Maximized";
        case WindowEventType::TAB_ACTIVATED: return L"Tab Activated";
        case WindowEventType::TAB_CREATED: return L"Tab Created";
        case WindowEventType::TAB_CLOSED: return L"Tab Closed";
        default: return L"Unknown Event";
    }
}

// Helper function: Format timestamp
inline std::wstring FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    
    wchar_t buffer[100];
    wcsftime(buffer, sizeof(buffer)/sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &timeinfo);
    
    return std::wstring(buffer);
}

#endif // WINDOW_EVENT_MONITOR_H