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
#include <comdef.h>
#include <atlbase.h>

// 窗口事件类型
enum class WindowEventType {
    WINDOW_ACTIVATED,      // 窗口被激活/获得焦点
    WINDOW_CREATED,        // 新窗口被创建
    WINDOW_DESTROYED,      // 窗口被销毁
    APPLICATION_STARTED,   // 应用程序启动
    APPLICATION_ENDED,     // 应用程序结束
    WINDOW_MINIMIZED,      // 窗口最小化
    WINDOW_RESTORED,       // 窗口恢复
    WINDOW_MAXIMIZED,      // 窗口最大化
    TAB_ACTIVATED,         // 浏览器标签页切换
    TAB_CREATED,           // 标签页创建
    TAB_CLOSED             // 标签页关闭
};

// 窗口信息结构
struct WindowInfo {
    HWND hwnd;                    // 窗口句柄
    DWORD processId;               // 进程ID
    DWORD threadId;                // 线程ID
    std::wstring windowTitle;      // 窗口标题
    std::wstring className;        // 窗口类名
    std::wstring processName;      // 进程名称
    std::wstring processPath;      // 进程完整路径
    WindowEventType eventType;     // 事件类型
    std::chrono::system_clock::time_point timestamp; // 事件时间戳

    // 对于Chrome等浏览器的标签页信息
    std::wstring tabTitle;            // 标签页标题（若可用）
    std::wstring tabUrl;              // 标签页URL（如果可通过UIA/辅助功能获得）
    
    WindowInfo() : hwnd(nullptr), processId(0), threadId(0), 
                   eventType(WindowEventType::WINDOW_ACTIVATED),
                   timestamp(std::chrono::system_clock::now()) {}
};

// 事件回调函数类型
using EventCallback = std::function<void(const WindowInfo&)>;

// Windows事件监控器类
class WindowEventMonitor {
public:
    WindowEventMonitor();
    ~WindowEventMonitor();

    // 启动监控
    bool Start();
    
    // 停止监控
    void Stop();
    
    // 注册事件回调
    void RegisterCallback(EventCallback callback);
    
    // 清除所有回调
    void ClearCallbacks();
    
    // 获取当前活动窗口信息
    WindowInfo GetActiveWindowInfo();
    
    // 获取所有顶级窗口列表
    std::vector<WindowInfo> GetAllWindows();
    
    // 检查监控器是否正在运行
    bool IsRunning() const { return m_isRunning; }
    
    // 获取错误信息
    std::wstring GetLastError() const { return m_lastError; }

private:
    // Hook回调函数（静态）
    static LRESULT CALLBACK WindowEventProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                      HWND hwnd, LONG idObject, LONG idChild,
                                      DWORD eventThread, DWORD eventTime);

    // UI Automation 事件处理（用于Chrome标签页等控件）
    void InitUIAutomation();
    void CleanupUIAutomation();
    void SubscribeUIAEvents(HWND topLevelHwnd);
    void UnsubscribeUIAEvents();
    static void CALLBACK WinEventObjectNameChange(HWINEVENTHOOK hWinEventHook, DWORD event,
                                                  HWND hwnd, LONG idObject, LONG idChild,
                                                  DWORD eventThread, DWORD eventTime);
    void HandleObjectNameChange(HWND hwnd);
    bool IsChromeWindow(HWND hwnd);

    // 尝试通过UI Automation获取标签页标题
    bool TryGetChromeTabInfo(HWND hwnd, std::wstring& tabTitle);
    
    // 辅助: 获取子窗口的可访问对象名
    static std::wstring GetAccNameFromObject(HWND hwnd);

    // UIA对象
    CComPtr<IUIAutomation> m_uia;
    
    // 获取窗口详细信息
    static WindowInfo GetWindowInfo(HWND hwnd);
    
    // 获取进程名称
    static std::wstring GetProcessName(DWORD processId);
    
    // 获取进程完整路径
    static std::wstring GetProcessPath(DWORD processId);
    
    // 触发事件回调
    void TriggerCallbacks(const WindowInfo& info);
    
    // 消息循环线程
    void MessageLoopThread();
    
    // 枚举所有窗口的回调函数
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

private:
    static WindowEventMonitor* s_instance;  // 单例实例
    HWINEVENTHOOK m_hook;                   // Windows事件Hook句柄
    HHOOK m_shellHook;                      // Shell Hook句柄
    std::vector<EventCallback> m_callbacks; // 事件回调列表
    std::atomic<bool> m_isRunning;          // 运行状态标志
    std::thread m_messageThread;            // 消息循环线程
    std::wstring m_lastError;               // 最后的错误信息
    HWND m_messageWindow;                   // 消息窗口句柄
};

// 辅助函数：将事件类型转换为字符串
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

// 辅助函数：格式化时间戳
inline std::wstring FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t);
    
    wchar_t buffer[100];
    wcsftime(buffer, sizeof(buffer)/sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &timeinfo);
    
    return std::wstring(buffer);
}

#endif // WINDOW_EVENT_MONITOR_H