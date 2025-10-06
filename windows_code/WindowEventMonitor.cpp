#include "WindowEventMonitor.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")

// 静态成员初始化
WindowEventMonitor* WindowEventMonitor::s_instance = nullptr;

WindowEventMonitor::WindowEventMonitor() 
    : m_hook(nullptr), m_shellHook(nullptr), m_isRunning(false), m_messageWindow(nullptr) {
    s_instance = this;
}

WindowEventMonitor::~WindowEventMonitor() {
    Stop();
    s_instance = nullptr;
}

bool WindowEventMonitor::Start() {
    if (m_isRunning) {
        m_lastError = L"Monitor is already running";
        return false;
    }

    m_isRunning = true;

    // 启动消息循环线程
    m_messageThread = std::thread(&WindowEventMonitor::MessageLoopThread, this);
    
    // 等待线程初始化完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

void WindowEventMonitor::Stop() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    // 发送退出消息到消息窗口
    if (m_messageWindow) {
        PostMessage(m_messageWindow, WM_QUIT, 0, 0);
    }

    // 等待消息循环线程结束
    if (m_messageThread.joinable()) {
        m_messageThread.join();
    }

    // 清理Hook
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }

    if (m_shellHook) {
        UnhookWindowsHookEx(m_shellHook);
        m_shellHook = nullptr;
    }
}

void WindowEventMonitor::MessageLoopThread() {
    // 创建一个隐藏的消息窗口
    const wchar_t* className = L"WindowEventMonitorClass";
    WNDCLASSEXW wc = { 0 }; // 使用Unicode版本
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DefWindowProcW; // Unicode版本
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;

    if (!RegisterClassExW(&wc)) { // Unicode版本
        DWORD error = ::GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            m_lastError = L"Failed to register window class";
            return;
        }
    }

    m_messageWindow = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, // Unicode版本
                                     HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!m_messageWindow) {
        m_lastError = L"Failed to create message window";
        UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode版本
        return;
    }

    // 设置Windows事件Hook - 监控窗口激活和名称变化事件（用于检测Chrome标签页切换）
    m_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,      // 最小事件
        EVENT_OBJECT_NAMECHANGE,      // 最大事件（包含窗口标题变化）
        nullptr,                       // DLL句柄（nullptr表示在调用进程中）
        WinEventProc,                  // 回调函数
        0,                            // 进程ID（0表示所有进程）
        0,                            // 线程ID（0表示所有线程）
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS // 标志，跳过自身进程
    );

    if (!m_hook) {
        m_lastError = L"Failed to set Windows event hook";
        DestroyWindow(m_messageWindow);
        UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode版本
        return;
    }

    // 消息循环
    MSG msg;
    while (m_isRunning && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理窗口
    DestroyWindow(m_messageWindow);
    UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode版本
    m_messageWindow = nullptr;
}

void CALLBACK WindowEventMonitor::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                                               HWND hwnd, LONG idObject, LONG idChild,
                                               DWORD eventThread, DWORD eventTime) {
    if (!s_instance || !s_instance->m_isRunning) {
        return;
    }

    // 过滤非窗口对象事件
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }

    // 过滤无效窗口
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    WindowInfo info = GetWindowInfo(hwnd);
    
    // 根据事件类型设置事件类型
    switch (event) {
        case EVENT_SYSTEM_FOREGROUND:
        case EVENT_OBJECT_FOCUS:
            info.eventType = WindowEventType::WINDOW_ACTIVATED;
            break;
        case EVENT_OBJECT_NAMECHANGE:
            // 检查是否是Chrome/Edge等浏览器窗口的标题变化（可能是标签页切换）
            if (s_instance->IsChromeWindow(hwnd)) {
                std::wstring tabTitle;
                if (s_instance->TryGetChromeTabInfo(hwnd, tabTitle)) {
                    info.tabTitle = tabTitle;
                    info.eventType = WindowEventType::TAB_ACTIVATED;
                }
            } else {
                // 其他程序的窗口标题变化也触发事件
                info.eventType = WindowEventType::WINDOW_ACTIVATED;
            }
            break;
        default:
            return;  // 忽略其他事件
    }

    // 触发回调
    s_instance->TriggerCallbacks(info);
}

WindowInfo WindowEventMonitor::GetWindowInfo(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    info.timestamp = std::chrono::system_clock::now();

    // 获取窗口标题 - 使用Unicode版本
    wchar_t windowTitle[256] = { 0 };
    GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));
    info.windowTitle = windowTitle;

    // 获取窗口类名 - 使用Unicode版本
    wchar_t className[256] = { 0 };
    GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t));
    info.className = className;

    // 获取进程和线程ID
    info.threadId = GetWindowThreadProcessId(hwnd, &info.processId);

    // 获取进程名称和路径
    if (info.processId != 0) {
        info.processName = GetProcessName(info.processId);
        info.processPath = GetProcessPath(info.processId);
    }

    return info;
}

std::wstring WindowEventMonitor::GetProcessName(DWORD processId) {
    std::wstring processName = L"Unknown";
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        wchar_t buffer[MAX_PATH]; // Unicode缓冲区
        if (GetModuleBaseNameW(hProcess, nullptr, buffer, MAX_PATH)) { // Unicode版本
            processName = buffer;
        }
        CloseHandle(hProcess);
    }
    
    return processName;
}

std::wstring WindowEventMonitor::GetProcessPath(DWORD processId) {
    std::wstring processPath = L"Unknown";
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        wchar_t buffer[MAX_PATH]; // Unicode缓冲区
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) { // Unicode版本
            processPath = buffer;
        }
        CloseHandle(hProcess);
    }
    
    return processPath;
}

void WindowEventMonitor::RegisterCallback(EventCallback callback) {
    m_callbacks.push_back(callback);
}

void WindowEventMonitor::ClearCallbacks() {
    m_callbacks.clear();
}

void WindowEventMonitor::TriggerCallbacks(const WindowInfo& info) {
    for (const auto& callback : m_callbacks) {
        if (callback) {
            callback(info);
        }
    }
}

WindowInfo WindowEventMonitor::GetActiveWindowInfo() {
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
        WindowInfo info = GetWindowInfo(hwnd);
        info.eventType = WindowEventType::WINDOW_ACTIVATED;
        return info;
    }
    return WindowInfo();
}

std::vector<WindowInfo> WindowEventMonitor::GetAllWindows() {
    std::vector<WindowInfo> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

BOOL CALLBACK WindowEventMonitor::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) {
        return TRUE;  // 跳过不可见窗口
    }

    // 获取窗口标题 - 使用Unicode版本
    wchar_t windowTitle[256];
    GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));
    
    // 跳过没有标题的窗口
    if (wcslen(windowTitle) == 0) {
        return TRUE;
    }

    auto* windows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    WindowInfo info = GetWindowInfo(hwnd);
    windows->push_back(info);

    return TRUE;
}

// Chrome窗口检测
bool WindowEventMonitor::IsChromeWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    
    // 获取进程ID
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return false;

    // 获取进程名称
    std::wstring processName = GetProcessName(pid);
    
    // 转换为小写进行比较
    for (auto &ch : processName) {
        ch = towlower(ch);
    }
    
    // 检查是否是Chrome、Edge等浏览器
    return (processName.find(L"chrome.exe") != std::wstring::npos) ||
           (processName.find(L"msedge.exe") != std::wstring::npos) ||
           (processName.find(L"firefox.exe") != std::wstring::npos) ||
           (processName.find(L"opera.exe") != std::wstring::npos);
}

// 尝试获取Chrome标签页信息
bool WindowEventMonitor::TryGetChromeTabInfo(HWND hwnd, std::wstring& tabTitle) {
    // Chrome会在窗口标题中显示当前活动标签页的标题
    wchar_t title[1024] = {0};
    if (GetWindowTextW(hwnd, title, 1023) > 0) { // 使用Unicode版本
        tabTitle = title;
        return true;
    }
    return false;
}

LRESULT CALLBACK WindowEventMonitor::WindowEventProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Shell Hook处理（如果需要的话）
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
