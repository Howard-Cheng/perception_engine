#pragma once
#include <string>
#include <functional>
#include <Windows.h>

namespace MeetingAssistant {

// Callback types for notification actions
using NotificationCallback = std::function<void()>;

class NotificationService {
public:
    NotificationService(
        NotificationCallback onStartClicked,
        NotificationCallback onDismissClicked,
        NotificationCallback onStartSummaryClicked,
        NotificationCallback onCancelSummaryClicked);
    
    ~NotificationService();

    // Show Windows toast notification for meeting detection
    void ShowMeetingDetectedNotification(const std::string& appName);

    // Show Windows toast notification for meeting summary
    void ShowMeetingSummaryNotification();

    // Process notification activation (called from notification callback)
    void OnNotificationActivated(const std::wstring& arguments);

private:
    NotificationCallback onStartClicked_;
    NotificationCallback onDismissClicked_;
    NotificationCallback onStartSummaryClicked_;
    NotificationCallback onCancelSummaryClicked_;

    // Helper to show toast using Windows Runtime API
    void ShowToastInternal(const std::wstring& title, const std::wstring& message, 
                           const std::wstring& button1Text, const std::wstring& button1Action,
                           const std::wstring& button2Text, const std::wstring& button2Action);
};

} // namespace MeetingAssistant
