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
#include <set>  // ← 新增：用于去重

// 浏览器内容信息结构
struct BrowserContentInfo {
    std::wstring url;              // URL地址
    std::wstring title;            // 页面标题
    std::wstring textContent;      // 文本内容
    std::wstring htmlContent;      // HTML内容
    int elementCount;              // 元素数量
    std::vector<std::wstring> links; // 链接列表
    std::vector<std::wstring> images; // 图片列表
    std::set<std::wstring> seenTexts;  // ← 新增：已见过的文本（用于去重）
};

// 浏览器类型枚举
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
    
    // 初始化UI Automation
    bool InitializeUIAutomation();
    
    // 获取活动窗口的浏览器类型
    BrowserType DetectBrowserType(HWND hwnd);
    
    // 递归遍历UI元素树
    void TraverseElementTree(IUIAutomationElement* pElement, BrowserContentInfo& info, int depth = 0);
    
    // 提取元素信息
    void ExtractElementInfo(IUIAutomationElement* pElement, BrowserContentInfo& info);
    
    // 获取元素的文本内容
    std::wstring GetElementText(IUIAutomationElement* pElement);
    
    // 获取元素的属性
    std::wstring GetElementProperty(IUIAutomationElement* pElement, PROPERTYID propertyId);
    
    // 获取Chrome/Edge的地址栏URL
    std::wstring GetChromeEdgeURL(IUIAutomationElement* pRootElement);
    
    // 获取Firefox的地址栏URL
    std::wstring GetFirefoxURL(IUIAutomationElement* pRootElement);
    
    // 查找元素（通过AutomationId或Name）
    CComPtr<IUIAutomationElement> FindElement(IUIAutomationElement* pParent, 
                                               const std::wstring& automationId, 
                                               const std::wstring& name);
    
    // 获取控件类型名称
    std::wstring GetControlTypeName(long controlType);

public:
    BrowserContentExtractor();
    ~BrowserContentExtractor();
    
    // 获取当前活跃浏览器的内容
    bool GetActiveBrowserContent(BrowserContentInfo& outInfo);
    
    // 获取指定窗口句柄的浏览器内容
    bool GetBrowserContentByHWND(HWND hwnd, BrowserContentInfo& outInfo);
    
    // 打印浏览器内容信息
    void PrintBrowserContent(const BrowserContentInfo& info);
};
