#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <UIAutomation.h>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <atlbase.h>
#include <comdef.h>
#include <set>  // For tracking extracted texts to avoid duplicates

// Browser content information structure
struct BrowserContentInfo {
    std::wstring url;              // URL address
    std::wstring title;            // Page title
    std::wstring textContent;      // Text content
    std::wstring htmlContent;      // HTML content
    int elementCount;              // Element count
    std::vector<std::wstring> links; // Link list
    std::vector<std::wstring> images; // Image list
    std::set<std::wstring> seenTexts;  // Track extracted texts for deduplication
};

// Browser type enumeration
enum class BrowserType {
    Chrome,
    Edge,
    Firefox,
    Teams,
    Unknown
};

class BrowserContentExtractor {
private:
    CComPtr<IUIAutomation> pAutomation;
    BrowserType browserType;
    
    // Initialize UI Automation
    bool InitializeUIAutomation();
    
    // Get browser type of window
    BrowserType DetectBrowserType(HWND hwnd);
    
    // Recursively traverse UI element tree
    void TraverseElementTree(IUIAutomationElement* pElement, BrowserContentInfo& info, int depth = 0);
    
    // Extract element information
    void ExtractElementInfo(IUIAutomationElement* pElement, BrowserContentInfo& info);
    
    // Get element text content
    std::wstring GetElementText(IUIAutomationElement* pElement);
    
    // Get element property
    std::wstring GetElementProperty(IUIAutomationElement* pElement, PROPERTYID propertyId);
    
    // Get Chrome/Edge address bar URL
    std::wstring GetChromeEdgeURL(IUIAutomationElement* pRootElement);
    
    // Get Firefox address bar URL
    std::wstring GetFirefoxURL(IUIAutomationElement* pRootElement);
    
    // Find element (by AutomationId or Name)
    CComPtr<IUIAutomationElement> FindElement(IUIAutomationElement* pParent, 
                                               const std::wstring& automationId, 
                                               const std::wstring& name);
    
    // Get control type name
    std::wstring GetControlTypeName(long controlType);

public:
    BrowserContentExtractor();
    ~BrowserContentExtractor();
    
    // Get active browser window content
    bool GetActiveBrowserContent(BrowserContentInfo& outInfo);
    
    // Get browser window content by window handle
    bool GetBrowserContentByHWND(HWND hwnd, BrowserContentInfo& outInfo);
    
    // Print browser content information
    void PrintBrowserContent(const BrowserContentInfo& info);
};
