#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "platform/BrowserContentExtractor.h"
#include "pe_base/logger.h"  // Correct path - same directory as BrowserContentExtractor.cpp
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
        PE_ERROR_THIS("Initialize UI Automation failed! HRESULT" << hr)
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
        //PE_DEBUG("Found Document element (web content area)");
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
                        name.find(L"Bookmark") == std::wstring::npos &&
                        name.find(L"Tab") == std::wstring::npos &&
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
        PE_ERROR("Cannot get foreground window!");
        return false;
    }
    
    return GetBrowserContentByHWND(hwnd, outInfo);
}

bool BrowserContentExtractor::GetBrowserContentByHWND(HWND hwnd, BrowserContentInfo& outInfo) {
    if (!pAutomation) {
        PE_ERROR("UI Automation not initialized!");
        return false;
    }
    
    // Validate window handle before processing
    if (!hwnd || !IsWindow(hwnd)) {
        PE_ERROR("Invalid window handle!");
        return false;
    }
    
    // Check if window is still visible
    if (!IsWindowVisible(hwnd)) {
        PE_WARN("Window is not visible, skipping content extraction");
        return false;
    }
    
    browserType = DetectBrowserType(hwnd);
    
    wchar_t windowTitle[512] = { 0 };
    GetWindowTextW(hwnd, windowTitle, 512);
    outInfo.title = windowTitle;
    
    //PE_DEBUG("Detected browser type:");
    switch (browserType) {
        case BrowserType::Chrome:
            PE_DEBUG("Chrome");
            break;
        case BrowserType::Edge:
            PE_DEBUG("Edge");
            break;
        case BrowserType::Firefox:
            PE_DEBUG("Firefox");
            break;
        case BrowserType::Teams:
            PE_DEBUG("Teams");
            break;
        default:
            //PE_DEBUG("Unknown (will try generic parsing)");
            break;
    }
    
    CComPtr<IUIAutomationElement> pRootElement;
    HRESULT hr = pAutomation->ElementFromHandle(hwnd, &pRootElement);
    
    if (FAILED(hr) || !pRootElement) {
        PE_ERROR("Cannot get UI element from window handle!");
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

    //PE_DEBUG("Searching for web content area...");

    // KEY CHANGE: Only traverse web content area, not entire browser window
    CComPtr<IUIAutomationElement> pContentElement = FindDocumentElement(pAutomation, pRootElement);

    if (pContentElement) {
        //PE_DEBUG("Starting element traversal from content area...");
        TraverseElementTree(pContentElement, outInfo, 0);
    } else {
        PE_WARN("Could not find content area, parsing entire window...");
        TraverseElementTree(pRootElement, outInfo, 0);
    }

    //LOG_DEBUG_FMT("Parsing complete! Found %d elements", outInfo.elementCount);
    
    return true;
}

void BrowserContentExtractor::TraverseElementTree(IUIAutomationElement* pElement, 
                                                   BrowserContentInfo& info, 
                                                   int depth) {
    // Strict depth and validity checks
    if (!pElement || depth > 15) {
        return;
    }
    
    // Exception protection for element operations
    try {
        ExtractElementInfo(pElement, info);
        info.elementCount++;
        
        CComPtr<IUIAutomationTreeWalker> pWalker;
        HRESULT hr = pAutomation->get_RawViewWalker(&pWalker);
        
        if (SUCCEEDED(hr) && pWalker) {
            CComPtr<IUIAutomationElement> pChild;
            hr = pWalker->GetFirstChildElement(pElement, &pChild);
            
            // Child count limit to prevent infinite loops
            int childCount = 0;
            while (SUCCEEDED(hr) && pChild && childCount < 1000) {
                TraverseElementTree(pChild, info, depth + 1);
                
                CComPtr<IUIAutomationElement> pNext;
                hr = pWalker->GetNextSiblingElement(pChild, &pNext);
                pChild = pNext;
                childCount++;
            }
        }
    }
    catch (const _com_error&) {
        PE_WARN("COM error during element traversal - element may have been destroyed");
        return;
    }
    catch (const std::exception&) {
        PE_WARN("Exception during element traversal - continuing with partial results");
        return;
    }
    catch (...) {
        PE_WARN("Unknown exception during element traversal");
        return;
    }
}

void BrowserContentExtractor::ExtractElementInfo(IUIAutomationElement* pElement, 
                                                  BrowserContentInfo& info) {
    if (!pElement) return;
    
    CONTROLTYPEID controlType;
    pElement->get_CurrentControlType(&controlType);
    
    std::wstring text = GetElementText(pElement);
    
    if (!text.empty() && text.length() > 1) {
        size_t start = text.find_first_not_of(L" \t\n\r");
        size_t end = text.find_last_not_of(L" \t\n\r");
        
        if (start != std::wstring::npos && end != std::wstring::npos) {
            text = text.substr(start, end - start + 1);
            
            if (info.seenTexts.find(text) == info.seenTexts.end()) {
                info.seenTexts.insert(text);
                
                if (!info.textContent.empty()) {
                    info.textContent += L"\n";
                }
                info.textContent += text;
            }
        }
    }
    
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
    
    if (controlType == UIA_ImageControlTypeId) {
        std::wstring name = GetElementProperty(pElement, UIA_NamePropertyId);
        if (!name.empty()) {
            if (std::find(info.images.begin(), info.images.end(), name) == info.images.end()) {
                info.images.push_back(name);
            }
        }
    }
}

// 辅助函数：去除首尾空白
std::wstring Trim(const std::wstring& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) start++;

    auto end = str.end();
    do { end--; } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::wstring(start, end + 1);
}

std::wstring BrowserContentExtractor::GetElementText(IUIAutomationElement* pElement) {
    if (!pElement) return L"" ;
    
    std::wstring result;
    
    CComPtr<IUIAutomationTextPattern> pTextPattern;
    HRESULT hr = pElement->GetCurrentPatternAs(UIA_TextPatternId,
                                       __uuidof(IUIAutomationTextPattern), 
                                       (void**)&pTextPattern);
    if (SUCCEEDED(hr) && pTextPattern) {
        CComPtr<IUIAutomationTextRange> pTextRange;
        if (SUCCEEDED(pTextPattern->get_DocumentRange(&pTextRange)) && pTextRange) {
            BSTR text = NULL;
            if (SUCCEEDED(pTextRange->GetText(-1, &text)) && text) {
                result = text;
                SysFreeString(text);
                if (!Trim(result).empty()) 
                    return result;
            }
        }
    }

    CComPtr<IUIAutomationValuePattern> pValuePattern;
    hr = pElement->GetCurrentPatternAs(UIA_ValuePatternId,
        __uuidof(IUIAutomationValuePattern),
        (void**)&pValuePattern);
    if (SUCCEEDED(hr) && pValuePattern) {
        BSTR value = NULL;
        if (SUCCEEDED(pValuePattern->get_CurrentValue(&value)) && value) {
            result = value;
            SysFreeString(value);
            if (!Trim(result).empty()) 
                return result;
        }
    }

    CComPtr<IUIAutomationLegacyIAccessiblePattern> pLegacyPattern;
    pElement->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, __uuidof(IUIAutomationLegacyIAccessiblePattern), (void**)&pLegacyPattern);

    if (pLegacyPattern) {
        BSTR value = NULL;
        if (SUCCEEDED(pLegacyPattern->get_CurrentValue(&value)) && value) {
            result = value;
            SysFreeString(value);
            if (!Trim(result).empty()) 
                return result;
        }
    }

    BSTR name = NULL;
    if (SUCCEEDED(pElement->get_CurrentName(&name)) && name) {
        result = name;
        SysFreeString(name);
        if (!Trim(result).empty()) 
            return result;
    }

    if (pLegacyPattern) {
        // 有些怪异程序的 CurrentName 是空的，但 accName 有值
        BSTR legName = NULL;
        if (SUCCEEDED(pLegacyPattern->get_CurrentName(&legName)) && legName) {
            result = legName;
            SysFreeString(legName);
            if (!Trim(result).empty()) 
                return result;
        }

        // Description 优先级很低，通常是 HelpText，不到万不得已不用
        BSTR desc = NULL;
        if (SUCCEEDED(pLegacyPattern->get_CurrentDescription(&desc)) && desc) {
            result = desc;
            SysFreeString(desc);
            if (!Trim(result).empty()) 
                return result;
        }
    }

    UIA_HWND nativeHwnd = NULL;
    if (SUCCEEDED(pElement->get_CurrentNativeWindowHandle(&nativeHwnd)) && nativeHwnd) {
        HWND hwnd = (HWND)nativeHwnd;
        if (IsWindow(hwnd)) {
            // 简单的长度检查，防止过长
            int len = ::GetWindowTextLengthW(hwnd);
            if (len > 0 && len < 4096) { // 限制长度防止 buffer overflow 风险
                std::vector<wchar_t> buffer(len + 1);
                if (::GetWindowTextW(hwnd, buffer.data(), len + 1)) {
                    result = buffer.data();
                    if (!Trim(result).empty()) 
                        return result;
                }
            }
        }
    }
    
    return result;
}

//std::wstring BrowserContentExtractor::GetElementText(IUIAutomationElement* pElement) {
//    if (!pElement) return L"";
//
//    std::wstring result;
//
//    BSTR name = NULL;
//    if (SUCCEEDED(pElement->get_CurrentName(&name)) && name) {
//        result = name;
//        SysFreeString(name);
//        if (!result.empty()) 
//            return result;
//    }
//
//    CComPtr<IUIAutomationValuePattern> pValuePattern;
//    HRESULT hr = pElement->GetCurrentPatternAs(UIA_ValuePatternId,
//        __uuidof(IUIAutomationValuePattern),
//        (void**)&pValuePattern);
//    if (SUCCEEDED(hr) && pValuePattern) {
//        BSTR value = NULL;
//        if (SUCCEEDED(pValuePattern->get_CurrentValue(&value)) && value) {
//            result = value;
//            SysFreeString(value);
//            if (!result.empty()) 
//                return result;
//        }
//    }
//
//    CComPtr<IUIAutomationTextPattern> pTextPattern;
//    hr = pElement->GetCurrentPatternAs(UIA_TextPatternId,
//        __uuidof(IUIAutomationTextPattern),
//        (void**)&pTextPattern);
//    if (SUCCEEDED(hr) && pTextPattern) {
//        CComPtr<IUIAutomationTextRange> pTextRange;
//        if (SUCCEEDED(pTextPattern->get_DocumentRange(&pTextRange)) && pTextRange) {
//            BSTR text = NULL;
//            if (SUCCEEDED(pTextRange->GetText(-1, &text)) && text) {
//                result = text;
//                SysFreeString(text);
//                if (!result.empty()) 
//                    return result;
//            }
//        }
//    }
//
//    return result;
//}

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
    if (!pRootElement) {
        PE_ERROR("GetChromeEdgeURL: pRootElement is null");
        return L"";
    }
    
    // Strategy 1: Look for Edit controls with Omnibox/Address bar pattern
    CComPtr<IUIAutomationCondition> pCondition;
    VARIANT varProp;
    varProp.vt = VT_I4;
    varProp.lVal = UIA_EditControlTypeId;
    
    HRESULT hr = pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, varProp, &pCondition);
    
    if (FAILED(hr) || !pCondition) {
        PE_ERROR_THIS("GetChromeEdgeURL: Failed to create property condition, HRESULT=" << std::hex << hr);
        return L"";
    }
    
    
    if (pCondition) {
        // First try: FindFirst with specific AutomationId/Name pattern
        CComPtr<IUIAutomationElement> pFound;
        hr = pRootElement->FindFirst(TreeScope_Descendants, pCondition, &pFound);
        
        if (SUCCEEDED(hr) && pFound) {
            std::wstring automationId = GetElementProperty(pFound, UIA_AutomationIdPropertyId);
            std::wstring name = GetElementProperty(pFound, UIA_NamePropertyId);
            
            if (automationId.find(L"Omnibox") != std::wstring::npos ||
                automationId.find(L"addressbar") != std::wstring::npos ||
                name.find(L"Address") != std::wstring::npos) {  // Chinese "Address"
                
                std::wstring url = GetElementProperty(pFound, UIA_ValueValuePropertyId);
                if (!url.empty()) {
                    PE_INFO_THIS("GetChromeEdgeURL: Found URL via specific Edit: " << WStringToString(url).c_str());
                    return url;
                }
            }
        } else {
            PE_WARN_THIS("GetChromeEdgeURL: FindFirst failed, HRESULT=" << std::hex << hr);
        }
        
        // Second try: FindAll and iterate through all Edit controls
        CComPtr<IUIAutomationElementArray> pFoundArray;
        hr = pRootElement->FindAll(TreeScope_Descendants, pCondition, &pFoundArray);
        
        if (SUCCEEDED(hr) && pFoundArray) {
            int length = 0;
            pFoundArray->get_Length(&length);
            
            for (int i = 0; i < length && i < 20; i++) {  // Increased from 10 to 20
                CComPtr<IUIAutomationElement> pElem;
                if (SUCCEEDED(pFoundArray->GetElement(i, &pElem)) && pElem) {
                    std::wstring automationId = GetElementProperty(pElem, UIA_AutomationIdPropertyId);
                    std::wstring name = GetElementProperty(pElem, UIA_NamePropertyId);
                    std::wstring value = GetElementProperty(pElem, UIA_ValueValuePropertyId);
                    
                    if (!value.empty() && 
                        (value.find(L"http://") != std::wstring::npos || 
                         value.find(L"https://") != std::wstring::npos)) {
                        PE_INFO_THIS("GetChromeEdgeURL: Found URL in Edit[" << i << "]: " << WStringToString(value).c_str());
                        return value;
                    }
                }
            }
            
            PE_WARN("GetChromeEdgeURL: No Edit control with URL found");
        } else {
            PE_ERROR_THIS("GetChromeEdgeURL: FindAll failed, HRESULT=" << std::hex << hr);
        }
    }
    
    // Strategy 2: Try using ValuePattern on all descendants
    CComPtr<IUIAutomationCondition> pTrueCondition;
    hr = pAutomation->CreateTrueCondition(&pTrueCondition);
    
    if (SUCCEEDED(hr) && pTrueCondition) {
        CComPtr<IUIAutomationElementArray> pAllElements;
        hr = pRootElement->FindAll(TreeScope_Descendants, pTrueCondition, &pAllElements);
        
        if (SUCCEEDED(hr) && pAllElements) {
            int length = 0;
            pAllElements->get_Length(&length);
            PE_DEBUG_THIS("GetChromeEdgeURL: Found " << length << " total elements, checking for URL...");
            
            for (int i = 0; i < length && i < 100; i++) {
                CComPtr<IUIAutomationElement> pElem;
                if (SUCCEEDED(pAllElements->GetElement(i, &pElem)) && pElem) {
                    // Check if element supports ValuePattern
                    CComPtr<IUIAutomationValuePattern> pValuePattern;
                    hr = pElem->GetCurrentPatternAs(UIA_ValuePatternId, 
                                                     __uuidof(IUIAutomationValuePattern), 
                                                     (void**)&pValuePattern);
                    if (SUCCEEDED(hr) && pValuePattern) {
                        BSTR value = NULL;
                        if (SUCCEEDED(pValuePattern->get_CurrentValue(&value)) && value) {
                            std::wstring valueStr(value);
                            SysFreeString(value);
                            
                            if ((valueStr.find(L"http://") != std::wstring::npos || 
                                 valueStr.find(L"https://") != std::wstring::npos) &&
                                valueStr.length() > 10) {
                                PE_INFO_THIS("GetChromeEdgeURL: Found URL via ValuePattern: " << WStringToString(valueStr).c_str());
                                return valueStr;
                            }
                        }
                    }
                }
            }
        }
    }
    
    PE_WARN("GetChromeEdgeURL: URL not found, returning empty string");
    return L"";
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
    PE_INFO("========== Browser Content Info ==========");
    PE_INFO_THIS("Title:" << WStringToString(info.title).c_str())
    PE_INFO_THIS("URL:" << (info.url.empty() ? "(not found)" : WStringToString(info.url).c_str()))
    PE_INFO_THIS("Element count: %d" << info.elementCount);
    
    PE_INFO_THIS("Links found:" << info.links.size())
    if (!info.links.empty()) {
        PE_INFO("Link list (first 10):");
        size_t maxLinks = (info.links.size() < 10) ? info.links.size() : 10;
        for (size_t i = 0; i < maxLinks; i++) {
            PE_INFO_THIS(i + 1 << "," << WStringToString(info.links[i]).c_str())
        }
    }
    
    PE_INFO_THIS("Images found:" << info.images.size())
    if (!info.images.empty()) {
        PE_INFO("Image list (first 10):");
        size_t maxImages = (info.images.size() < 10) ? info.images.size() : 10;
        for (size_t i = 0; i < maxImages; i++) {
            PE_INFO_THIS(i + 1 << "," << WStringToString(info.links[i]).c_str())
        }
    }
    
    PE_INFO("Page text content (first 2000 chars):");
    PE_INFO("-----------------------------------");
    if (info.textContent.length() > 2000) {
        PE_INFO_THIS(WStringToString(info.textContent.substr(0, 2000)).c_str() << "...")
    }
    else {
        PE_INFO(WStringToString(info.textContent).c_str());
    }
    PE_INFO("-----------------------------------");
    PE_INFO("========== End of Info ==========");
}
