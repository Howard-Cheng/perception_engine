#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "BrowserContentExtractor.h"
#include "Logger.h"  // Correct path - same directory as BrowserContentExtractor.cpp
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <codecvt>
#include <locale>

// Helper function to convert wstring to string for logging
static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

BrowserContentExtractor::BrowserContentExtractor() : browserType(BrowserType::Unknown) {
    CoInitialize(NULL);
    InitializeUIAutomation();
}

BrowserContentExtractor::~BrowserContentExtractor() {
    pAutomation.Release();
    CoUninitialize();
}

bool BrowserContentExtractor::InitializeUIAutomation() {
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), NULL,
        CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation),
        (void**)&pAutomation);
    
    if (FAILED(hr) || !pAutomation) {
        LOG_ERROR_FMT("Initialize UI Automation failed! HRESULT: 0x%lX", hr);
        return false;
    }
    return true;
}

BrowserType BrowserContentExtractor::DetectBrowserType(HWND hwnd) {
    wchar_t className[256] = { 0 };
    GetClassNameW(hwnd, className, 256);
    
    std::wstring classNameStr(className);
    
    if (classNameStr.find(L"Chrome") != std::wstring::npos) {
        return BrowserType::Chrome;
    }
    else if (classNameStr.find(L"MozillaWindowClass") != std::wstring::npos) {
        return BrowserType::Firefox;
    }
    else if (classNameStr.find(L"Edge") != std::wstring::npos) {
        return BrowserType::Edge;
    }
    else if (classNameStr.find(L"Teams") != std::wstring::npos) {
        return BrowserType::Teams;
    }
    
    wchar_t windowTitle[512] = { 0 };
    GetWindowTextW(hwnd, windowTitle, 512);
    std::wstring titleStr(windowTitle);
    
    if (titleStr.find(L"Chrome") != std::wstring::npos) {
        return BrowserType::Chrome;
    }
    else if (titleStr.find(L"Firefox") != std::wstring::npos) {
        return BrowserType::Firefox;
    }
    else if (titleStr.find(L"Edge") != std::wstring::npos || 
             titleStr.find(L"Microsoft Edge") != std::wstring::npos) {
        return BrowserType::Edge;
    }
    else if (titleStr.find(L"Teams") != std::wstring::npos ||
        titleStr.find(L"Microsoft Teams") != std::wstring::npos) {
        return BrowserType::Teams;
    }
    
    return BrowserType::Unknown;
}

// Helper function: Get element property (static version for FindDocumentElement)
std::wstring GetElementPropertyStatic(IUIAutomationElement* pElement, PROPERTYID propertyId) {
    if (!pElement) return L"" ;
    
    VARIANT var;
    VariantInit(&var);
    
    HRESULT hr = pElement->GetCurrentPropertyValue(propertyId, &var);
    
    std::wstring result;
    if (SUCCEEDED(hr)) {
        if (var.vt == VT_BSTR && var.bstrVal) {
            result = var.bstrVal;
        }
    }
    
    VariantClear(&var);
    return result;
}

// NEW: Find page content area (Document control)
CComPtr<IUIAutomationElement> FindDocumentElement(IUIAutomation* pAutomation, IUIAutomationElement* pRootElement) {
    if (!pAutomation || !pRootElement) {
        return nullptr;
    }
    
    // Create condition: search for Document control type
    CComPtr<IUIAutomationCondition> pCondition;
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_DocumentControlTypeId;
    
    HRESULT hr = pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
    if (FAILED(hr) || !pCondition) {
        return nullptr;
    }
    
    // Find first Document element (usually the web content area)
    CComPtr<IUIAutomationElement> pDocElement;
    hr = pRootElement->FindFirst(TreeScope_Descendants, pCondition, &pDocElement);
    
    if (SUCCEEDED(hr) && pDocElement) {
        LOG_INFO("Found Document element (web content area)");
        return pDocElement;
    }
    
    // If no Document found, try Pane control (some browsers use Pane)
    varProp.lVal = UIA_PaneControlTypeId;
    pCondition.Release();
    hr = pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
    
    if (SUCCEEDED(hr) && pCondition) {
        CComPtr<IUIAutomationElementArray> pPaneArray;
        hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pPaneArray);
        
        if (SUCCEEDED(hr) && pPaneArray) {
            int length = 0;
            pPaneArray->get_Length(&length);
            
            // Iterate through all Panes, find the most likely web content
            for (int i = 0; i < length && i < 10; i++) {
                CComPtr<IUIAutomationElement> pPane;
                if (SUCCEEDED(pPaneArray->GetElement(i, &pPane)) && pPane) {
                    // Check Name property, exclude toolbar, bookmark bar, etc.
                    std::wstring name = GetElementPropertyStatic(pPane, UIA_NamePropertyId);
                    std::wstring automationId = GetElementPropertyStatic(pPane, UIA_AutomationIdPropertyId);
                    
                    // Skip Panes that are obviously not web content
                    if (name.find(L"Toolbar") == std::wstring::npos &&
                        name.find(L"工具栏") == std::wstring::npos &&
                        name.find(L"Bookmark") == std::wstring::npos &&
                        name.find(L"书签") == std::wstring::npos &&
                        name.find(L"Tab") == std::wstring::npos &&
                        name.find(L"标签") == std::wstring::npos &&
                        automationId.find(L"Toolbar") == std::wstring::npos) {
                        
                        //LOG_INFO_FMT("Found potential content Pane: %s", WStringToString(name).c_str());
                        return pPane;
                    }
                }
            }
        }
    }
    
    return nullptr;
}

bool BrowserContentExtractor::GetActiveBrowserContent(BrowserContentInfo& outInfo) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        LOG_ERROR("Cannot get foreground window!");
        return false;
    }
    
    return GetBrowserContentByHWND(hwnd, outInfo);
}

bool BrowserContentExtractor::GetBrowserContentByHWND(HWND hwnd, BrowserContentInfo& outInfo) {
    if (!pAutomation) {
        LOG_ERROR("UI Automation not initialized!");
        return false;
    }
    
    browserType = DetectBrowserType(hwnd);
    
    wchar_t windowTitle[512] = { 0 };
    GetWindowTextW(hwnd, windowTitle, 512);
    outInfo.title = windowTitle;
    
    LOG_INFO("Detected browser type:");
    switch (browserType) {
        case BrowserType::Chrome:
            LOG_INFO("Chrome");
            break;
        case BrowserType::Edge:
            LOG_INFO("Edge");
            break;
        case BrowserType::Firefox:
            LOG_INFO("Firefox");
            break;
        case BrowserType::Teams:
            LOG_INFO("Teams");
            break;
        default:
            LOG_INFO("Unknown (will try generic parsing)");
            break;
    }
    
    CComPtr<IUIAutomationElement> pRootElement;
    HRESULT hr = pAutomation->ElementFromHandle(hwnd, &pRootElement);
    
    if (FAILED(hr) || !pRootElement) {
        LOG_ERROR("Cannot get UI element from window handle!");
        return false;
    }
    
    // Get URL
    if (browserType == BrowserType::Chrome || browserType == BrowserType::Edge) {
        outInfo.url = GetChromeEdgeURL(pRootElement);
    }
    else if (browserType == BrowserType::Firefox) {
        outInfo.url = GetFirefoxURL(pRootElement);
    }
    
    outInfo.elementCount = 0;
    
    LOG_INFO("Searching for web content area...");
    
    // KEY CHANGE: Only traverse web content area, not entire browser window
    CComPtr<IUIAutomationElement> pContentElement = FindDocumentElement(pAutomation, pRootElement);
    
    if (pContentElement) {
        LOG_INFO("Starting element traversal from content area...");
        TraverseElementTree(pContentElement, outInfo, 0);
    } else {
        LOG_WARN("Could not find content area, parsing entire window...");
        LOG_WARN("(This may include toolbar, tabs, and other UI elements)");
        TraverseElementTree(pRootElement, outInfo, 0);
    }
    
    LOG_INFO_FMT("Parsing complete! Found %d elements", outInfo.elementCount);
    
    return true;
}

void BrowserContentExtractor::TraverseElementTree(IUIAutomationElement* pElement, 
                                                   BrowserContentInfo& info, 
                                                   int depth) {
    if (!pElement || depth > 15) {
        return;
    }
    
    ExtractElementInfo(pElement, info);
    info.elementCount++;
    
    CComPtr<IUIAutomationTreeWalker> pWalker;
    HRESULT hr = pAutomation->get_RawViewWalker(&pWalker);
    
    if (SUCCEEDED(hr) && pWalker) {
        CComPtr<IUIAutomationElement> pChild;
        hr = pWalker->GetFirstChildElement(pElement, &pChild);
        
        while (SUCCEEDED(hr) && pChild) {
            TraverseElementTree(pChild, info, depth + 1);
            
            CComPtr<IUIAutomationElement> pNext;
            hr = pWalker->GetNextSiblingElement(pChild, &pNext);
            pChild = pNext;
        }
    }
}

void BrowserContentExtractor::ExtractElementInfo(IUIAutomationElement* pElement, 
                                                  BrowserContentInfo& info) {
    if (!pElement) return;
    
    CONTROLTYPEID controlType;
    pElement->get_CurrentControlType(&controlType);
    
    std::wstring text = GetElementText(pElement);
    
    // ? 改进：去重并清理文本
    if (!text.empty() && text.length() > 1) {
        // 移除前后空白
        size_t start = text.find_first_not_of(L" \t\n\r");
        size_t end = text.find_last_not_of(L" \t\n\r");
        
        if (start != std::wstring::npos && end != std::wstring::npos) {
            text = text.substr(start, end - start + 1);
            
            // ? 使用集合检查是否已经存在
            if (info.seenTexts.find(text) == info.seenTexts.end()) {
                // 文本未见过，添加到结果和集合中
                info.seenTexts.insert(text);
                
                if (!info.textContent.empty()) {
                    info.textContent += L"\n";
                }
                info.textContent += text;
            }
            // else: 文本已存在，跳过
        }
    }
    
    // 处理链接（已有去重）
    if (controlType == UIA_HyperlinkControlTypeId || 
        controlType == UIA_ButtonControlTypeId) {
        std::wstring value = GetElementProperty(pElement, UIA_ValueValuePropertyId);
        if (!value.empty() && 
            (value.find(L"http://") != std::wstring::npos || 
             value.find(L"https://") != std::wstring::npos)) {
            if (std::find(info.links.begin(), info.links.end(), value) == info.links.end()) {
                info.links.push_back(value);
            }
        }
        else if (!text.empty() && text.find(L"http") != std::wstring::npos) {
            if (std::find(info.links.begin(), info.links.end(), text) == info.links.end()) {
                info.links.push_back(text);
            }
        }
    }
    
    // 处理图片（已有去重）
    if (controlType == UIA_ImageControlTypeId) {
        std::wstring name = GetElementProperty(pElement, UIA_NamePropertyId);
        if (!name.empty()) {
            if (std::find(info.images.begin(), info.images.end(), name) == info.images.end()) {
                info.images.push_back(name);
            }
        }
    }
}

std::wstring BrowserContentExtractor::GetElementText(IUIAutomationElement* pElement) {
    if (!pElement) return L"" ;
    
    std::wstring result;
    
    BSTR name = NULL;
    if (SUCCEEDED(pElement->get_CurrentName(&name)) && name) {
        result = name;
        SysFreeString(name);
        if (!result.empty()) return result;
    }
    
    CComPtr<IUIAutomationValuePattern> pValuePattern;
    HRESULT hr = pElement->GetCurrentPatternAs(UIA_ValuePatternId, 
                                                __uuidof(IUIAutomationValuePattern), 
                                                (void**)&pValuePattern);
    if (SUCCEEDED(hr) && pValuePattern) {
        BSTR value = NULL;
        if (SUCCEEDED(pValuePattern->get_CurrentValue(&value)) && value) {
            result = value;
            SysFreeString(value);
            if (!result.empty()) return result;
        }
    }
    
    CComPtr<IUIAutomationTextPattern> pTextPattern;
    hr = pElement->GetCurrentPatternAs(UIA_TextPatternId, 
                                       __uuidof(IUIAutomationTextPattern), 
                                       (void**)&pTextPattern);
    if (SUCCEEDED(hr) && pTextPattern) {
        CComPtr<IUIAutomationTextRange> pTextRange;
        if (SUCCEEDED(pTextPattern->get_DocumentRange(&pTextRange)) && pTextRange) {
            BSTR text = NULL;
            if (SUCCEEDED(pTextRange->GetText(-1, &text)) && text) {
                result = text;
                SysFreeString(text);
                if (!result.empty()) return result;
            }
        }
    }
    
    return result;
}

std::wstring BrowserContentExtractor::GetElementProperty(IUIAutomationElement* pElement, 
                                                         PROPERTYID propertyId) {
    if (!pElement) return L"" ;
    
    VARIANT var;
    VariantInit(&var);
    
    HRESULT hr = pElement->GetCurrentPropertyValue(propertyId, &var);
    
    std::wstring result;
    if (SUCCEEDED(hr)) {
        if (var.vt == VT_BSTR && var.bstrVal) {
            result = var.bstrVal;
        }
    }
    
    VariantClear(&var);
    return result;
}

std::wstring BrowserContentExtractor::GetChromeEdgeURL(IUIAutomationElement* pRootElement) {
    if (!pRootElement) return L"" ;
    
    CComPtr<IUIAutomationCondition> pCondition;
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_EditControlTypeId;
    
    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
    
    if (pCondition) {
        CComPtr<IUIAutomationElement> pFound;
        HRESULT hr = pRootElement->FindFirst(TreeScope_Descendants, pCondition, &pFound);
        
        if (SUCCEEDED(hr) && pFound) {
            std::wstring automationId = GetElementProperty(pFound, UIA_AutomationIdPropertyId);
            std::wstring name = GetElementProperty(pFound, UIA_NamePropertyId);
            
            if (automationId.find(L"Omnibox") != std::wstring::npos ||
                name.find(L"Address") != std::wstring::npos) {
                
                std::wstring url = GetElementProperty(pFound, UIA_ValueValuePropertyId);
                if (!url.empty()) {
                    return url;
                }
            }
        }
        
        CComPtr<IUIAutomationElementArray> pFoundArray;
        hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pFoundArray);
        
        if (SUCCEEDED(hr) && pFoundArray) {
            int length = 0;
            pFoundArray->get_Length(&length);
            
            for (int i = 0; i < length && i < 10; i++) {
                CComPtr<IUIAutomationElement> pElem;
                if (SUCCEEDED(pFoundArray->GetElement(i, &pElem)) && pElem) {
                    std::wstring value = GetElementProperty(pElem, UIA_ValueValuePropertyId);
                    if (!value.empty() && 
                        (value.find(L"http://") != std::wstring::npos || 
                         value.find(L"https://") != std::wstring::npos)) {
                        return value;
                    }
                }
            }
        }
    }
    
    return L"" ;
}

std::wstring BrowserContentExtractor::GetFirefoxURL(IUIAutomationElement* pRootElement) {
    if (!pRootElement) return L"" ;
    
    CComPtr<IUIAutomationCondition> pCondition;
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_EditControlTypeId;
    
    pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
    
    if (pCondition) {
        CComPtr<IUIAutomationElementArray> pFoundArray;
        HRESULT hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pFoundArray);
        
        if (SUCCEEDED(hr) && pFoundArray) {
            int length = 0;
            pFoundArray->get_Length(&length);
            
            for (int i = 0; i < length && i < 10; i++) {
                CComPtr<IUIAutomationElement> pElem;
                if (SUCCEEDED(pFoundArray->GetElement(i, &pElem)) && pElem) {
                    std::wstring value = GetElementProperty(pElem, UIA_ValueValuePropertyId);
                    if (!value.empty() && 
                        (value.find(L"http://") != std::wstring::npos || 
                         value.find(L"https://") != std::wstring::npos)) {
                        return value;
                    }
                }
            }
        }
    }
    
    return L"" ;
}

std::wstring BrowserContentExtractor::GetControlTypeName(long controlType) {
    switch (controlType) {
        case UIA_ButtonControlTypeId: return L"Button";
        case UIA_EditControlTypeId: return L"Edit";
        case UIA_TextControlTypeId: return L"Text";
        case UIA_HyperlinkControlTypeId: return L"Hyperlink";
        case UIA_ImageControlTypeId: return L"Image";
        case UIA_ListControlTypeId: return L"List";
        case UIA_DocumentControlTypeId: return L"Document";
        default: return L"Unknown";
    }
}

void BrowserContentExtractor::PrintBrowserContent(const BrowserContentInfo& info) {
    LOG_INFO("========== Browser Content Info ==========");
    LOG_INFO_FMT("Title: %s", WStringToString(info.title).c_str());
    LOG_INFO_FMT("URL: %s", info.url.empty() ? "(not found)" : WStringToString(info.url).c_str());
    LOG_INFO_FMT("Element count: %d", info.elementCount);
    
    LOG_INFO_FMT("Links found: %zu", info.links.size());
    if (!info.links.empty()) {
        LOG_INFO("Link list (first 10):");
        size_t maxLinks = (info.links.size() < 10) ? info.links.size() : 10;
        for (size_t i = 0; i < maxLinks; i++) {
            LOG_INFO_FMT("  %zu. %s", i + 1, WStringToString(info.links[i]).c_str());
        }
    }
    
    LOG_INFO_FMT("Images found: %zu", info.images.size());
    if (!info.images.empty()) {
        LOG_INFO("Image list (first 10):");
        size_t maxImages = (info.images.size() < 10) ? info.images.size() : 10;
        for (size_t i = 0; i < maxImages; i++) {
            LOG_INFO_FMT("  %zu. %s", i + 1, WStringToString(info.images[i]).c_str());
        }
    }
    
    LOG_INFO("Page text content (first 2000 chars):");
    LOG_INFO("-----------------------------------");
    if (info.textContent.length() > 2000) {
        LOG_INFO_FMT("%s...", WStringToString(info.textContent.substr(0, 2000)).c_str());
    }
    else {
        LOG_INFO(WStringToString(info.textContent).c_str());
    }
    LOG_INFO("-----------------------------------");
    LOG_INFO("========== End of Info ==========");
}
