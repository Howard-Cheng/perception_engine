#include "platform/MouseTracker.h"
#include "DatabaseTypes.h"  // Add this include for database::MouseEvent
#include <iostream>
#include <sstream>
#include <iomanip>
#include <psapi.h>
#include <atlbase.h>
#include <UIAutomationClient.h>
#include <ShellScalingApi.h>
#include "platform/WindowsAPIs.h"

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "Shcore.lib")

MouseTracker* MouseTracker::s_instance = nullptr;

MouseTracker::MouseTracker()
    : m_mouseHook(nullptr)
    , m_pAutomation(nullptr)
    , m_isRunning(false)
    , m_lastClickTime(0)
    , m_isDragging(false)
    , m_dragWindow(nullptr)
{
    m_lastClickPos.x = 0;
    m_lastClickPos.y = 0;
    m_dragStartPos.x = 0;
    m_dragStartPos.y = 0;
    s_instance = this;
}

MouseTracker::~MouseTracker() {
    Stop();
    if (m_pAutomation) {
        m_pAutomation->Release();
        m_pAutomation = nullptr;
    }
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    s_instance = nullptr;
}

bool MouseTracker::Initialize() {
    // Set DPI awareness: Use Per-Monitor V2 mode
    // This ensures correct coordinate system on high DPI displays
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return false;
    }

    // Create UI Automation instance
    hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        (void**)&m_pAutomation);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    // Open log file
    m_logFile.open(L"mouse_operations_log.txt", std::ios::app);
    if (!m_logFile.is_open()) {
        return false;
    }

    m_logFile << "\n========== Mouse Tracker Started at " << GetCurrentTimeString() << " ==========\n" << std::flush;

    return true;
}

void MouseTracker::Start() {
    if (m_isRunning) return;

    m_isRunning = true;

    // Start processing thread
    m_processingThread = std::thread(&MouseTracker::ProcessRecordQueue, this);

    // Critical fix: Hook must be installed in message loop thread
    // Start message loop thread and install hook in that thread
    m_messageLoopThread = std::thread([this]() {
        std::wcout << L"[MouseTracker] Message loop thread starting...\n" << std::flush;
        m_logFile << "Message loop thread starting.\n" << std::flush;

        // Install hook in message loop thread (critical!)
        m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);

        if (m_mouseHook) {
            std::wcout << L"[MouseTracker] Mouse hook installed successfully in message loop thread.\n" << std::flush;
            m_logFile << "Mouse hook installed successfully in message loop thread.\n" << std::flush;

            // Start message loop
            MSG msg;
            std::wcout << L"[MouseTracker] Starting message loop...\n" << std::flush;

            while (m_isRunning && GetMessage(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            std::wcout << L"[MouseTracker] Message loop ended.\n" << std::flush;
            m_logFile << "Message loop ended.\n" << std::flush;

            // Clean up hook
            if (m_mouseHook) {
                UnhookWindowsHookEx(m_mouseHook);
                m_mouseHook = nullptr;
                std::wcout << L"[MouseTracker] Mouse hook uninstalled.\n" << std::flush;
            }
        }
        else {
            // Hook installation failed
            DWORD error = GetLastError();
            std::wcerr << L"[MouseTracker] Mouse hook installation FAILED! Error code: " << error << L"\n" << std::flush;
            m_logFile << "Mouse hook installation FAILED! Error code: " << error << L"\n" << std::flush;
            m_isRunning = false;
        }
    });

    // Give message loop some time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (m_mouseHook) {
        std::wcout << L"[MouseTracker] Hook should be active now. Try clicking your mouse!\n" << std::flush;
    }
    else {
        std::wcerr << L"[MouseTracker] Hook installation failed! Mouse tracking will not work.\n" << std::flush;
    }
}

void MouseTracker::Stop() {
    if (!m_isRunning) return;

    m_isRunning = false;

    // Stop message loop thread (by sending quit message)
    if (m_messageLoopThread.joinable()) {
        // Send WM_QUIT message to stop GetMessage loop
        PostQuitMessage(0);
        m_messageLoopThread.join();
        std::wcout << L"[MouseTracker] Message loop thread stopped.\n" << std::flush;
    }

    // Wake up processing thread and wait for it to finish
    m_queueCondition.notify_all();
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    // Note: Hook has already been cleaned up in message loop thread

    if (m_logFile.is_open()) {
        m_logFile << "========== Mouse Tracker Stopped at " << GetCurrentTimeString() << " ==========\n" << std::flush;
    }
}

LRESULT CALLBACK MouseTracker::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    static int callCount = 0;
    if (callCount < 5) {  // Only output first 5 times to avoid flooding
        std::wcout << L"[HOOK] MouseHookProc called! nCode=" << nCode
            << L", wParam=" << wParam << L"\n" << std::flush;
        callCount++;
    }

    if (nCode >= 0 && s_instance && s_instance->m_isRunning) {
        const MSLLHOOKSTRUCT* mouseInfo = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        // Ignore window dragging (by detecting if in non-client area)
        HWND hwnd = WindowFromPoint(mouseInfo->pt);
        if (hwnd) {
            LRESULT hitTest = SendMessage(hwnd, WM_NCHITTEST, 0,
                MAKELPARAM(mouseInfo->pt.x, mouseInfo->pt.y));
            // If in title bar or border, ignore
            if (hitTest == HTCAPTION || hitTest == HTBORDER || hitTest == HTLEFT ||
                hitTest == HTRIGHT || hitTest == HTTOP || hitTest == HTBOTTOM) {
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }
        }

        s_instance->ProcessMouseEvent(wParam, mouseInfo);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void MouseTracker::ProcessMouseEvent(WPARAM wParam, const MSLLHOOKSTRUCT* mouseInfo) {
    MouseEventType eventType = MouseEventType::UNKNOWN;
    DWORD currentTime = GetTickCount();

    switch (wParam) {
    case WM_LBUTTONDOWN: {
        // Record drag start
        m_isDragging = true;
        m_dragStartPos = mouseInfo->pt;
        m_dragWindow = WindowFromPoint(mouseInfo->pt);

        // Detect double click
        if (currentTime - m_lastClickTime < GetDoubleClickTime() &&
            abs(mouseInfo->pt.x - m_lastClickPos.x) < 5 &&
            abs(mouseInfo->pt.y - m_lastClickPos.y) < 5) {
            eventType = MouseEventType::LEFT_DOUBLE_CLICK;
            m_lastClickTime = 0; // Reset to avoid triple click being detected as double click
            m_isDragging = false; // Double click doesn't count as drag
        }
        else {
            // Don't immediately trigger LEFT_CLICK, wait for LBUTTONUP to determine if it's text selection
            m_lastClickTime = currentTime;
            m_lastClickPos = mouseInfo->pt;
            return; // Temporarily don't process, wait for LBUTTONUP
        }
        break;
    }
    case WM_LBUTTONUP: {
        // Check if it's text selection (drag distance exceeds threshold)
        if (m_isDragging) {
            int dragDistance = abs(mouseInfo->pt.x - m_dragStartPos.x) +
                abs(mouseInfo->pt.y - m_dragStartPos.y);

            // If drag distance exceeds 10 pixels, consider it text selection
            if (dragDistance > 10) {
                eventType = MouseEventType::TEXT_SELECTION;

                // Quickly enqueue text selection event
                PendingMouseEvent event;
                event.eventType = eventType;
                event.position = mouseInfo->pt;
                event.pointWindow = m_dragWindow;
                event.timestamp = std::chrono::system_clock::now();

                {
                    std::lock_guard<std::mutex> lock(s_instance->m_queueMutex);
                    s_instance->m_eventQueue.push(event);
                }
                s_instance->m_queueCondition.notify_one();

                m_isDragging = false;
                return; // Text selection event processed, don't process as click
            }
            else {
                // Small drag distance, regular click
                eventType = MouseEventType::LEFT_CLICK;
            }

            m_isDragging = false;
        }
        else {
            // No LBUTTONDOWN recorded, could be other situation
            return;
        }
        break;
    }
    case WM_RBUTTONDOWN:
        eventType = MouseEventType::RIGHT_CLICK;
        m_isDragging = false; // Right click doesn't count as drag
        break;
    default:
        return;
    }

    if (eventType != MouseEventType::UNKNOWN && eventType != MouseEventType::TEXT_SELECTION) {
        // Quickly enqueue, don't block hook
        PendingMouseEvent event;
        event.eventType = eventType;
        event.position = mouseInfo->pt;

        // Critical fix: In multi-monitor environment, WindowFromPoint may return child window with incorrect coordinate system
        // Should get top-level window instead of child window
        HWND pointWindow = WindowFromPoint(mouseInfo->pt);
        HWND foregroundWindow = GetForegroundWindow();

        // Get top-level parent window of pointWindow
        HWND topLevelWindow = pointWindow;
        if (pointWindow) {
            HWND parent = pointWindow;
            while (parent) {
                HWND nextParent = GetParent(parent);
                if (!nextParent) {
                    topLevelWindow = parent;
                    break;
                }
                parent = nextParent;
            }
        }

        // Verify if top-level window matches foreground window
        bool useForeground = (topLevelWindow != foregroundWindow);

        // Prefer foreground window (more reliable), unless topLevelWindow actually contains click coordinates
        if (topLevelWindow && IsWindow(topLevelWindow)) {
            RECT rect;
            if (GetWindowRect(topLevelWindow, &rect)) {
                POINT pt = mouseInfo->pt;
                if (pt.x >= rect.left && pt.x < rect.right &&
                    pt.y >= rect.top && pt.y < rect.bottom) {
                    useForeground = false;  // Coordinates within range, use topLevelWindow
                }
            }
        }

        event.pointWindow = useForeground ? foregroundWindow : topLevelWindow;

        // Debug: Output click information
#ifdef _DEBUG
        wchar_t className[256] = { 0 };
        wchar_t topClassName[256] = { 0 };
        wchar_t fgClassName[256] = { 0 };
        if (pointWindow && IsWindow(pointWindow)) {
            GetClassNameW(pointWindow, className, 256);
        }
        if (topLevelWindow && IsWindow(topLevelWindow)) {
            GetClassNameW(topLevelWindow, topClassName, 256);
        }
        if (foregroundWindow && IsWindow(foregroundWindow)) {
            GetClassNameW(foregroundWindow, fgClassName, 256);
        }
        std::wcout << L"[HOOK] Click at (" << mouseInfo->pt.x << L", " << mouseInfo->pt.y << L")\n"
            << L"  PointWindow: " << pointWindow << L" Class: " << className << L"\n"
            << L"  TopLevelWindow: " << topLevelWindow << L" Class: " << topClassName << L"\n"
            << L"  ForegroundWindow: " << foregroundWindow << L" Class: " << fgClassName << L"\n"
            << L"  Using: " << (useForeground ? L"ForegroundWindow" : L"TopLevelWindow") << L"\n";

        // Output display information
        RECT topRect = { 0 };
        if (event.pointWindow && IsWindow(event.pointWindow)) {
            GetWindowRect(event.pointWindow, &topRect);
            std::wcout << L"  TargetWindow Rect: (" << topRect.left << L", " << topRect.top
                << L") - (" << topRect.right << L", " << topRect.bottom << L")\n";

            bool isInside = (mouseInfo->pt.x >= topRect.left && mouseInfo->pt.x < topRect.right &&
                mouseInfo->pt.y >= topRect.top && mouseInfo->pt.y < topRect.bottom);
            std::wcout << L"  Click is " << (isInside ? L"INSIDE" : L"OUTSIDE")
                << L" target window bounds\n";
        }
#endif

        event.timestamp = std::chrono::system_clock::now();

        {
            std::lock_guard<std::mutex> lock(s_instance->m_queueMutex);
            s_instance->m_eventQueue.push(event);
        }
        s_instance->m_queueCondition.notify_one();
    }
}

void MouseTracker::ProcessRecordQueue() {
    // Initialize COM in worker thread (each thread needs separate initialization)
    auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (m_isRunning) {
        PendingMouseEvent event;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            // Wait for queue to have data or stop signal
            m_queueCondition.wait(lock, [this] {
                return !m_eventQueue.empty() || !m_isRunning;
                });

            if (!m_isRunning && m_eventQueue.empty()) {
                break;
            }

            if (!m_eventQueue.empty()) {
                event = m_eventQueue.front();
                m_eventQueue.pop();
            }
            else {
                continue;
            }
        }

        // Process time-consuming operations in worker thread
        RecordMouseOperation(event.eventType, event.position, event.pointWindow);
    }

    CoUninitialize();
}

void MouseTracker::RecordMouseOperation(MouseEventType eventType, POINT position, HWND pointWindow) {
    MouseOperationRecord record;
    record.timestamp = std::chrono::system_clock::now();
    record.eventType = eventType;
    record.position = position;

    // Critical improvement: First immediately get element content (before UI state changes)
    database::MouseEvent dbMouseEvent;  // Use database namespace
    try {
        if (eventType == MouseEventType::TEXT_SELECTION) {
            // For text selection, use special method to get selected text
            dbMouseEvent.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            dbMouseEvent.eventType = "TextSelection";
            dbMouseEvent.posX = position.x;
            dbMouseEvent.posY = position.y;
            dbMouseEvent.content = WindowsAPIs::WideStringToUtf8(GetSelectedText(pointWindow));
            m_mouseEvents.push_back(dbMouseEvent);
        }
        else {
            // For click events, get element content
            m_clickedCount++;
        }
    }
    catch (...) {
        std::wcout << L"[Error getting content]" << std::endl;
    }

    // Write to log file (asynchronously)
    if (m_logFile.is_open()) {
        m_logFile << dbMouseEvent.content << "\n" << std::flush;
    }
}

MouseTracker::ElementInfo MouseTracker::GetElementContentAtPoint(POINT pt, HWND targetWindow) {
    ElementInfo result;
    result.content = L"";
    result.elementType = L"Unknown";

    if (!m_pAutomation) return result;

    // Use tree traversal solution (more accurate, lower latency)
    HWND hwnd = targetWindow;
    if (!hwnd || !IsWindow(hwnd)) {
        hwnd = GetForegroundWindow();
    }

    if (!hwnd || !IsWindow(hwnd)) {
        return result;
    }

    // Get root element
    IUIAutomationElement* rootElement = nullptr;
    HRESULT hr = m_pAutomation->ElementFromHandle(hwnd, &rootElement);

    if (FAILED(hr) || !rootElement) {
        return result;
    }

    // Optimization: First find content area to reduce traversal scope
    IUIAutomationElement* contentArea = FindContentArea(rootElement);
    IUIAutomationElement* searchRoot = contentArea ? contentArea : rootElement;

    // Get TreeWalker
    IUIAutomationTreeWalker* walker = nullptr;
    hr = m_pAutomation->get_RawViewWalker(&walker);

    if (FAILED(hr) || !walker) {
        if (contentArea) contentArea->Release();
        rootElement->Release();
        return result;
    }

    // Find target element in element tree
    IUIAutomationElement* targetElement = FindElementAtPointInTree(searchRoot, pt, walker, 0);

    // If not found in content area, try searching in entire window
    if (!targetElement && contentArea) {
        targetElement = FindElementAtPointInTree(rootElement, pt, walker, 0);
    }

    walker->Release();

    if (targetElement) {
        // Get element information
        CONTROLTYPEID controlType;
        targetElement->get_CurrentControlType(&controlType);
        result.elementType = GetElementTypeString(controlType);

        // Get content
        result.content = TryGetElementContent(targetElement, controlType);

        if (result.content.empty()) {
            // If current element has no content, recursively search child elements
            IUIAutomationTreeWalker* contentWalker = nullptr;
            m_pAutomation->get_RawViewWalker(&contentWalker);
            if (contentWalker) {
                result.content = TraverseForContent(targetElement, contentWalker, 0, 3);
                contentWalker->Release();
            }
        }

        targetElement->Release();
    }
    else {
        // Fallback solution: If tree traversal fails, use ElementFromPoint
        IUIAutomationElement* pointElement = nullptr;
        hr = m_pAutomation->ElementFromPoint(pt, &pointElement);

        if (SUCCEEDED(hr) && pointElement) {
            CONTROLTYPEID controlType;
            pointElement->get_CurrentControlType(&controlType);
            result.elementType = GetElementTypeString(controlType);
            result.content = TryGetElementContent(pointElement, controlType);

            if (result.content.empty()) {
                IUIAutomationTreeWalker* contentWalker = nullptr;
                m_pAutomation->get_RawViewWalker(&contentWalker);
                if (contentWalker) {
                    result.content = TraverseForContent(pointElement, contentWalker, 0, 3);
                    contentWalker->Release();
                }
            }

            pointElement->Release();
        }
    }

    if (contentArea) contentArea->Release();
    rootElement->Release();

    if (result.content.empty()) {
        result.content = L"[No Content Found]";
    }

    return result;
}

// New: Find content area (similar to BrowserContentExtractor::FindDocumentElement)
IUIAutomationElement* MouseTracker::FindContentArea(IUIAutomationElement* rootElement) {
    if (!m_pAutomation || !rootElement) {
        return nullptr;
    }

    // 1. First try to find Document control (suitable for browsers)
    IUIAutomationCondition* condition = nullptr;
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_DocumentControlTypeId;

    HRESULT hr = m_pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &condition);

    if (SUCCEEDED(hr) && condition) {
        IUIAutomationElement* docElement = nullptr;
        hr = rootElement->FindFirst(TreeScope_Descendants, condition, &docElement);
        condition->Release();

        if (SUCCEEDED(hr) && docElement) {
            return docElement;
        }
    }

    // 2. If Document not found, find suitable Pane (suitable for Teams and other applications)
    varProp.lVal = UIA_PaneControlTypeId;
    hr = m_pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &condition);

    if (SUCCEEDED(hr) && condition) {
        IUIAutomationElementArray* paneArray = nullptr;
        hr = rootElement->FindAll(TreeScope_Descendants, condition, &paneArray);
        condition->Release();

        if (SUCCEEDED(hr) && paneArray) {
            int length = 0;
            paneArray->get_Length(&length);

            // Traverse all Panes to find the one most likely to be content area
            for (int i = 0; i < length && i < 20; i++) {
                IUIAutomationElement* pane = nullptr;
                if (SUCCEEDED(paneArray->GetElement(i, &pane)) && pane) {
                    // Check Name and AutomationId, exclude toolbars, bookmark bars, etc.
                    BSTR name = nullptr;
                    BSTR automationId = nullptr;
                    pane->get_CurrentName(&name);
                    pane->get_CurrentAutomationId(&automationId);

                    std::wstring nameStr = name ? name : L"";
                    std::wstring idStr = automationId ? automationId : L"";

                    if (name) SysFreeString(name);
                    if (automationId) SysFreeString(automationId);

                    // Exclude Panes that are clearly not content areas
                    bool isExcluded =
                        nameStr.find(L"Toolbar") != std::wstring::npos ||
                        nameStr.find(L"Bookmark") != std::wstring::npos ||
                        nameStr.find(L"Tab Bar") != std::wstring::npos ||
                        nameStr.find(L"Navigation") != std::wstring::npos ||
                        idStr.find(L"Toolbar") != std::wstring::npos ||
                        idStr.find(L"TabBar") != std::wstring::npos;

                    if (!isExcluded) {
                        paneArray->Release();
                        return pane;
                    }

                    pane->Release();
                }
            }

            paneArray->Release();
        }
    }

    // 3. If none found, return nullptr (use root element)
    return nullptr;
}

// New: Find element containing specified coordinates in element tree (returns smallest matching element)
IUIAutomationElement* MouseTracker::FindElementAtPointInTree(IUIAutomationElement* element, POINT pt, IUIAutomationTreeWalker* walker, int depth) {
    if (!element || !walker || depth > 15) {
        return nullptr;
    }

    // Check current element's bounding rectangle
    RECT rect;
    HRESULT hr = element->get_CurrentBoundingRectangle(&rect);

    if (FAILED(hr)) {
        return nullptr;
    }

    // Critical fix: For Document elements, if bounding rectangle is (0,0)-(0,0), use parent window's bounds
    if (rect.left == 0 && rect.top == 0 && rect.right == 0 && rect.bottom == 0) {
        // Try to get window handle from element
        IUIAutomationElement* rootElement = element;
        HWND hwnd = nullptr;

        // Search upward until valid window handle is found
        while (rootElement) {
            UIA_HWND uiaHwnd = 0;
            if (SUCCEEDED(rootElement->get_CurrentNativeWindowHandle(&uiaHwnd)) && uiaHwnd) {
                hwnd = (HWND)(LONG_PTR)uiaHwnd;
                break;
            }

            IUIAutomationTreeWalker* tempWalker = nullptr;
            if (SUCCEEDED(m_pAutomation->get_RawViewWalker(&tempWalker)) && tempWalker) {
                IUIAutomationElement* parent = nullptr;
                if (SUCCEEDED(tempWalker->GetParentElement(rootElement, &parent)) && parent) {
                    if (rootElement != element) rootElement->Release();
                    rootElement = parent;
                }
                else {
                    tempWalker->Release();
                    break;
                }
                tempWalker->Release();
            }
            else {
                break;
            }
        }

        if (hwnd && IsWindow(hwnd)) {
            GetWindowRect(hwnd, &rect);
        }

        if (rootElement != element) rootElement->Release();
    }

    // If point not within current element, return null
    if (pt.x < rect.left || pt.x > rect.right || pt.y < rect.top || pt.y > rect.bottom) {
        return nullptr;
    }

    // Key improvement: Point is within current element, first check if current element has text content
    CONTROLTYPEID controlType;
    element->get_CurrentControlType(&controlType);
    std::wstring currentContent = TryGetElementContent(element, controlType);

    // Continue searching child elements to see if there are more precise (smaller area) child elements with content
    IUIAutomationElement* child = nullptr;
    hr = walker->GetFirstChildElement(element, &child);

    IUIAutomationElement* bestMatch = nullptr;
    LONG bestArea = LONG_MAX;
    bool bestHasContent = false;

    while (SUCCEEDED(hr) && child) {
        // Recursively search child elements
        IUIAutomationElement* childMatch = FindElementAtPointInTree(child, pt, walker, depth + 1);

        if (childMatch) {
            // Check if this child element has content
            CONTROLTYPEID childType;
            childMatch->get_CurrentControlType(&childType);
            std::wstring childContent = TryGetElementContent(childMatch, childType);
            bool childHasContent = !childContent.empty();

            // Calculate area
            RECT childRect;
            if (SUCCEEDED(childMatch->get_CurrentBoundingRectangle(&childRect))) {
                LONG area = (childRect.right - childRect.left) * (childRect.bottom - childRect.top);

                // Prefer elements with content, then elements with smaller area
                bool isBetter = false;
                if (childHasContent && !bestHasContent) {
                    isBetter = true;  // Has content is better than no content
                }
                else if (childHasContent == bestHasContent && area > 0 && area < bestArea) {
                    isBetter = true;  // Same content status, choose smaller area
                }

                if (isBetter) {
                    if (bestMatch) bestMatch->Release();
                    bestMatch = childMatch;
                    bestArea = area;
                    bestHasContent = childHasContent;
                }
                else {
                    childMatch->Release();
                }
            }
            else {
                childMatch->Release();
            }
        }

        // Get next sibling
        IUIAutomationElement* next = nullptr;
        hr = walker->GetNextSiblingElement(child, &next);
        child->Release();
        child = next;
    }

    // Decision logic:
    // 1. If found child element with content, return it
    // 2. If current element has content but no child elements with content found, return current element
    // 3. If neither has content, return smallest child element or current element
    if (bestMatch && bestHasContent) {
        // Found child element with content
        return bestMatch;
    }
    else if (!currentContent.empty()) {
        // Current element has content, no better child elements found
        if (bestMatch) bestMatch->Release();
        element->AddRef();
        return element;
    }
    else if (bestMatch) {
        // Neither has content, return smallest child element
        return bestMatch;
    }
    else {
        // No child elements, return current element
        element->AddRef();
        return element;
    }
}

// New: Recursively traverse element tree to find content (similar to BrowserContentExtractor::TraverseElementTree)
std::wstring MouseTracker::TraverseForContent(IUIAutomationElement* element, IUIAutomationTreeWalker* walker, int depth, int maxDepth) {
    if (!element || !walker || depth > maxDepth) {
        return L"";
    }

    // First try current element
    CONTROLTYPEID controlType;
    element->get_CurrentControlType(&controlType);
    std::wstring content = TryGetElementContent(element, controlType);

    if (!content.empty()) {
        return content;
    }

    // Recursively traverse child elements
    IUIAutomationElement* child = nullptr;
    HRESULT hr = walker->GetFirstChildElement(element, &child);

    while (SUCCEEDED(hr) && child) {
        std::wstring childContent = TraverseForContent(child, walker, depth + 1, maxDepth);

        if (!childContent.empty()) {
            child->Release();
            return childContent;
        }

        // Get next sibling element
        IUIAutomationElement* next = nullptr;
        hr = walker->GetNextSiblingElement(child, &next);
        child->Release();
        child = next;
    }

    return L"";
}

// New helper function: Try to get content from element (encapsulates all retrieval methods)
std::wstring MouseTracker::TryGetElementContent(IUIAutomationElement* element, CONTROLTYPEID controlType) {
    if (!element) return L"";

    std::wstring result;
    HRESULT hr;

    // 1. First try to get Name property
    BSTR name = nullptr;
    if (SUCCEEDED(element->get_CurrentName(&name)) && name) {
        std::wstring nameStr = name;
        SysFreeString(name);

        // Trim leading and trailing whitespace characters
        nameStr = TrimWhitespace(nameStr);

        if (!nameStr.empty()) {
            result = nameStr;

            // For hyperlinks, try to append URL
            if (controlType == UIA_HyperlinkControlTypeId) {
                IUIAutomationValuePattern* valuePattern = nullptr;
                if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId,
                    __uuidof(IUIAutomationValuePattern), (void**)&valuePattern)) && valuePattern) {
                    BSTR url = nullptr;
                    if (SUCCEEDED(valuePattern->get_CurrentValue(&url)) && url) {
                        std::wstring urlStr = url;
                        SysFreeString(url);
                        urlStr = TrimWhitespace(urlStr);
                        if (!urlStr.empty()) {
                            result += L" - " + urlStr;
                        }
                    }
                    valuePattern->Release();
                }
            }

            return result;
        }
    }

    // 2. Try ValuePattern (suitable for edit boxes, input fields, etc.)
    IUIAutomationValuePattern* valuePattern = nullptr;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId,
        __uuidof(IUIAutomationValuePattern), (void**)&valuePattern)) && valuePattern) {
        BSTR value = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&value)) && value) {
            std::wstring valueStr = value;
            SysFreeString(value);
            valueStr = TrimWhitespace(valueStr);
            if (!valueStr.empty()) {
                result = valueStr;
                valuePattern->Release();
                return result;
            }
        }
        valuePattern->Release();
    }

    // 3. Try TextPattern (suitable for text content, documents, etc.)
    IUIAutomationTextPattern* textPattern = nullptr;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_TextPatternId,
        __uuidof(IUIAutomationTextPattern), (void**)&textPattern)) && textPattern) {
        IUIAutomationTextRange* textRange = nullptr;
        if (SUCCEEDED(textPattern->get_DocumentRange(&textRange)) && textRange) {
            BSTR text = nullptr;
            if (SUCCEEDED(textRange->GetText(-1, &text)) && text) {
                std::wstring textStr = text;
                SysFreeString(text);
                textStr = TrimWhitespace(textStr);
                if (!textStr.empty()) {
                    result = textStr;
                    textRange->Release();
                    textPattern->Release();
                    return result;
                }
            }
            textRange->Release();
        }
        textPattern->Release();
    }

    // 4. Try HelpText as fallback
    BSTR helpText = nullptr;
    if (SUCCEEDED(element->get_CurrentHelpText(&helpText)) && helpText) {
        std::wstring helpStr = helpText;
        SysFreeString(helpText);
        helpStr = TrimWhitespace(helpStr);
        if (!helpStr.empty()) {
            result = helpStr;
            return result;
        }
    }

    return result;  // Return empty string indicating no content found
}

std::wstring MouseTracker::GetElementTypeString(CONTROLTYPEID controlType) {
    switch (controlType) {
    case UIA_ButtonControlTypeId: return L"Button";
    case UIA_HyperlinkControlTypeId: return L"Hyperlink";
    case UIA_TextControlTypeId: return L"Text";
    case UIA_EditControlTypeId: return L"TextBox";
    case UIA_TabItemControlTypeId: return L"Tab";
    case UIA_MenuItemControlTypeId: return L"MenuItem";
    case UIA_CheckBoxControlTypeId: return L"CheckBox";
    case UIA_RadioButtonControlTypeId: return L"RadioButton";
    case UIA_ComboBoxControlTypeId: return L"ComboBox";
    case UIA_ListItemControlTypeId: return L"ListItem";
    case UIA_ImageControlTypeId: return L"Image";
    default: return L"Unknown";
    }
}

HWND MouseTracker::GetRootOwnerWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return nullptr;
    }

    // Get top-level owner window
    HWND rootWindow = GetAncestor(hwnd, GA_ROOTOWNER);
    if (!rootWindow) {
        rootWindow = hwnd;
    }

    return rootWindow;
}

std::wstring MouseTracker::GetApplicationName(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return L"Unknown";
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == 0) {
        return L"Unknown";
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        return L"Unknown";
    }

    wchar_t processName[MAX_PATH] = L"";
    DWORD size = MAX_PATH;

    if (QueryFullProcessImageNameW(hProcess, 0, processName, &size)) {
        std::wstring fullPath(processName);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            CloseHandle(hProcess);
            return fullPath.substr(lastSlash + 1);
        }
    }

    CloseHandle(hProcess);
    return L"Unknown";
}

std::wstring MouseTracker::GetWindowTitle(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return L"";
    }

    wchar_t title[512] = L"";
    int length = GetWindowTextW(hwnd, title, 512);

    if (length > 0) {
        return std::wstring(title);
    }

    // If window title is empty, try to get class name
    wchar_t className[256] = L"";
    if (GetClassNameW(hwnd, className, 256) > 0) {
        return std::wstring(L"[") + className + L"]";
    }

    return L"";
}

void MouseTracker::CleanupOldRecords() {
    auto now = std::chrono::system_clock::now();
    auto oneHourAgo = now - std::chrono::hours(1);

    m_records.erase(
        std::remove_if(m_records.begin(), m_records.end(),
            [oneHourAgo](const MouseOperationRecord& record) {
                return record.timestamp < oneHourAgo;
            }),
        m_records.end()
    );
}

void MouseTracker::SaveToFile(const std::wstring& filename) {
    std::wofstream file(filename);
    if (!file.is_open()) return;

    file << L"{\n  \"records\": [\n";

    std::lock_guard<std::mutex> lock(m_recordsMutex);
    for (size_t i = 0; i < m_records.size(); ++i) {
        file << L"    " << m_records[i].toJson();
        if (i < m_records.size() - 1) {
            file << L",";
        }
        file << L"\n";
    }

    file << L"  ]\n}\n";
    file.close();
}

std::wstring MouseTracker::GetAllRecordsAsJson() {
    std::wstringstream ss;
    ss << L"{\n  \"records\": [\n";

    std::lock_guard<std::mutex> lock(m_recordsMutex);

    if (m_records.empty()) {
        ss << L"  ]\n}";
        return ss.str();
    }

    // New logic: Merge adjacent records with same applicationName
    std::vector<std::pair<std::wstring, std::vector<MouseOperationRecord>>> groupedRecords;

    std::wstring currentApp;
    std::vector<MouseOperationRecord> currentTracks;

    for (const auto& record : m_records) {
        if (record.applicationName != currentApp) {
            // Application switched, save previous group
            if (!currentTracks.empty()) {
                groupedRecords.push_back({ currentApp, currentTracks });
            }
            // Start new group
            currentApp = record.applicationName;
            currentTracks.clear();
        }
        // Add to current group
        currentTracks.push_back(record);
    }

    // Add last group
    if (!currentTracks.empty()) {
        groupedRecords.push_back({ currentApp, currentTracks });
    }

    // Generate JSON
    for (size_t i = 0; i < groupedRecords.size(); ++i) {
        const auto& [appName, tracks] = groupedRecords[i];

        // JSON escape function
        auto escapeJson = [](const std::wstring& str) -> std::wstring {
            std::wstring escaped;
            for (wchar_t c : str) {
                switch (c) {
                case L'\\': escaped += L"\\\\"; break;
                case L'\"': escaped += L"\\\""; break;
                case L'\n': escaped += L"\\n"; break;
                case L'\r': escaped += L"\\r"; break;
                case L'\t': escaped += L"\\t"; break;
                default: escaped += c; break;
                }
            }
            return escaped;
            };

        ss << L"    {\n";
        ss << L"      \"applicationName\": \"" << escapeJson(appName) << L"\",\n";
        ss << L"      \"tracks\": [\n";

        // Output all records for this application
        for (size_t j = 0; j < tracks.size(); ++j) {
            const auto& record = tracks[j];

            // Convert timestamp
            auto time_t_val = std::chrono::system_clock::to_time_t(record.timestamp);
            std::tm tm_val;
            localtime_s(&tm_val, &time_t_val);
            wchar_t timeStr[100];
            wcsftime(timeStr, 100, L"%Y-%m-%d %H:%M:%S", &tm_val);

            ss << L"        {\n";
            ss << L"          \"timestamp\": \"" << timeStr << L"\",\n";
            ss << L"          \"eventType\": \"" << MouseEventTypeToString(record.eventType) << L"\",\n";
            ss << L"          \"position\": {\"x\": " << record.position.x << L", \"y\": " << record.position.y << L"},\n";
            ss << L"          \"content\": \"" << escapeJson(record.content) << L"\",\n";
            ss << L"          \"windowTitle\": \"" << escapeJson(record.windowTitle) << L"\",\n";
            ss << L"          \"elementType\": \"" << escapeJson(record.elementType) << L"\"\n";
            ss << L"        }";

            if (j < tracks.size() - 1) {
                ss << L",";
            }
            ss << L"\n";
        }

        ss << L"      ]\n";
        ss << L"    }";

        if (i < groupedRecords.size() - 1) {
            ss << L",";
        }
        ss << L"\n";
    }

    ss << L"  ]\n}";
    return ss.str();
}

std::wstring MouseOperationRecord::toJson() const {
    std::wstringstream ss;

    // Convert timestamp to string
    auto time_t_val = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_val;
    localtime_s(&tm_val, &time_t_val);

    wchar_t timeStr[100];
    wcsftime(timeStr, 100, L"%Y-%m-%d %H:%M:%S", &tm_val);

    // JSON escape function
    auto escapeJson = [](const std::wstring& str) -> std::wstring {
        std::wstring escaped;
        for (wchar_t c : str) {
            switch (c) {
            case L'\\': escaped += L"\\\\"; break;
            case L'\"': escaped += L"\\\""; break;
            case L'\n': escaped += L"\\n"; break;
            case L'\r': escaped += L"\\r"; break;
            case L'\t': escaped += L"\\t"; break;
            default: escaped += c; break;
            }
        }
        return escaped;
        };

    ss << L"{\n"
        << L"      \"timestamp\": \"" << timeStr << L"\",\n"
        << L"      \"eventType\": \"" << MouseEventTypeToString(eventType) << L"\",\n"
        << L"      \"position\": {\"x\": " << position.x << L", \"y\": " << position.y << L"},\n"
        << L"      \"content\": \"" << escapeJson(content) << L"\",\n"
        << L"      \"applicationName\": \"" << escapeJson(applicationName) << L"\",\n"
        << L"      \"windowTitle\": \"" << escapeJson(windowTitle) << L"\",\n"
        << L"      \"elementType\": \"" << escapeJson(elementType) << L"\"\n"
        << L"    }";

    return ss.str();
}

std::wstring MouseEventTypeToString(MouseEventType type) {
    switch (type) {
    case MouseEventType::LEFT_CLICK: return L"LeftClick";
    case MouseEventType::LEFT_DOUBLE_CLICK: return L"DoubleClick";
    case MouseEventType::RIGHT_CLICK: return L"RightClick";
    case MouseEventType::TEXT_SELECTION: return L"TextSelection";
    default: return L"Unknown";
    }
}

std::string GetCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val;
    localtime_s(&tm_val, &time_t_val);

    wchar_t buffer[100];
    wcsftime(buffer, 100, L"%Y-%m-%d %H:%M:%S", &tm_val);
    return WindowsAPIs::WideStringToUtf8(buffer);
}

// Trim leading and trailing whitespace characters (spaces, tabs, newlines, etc.)
std::wstring TrimWhitespace(const std::wstring& str) {
    if (str.empty()) return str;

    // Find first non-whitespace character
    size_t start = 0;
    while (start < str.length() && ::iswspace(str[start])) {
        start++;
    }

    // If all characters are whitespace
    if (start == str.length()) {
        return L"";
    }

    // Find last non-whitespace character
    size_t end = str.length() - 1;
    while (end > start && ::iswspace(str[end])) {
        end--;
    }

    // Return trimmed string
    return str.substr(start, end - start + 1);
}

// New: Get selected text
std::wstring MouseTracker::GetSelectedText(HWND targetWindow) {
    if (!m_pAutomation) return L"";

    HWND hwnd = targetWindow;
    if (!hwnd || !IsWindow(hwnd)) {
        hwnd = GetForegroundWindow();
    }

    if (!hwnd || !IsWindow(hwnd)) {
        return L"";
    }

    // Get focus element (Focus Element)
    IUIAutomationElement* focusElement = nullptr;
    HRESULT hr = m_pAutomation->GetFocusedElement(&focusElement);

    if (SUCCEEDED(hr) && focusElement) {
        // Try to use TextPattern to get selected text
        IUIAutomationTextPattern* textPattern = nullptr;
        hr = focusElement->GetCurrentPatternAs(UIA_TextPatternId,
            __uuidof(IUIAutomationTextPattern), (void**)&textPattern);

        if (SUCCEEDED(hr) && textPattern) {
            // Get selected text range
            IUIAutomationTextRangeArray* selectionArray = nullptr;
            hr = textPattern->GetSelection(&selectionArray);

            if (SUCCEEDED(hr) && selectionArray) {
                int length = 0;
                selectionArray->get_Length(&length);

                std::wstring selectedText;

                // Traverse all selected ranges (there might be multiple)
                for (int i = 0; i < length; i++) {
                    IUIAutomationTextRange* textRange = nullptr;
                    if (SUCCEEDED(selectionArray->GetElement(i, &textRange)) && textRange) {
                        BSTR text = nullptr;
                        if (SUCCEEDED(textRange->GetText(-1, &text)) && text) {
                            if (i > 0) selectedText += L" ";
                            selectedText += text;
                            SysFreeString(text);
                        }
                        textRange->Release();
                    }
                }

                selectionArray->Release();
                textPattern->Release();
                focusElement->Release();

                selectedText = TrimWhitespace(selectedText);
                if (!selectedText.empty()) {
                    return selectedText;
                }
            }
            else {
                textPattern->Release();
            }
        }

        focusElement->Release();
    }

    // Fallback solution: Try to get from clipboard (if user copied selected text)
    // Note: This method is not perfect because clipboard may contain previous content
    // But as a fallback solution, better than nothing

    // Try to search from window's root element
    IUIAutomationElement* rootElement = nullptr;
    hr = m_pAutomation->ElementFromHandle(hwnd, &rootElement);

    if (SUCCEEDED(hr) && rootElement) {
        // Find elements that support TextPattern
        IUIAutomationTextPattern* textPattern = nullptr;
        hr = rootElement->GetCurrentPatternAs(UIA_TextPatternId,
            __uuidof(IUIAutomationTextPattern), (void**)&textPattern);

        if (SUCCEEDED(hr) && textPattern) {
            IUIAutomationTextRangeArray* selectionArray = nullptr;
            hr = textPattern->GetSelection(&selectionArray);

            if (SUCCEEDED(hr) && selectionArray) {
                int length = 0;
                selectionArray->get_Length(&length);

                std::wstring selectedText;

                for (int i = 0; i < length; i++) {
                    IUIAutomationTextRange* textRange = nullptr;
                    if (SUCCEEDED(selectionArray->GetElement(i, &textRange)) && textRange) {
                        BSTR text = nullptr;
                        if (SUCCEEDED(textRange->GetText(-1, &text)) && text) {
                            if (i > 0) selectedText += L" ";
                            selectedText += text;
                            SysFreeString(text);
                        }
                        textRange->Release();
                    }
                }

                selectionArray->Release();
                textPattern->Release();
                rootElement->Release();

                selectedText = TrimWhitespace(selectedText);
                if (!selectedText.empty()) {
                    return selectedText;
                }
            }
            else {
                textPattern->Release();
            }
        }

        rootElement->Release();
    }

    return L"[No Text Selected]";
}