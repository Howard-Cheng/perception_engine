#pragma once

// IMPORTANT: Define Windows macros BEFORE including any headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <objbase.h>  // For COM definitions including 'interface' keyword
#include <UIAutomationClient.h>
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <fstream>
#include <comdef.h>
#include <oleacc.h>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

// Include DatabaseTypes for MouseEvent definition
#include "database_client/include/DatabaseTypes.h"


#pragma comment(lib, "oleacc.lib")

// Mouse event types
enum class MouseEventType {
    LEFT_CLICK,
    LEFT_DOUBLE_CLICK,
    RIGHT_CLICK,
    TEXT_SELECTION,
    UNKNOWN
};

// Mouse operation record structure
struct MouseOperationRecord {
    std::chrono::system_clock::time_point timestamp;
    MouseEventType eventType;
    POINT position;
    std::wstring content;           // Interaction content (link, button name, text, etc.)
    std::wstring applicationName;   // Application name
    std::wstring windowTitle;       // Window title
    std::wstring elementType;       // Element type (button, link, textbox, etc.)

    std::wstring toJson() const;
};

// Pending mouse event
struct PendingMouseEvent {
    MouseEventType eventType;
    POINT position;
    HWND pointWindow;           // Window at coordinate position (for UI Automation)
    std::chrono::system_clock::time_point timestamp;
};

class MouseTracker {
public:
    MouseTracker();
    ~MouseTracker();

    bool Initialize();
    void Start();
    void Stop();
    void SaveToFile(const std::wstring& filename);
    std::wstring GetAllRecordsAsJson();
    inline void ResetMouseRecords() {
        m_clickedCount = 0;
        m_mouseEvents.clear();
    }
    inline UINT64 GetClickedCount() const {
        return m_clickedCount;
    }

    inline std::vector<database::MouseEvent> GetMouseEvents() const {
        return m_mouseEvents;
    }

private:
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static MouseTracker* s_instance;

    void ProcessMouseEvent(WPARAM wParam, const MSLLHOOKSTRUCT* mouseInfo);
    void RecordMouseOperation(MouseEventType eventType, POINT position, HWND pointWindow);
    void ProcessRecordQueue();  // Worker thread to process record queue
    
    // Return element content and type
    struct ElementInfo {
        std::wstring content;
        std::wstring elementType;
    };
    ElementInfo GetElementContentAtPoint(POINT pt, HWND targetWindow);
    
    std::wstring GetSelectedText(HWND targetWindow);
    std::wstring GetApplicationName(HWND hwnd);
    std::wstring GetWindowTitle(HWND hwnd);
    std::wstring GetElementTypeString(CONTROLTYPEID controlType);
    HWND GetRootOwnerWindow(HWND hwnd);  // Get top-level window
    
    // Get content from element (encapsulates all retrieval methods)
    std::wstring TryGetElementContent(IUIAutomationElement* element, CONTROLTYPEID controlType);
    
    // Recursively traverse element tree to find content
    std::wstring TraverseForContent(IUIAutomationElement* element, IUIAutomationTreeWalker* walker, int depth, int maxDepth);
    
    // Find element at specified coordinates in element tree
    IUIAutomationElement* FindElementAtPointInTree(IUIAutomationElement* element, POINT pt, IUIAutomationTreeWalker* walker, int depth);
    
    // Find content area (similar to BrowserContentExtractor::FindDocumentElement)
    IUIAutomationElement* FindContentArea(IUIAutomationElement* rootElement);
    
    void CleanupOldRecords();  // Clean up records older than 1 hour
    
    HHOOK m_mouseHook;
    IUIAutomation* m_pAutomation;
    
    std::vector<MouseOperationRecord> m_records;
    std::mutex m_recordsMutex;
    
    // Async processing queue
    std::queue<PendingMouseEvent> m_eventQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::thread m_processingThread;
    std::thread m_messageLoopThread;  // Message loop thread (required for hook)
    std::atomic<bool> m_isRunning;
    
    DWORD m_lastClickTime;
    POINT m_lastClickPos;
    
    // Text selection tracking
    bool m_isDragging;
    POINT m_dragStartPos;
    HWND m_dragWindow;

    std::ofstream m_logFile;

    UINT64 m_clickedCount;
    std::vector<database::MouseEvent> m_mouseEvents;
};

// Helper functions
std::wstring MouseEventTypeToString(MouseEventType type);
std::string GetCurrentTimeString();
std::wstring TrimWhitespace(const std::wstring& str);  // Trim leading and trailing whitespace
