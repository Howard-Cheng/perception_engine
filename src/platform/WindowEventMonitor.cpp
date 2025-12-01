#include "platform/WindowEventMonitor.h"
#include "pe_base/windows_helper.h"
#include <pe_base/logger.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <mutex>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")

// Static member initialization
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

    // Start message loop thread
    m_messageThread = std::thread(&WindowEventMonitor::MessageLoopThread, this);

    // Wait for thread initialization to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

void WindowEventMonitor::Stop() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;

    // Send quit message to message window
    if (m_messageWindow) {
        PostMessage(m_messageWindow, WM_QUIT, 0, 0);
    }

    // Wait for message loop thread to exit
    if (m_messageThread.joinable()) {
        m_messageThread.join();
    }

    // Cleanup hooks
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
    // Create a hidden message window
    const wchar_t* className = L"WindowEventMonitorClass";
    WNDCLASSEXW wc = { 0 }; // Use Unicode version
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DefWindowProcW; // Unicode version
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;

    if (!RegisterClassExW(&wc)) { // Unicode version
        DWORD error = ::GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            m_lastError = L"Failed to register window class";
            return;
        }
    }

    m_messageWindow = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, // Unicode version
        HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!m_messageWindow) {
        m_lastError = L"Failed to create message window";
        UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode version
        return;
    }

    // Set Windows event hook - Monitor window activation and name change events (for detecting Chrome tab switches)
    m_hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,      // Minimum event
        EVENT_OBJECT_NAMECHANGE,      // Maximum event (includes window title changes)
        nullptr,                       // DLL handle (nullptr means in calling process)
        WinEventProc,                  // Callback function
        0,                            // Process ID (0 means all processes)
        0,                            // Thread ID (0 means all threads)
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS // Flags, skip own process
    );

    if (!m_hook) {
        m_lastError = L"Failed to set Windows event hook";
        DestroyWindow(m_messageWindow);
        UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode version
        return;
    }

    // Message loop
    MSG msg;
    while (m_isRunning && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup window
    DestroyWindow(m_messageWindow);
    UnregisterClassW(className, GetModuleHandle(nullptr)); // Unicode version
    m_messageWindow = nullptr;
}

void CALLBACK WindowEventMonitor::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
    HWND hwnd, LONG idObject, LONG idChild,
    DWORD eventThread, DWORD eventTime) {
    if (!s_instance || !s_instance->m_isRunning) {
        return;
    }

    // Filter out non-window object events
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }

    // Filter out invalid windows
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    WindowInfo info = GetWindowInfo(hwnd);

    // Set event type based on event type
    switch (event) {
    case EVENT_SYSTEM_FOREGROUND:
    case EVENT_OBJECT_FOCUS:
        info.eventType = WindowEventType::WINDOW_ACTIVATED;
        break;
    case EVENT_OBJECT_NAMECHANGE:
        // Check if it's a Chrome/Edge browser window title change (may indicate tab switch)
        if (s_instance->IsChromeWindow(hwnd)) {
            std::wstring tabTitle;
            if (s_instance->TryGetChromeTabInfo(hwnd, tabTitle)) {
                info.tabTitle = tabTitle;
                info.eventType = WindowEventType::TAB_ACTIVATED;
            }
        }
        else {
            // Window title changes of other programs also trigger events
            info.eventType = WindowEventType::WINDOW_ACTIVATED;
        }
        break;
    default:
        return;  // Ignore other events
    }

    // Check if this event should be triggered (debounce duplicate events)
    if (!s_instance->ShouldTriggerEvent(hwnd, info.eventType, info.windowTitle)) {
        return;  // Skip duplicate event
    }

    // Trigger callbacks
    s_instance->TriggerCallbacks(info);
}

WindowInfo WindowEventMonitor::GetWindowInfo(HWND hwnd) {
    WindowInfo info;
    info.hwnd = hwnd;
    info.timestamp = std::chrono::system_clock::now();

    // Get window title - Use Unicode version
    wchar_t windowTitle[256] = { 0 };
    GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));
    info.windowTitle = windowTitle;

    // Get window class name - Use Unicode version
    wchar_t className[256] = { 0 };
    GetClassNameW(hwnd, className, sizeof(className) / sizeof(wchar_t));
    info.className = className;

    // Get process and thread ID
    info.threadId = GetWindowThreadProcessId(hwnd, &info.processId);

    // Get process name and path
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
        wchar_t buffer[MAX_PATH]; // Unicode buffer
        if (GetModuleBaseNameW(hProcess, nullptr, buffer, MAX_PATH)) { // Unicode version
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
        wchar_t buffer[MAX_PATH]; // Unicode buffer
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size)) { // Unicode version
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
        return TRUE;  // Skip invisible windows
    }

    // Get window title - Use Unicode version
    wchar_t windowTitle[256];
    GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));

    // Skip windows without titles
    if (wcslen(windowTitle) == 0) {
        return TRUE;
    }

    auto* windows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
    WindowInfo info = GetWindowInfo(hwnd);
    windows->push_back(info);

    return TRUE;
}

// Chrome window detection
bool WindowEventMonitor::IsChromeWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // Get process ID
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return false;

    // Get process name
    std::wstring processName = GetProcessName(pid);

    // Convert to lowercase for comparison
    for (auto& ch : processName) {
        ch = towlower(ch);
    }

    // Check if it's Chrome, Edge or other browsers
    return (processName.find(L"chrome.exe") != std::wstring::npos) ||
        (processName.find(L"msedge.exe") != std::wstring::npos) ||
        (processName.find(L"firefox.exe") != std::wstring::npos) ||
        (processName.find(L"opera.exe") != std::wstring::npos);
}

// Try to get Chrome tab information
bool WindowEventMonitor::TryGetChromeTabInfo(HWND hwnd, std::wstring& tabTitle) {
    // Chrome displays the current active tab's title in the window title
    wchar_t title[1024] = { 0 };
    if (GetWindowTextW(hwnd, title, 1023) > 0) { // Use Unicode version
        tabTitle = title;
        return true;
    }
    return false;
}

LRESULT CALLBACK WindowEventMonitor::WindowEventProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Shell Hook processing (if needed)
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ========================================
// Deduplication Helper
// ========================================

bool WindowEventMonitor::ShouldTriggerEvent(HWND hwnd, WindowEventType eventType,
    const std::wstring& title) {
    std::lock_guard<std::mutex> lock(m_lastEventMutex);

    // Check if this event is same as the last one
    if (m_lastEvent.hwnd == hwnd && m_lastEvent.windowTitle == title) {
        // Same window and same title as last event, skip
        PE_INFO_THIS("[WindowEventMonitor] Skipped duplicate: "
            << pe_base::WindowsHelper::ConvertToChar(title.c_str()).ToString()
            << " (same as previous event)")
            return false;
    }

    // Different event, update last event and trigger
    m_lastEvent.hwnd = hwnd;
    m_lastEvent.windowTitle = title;

    return true;
}
