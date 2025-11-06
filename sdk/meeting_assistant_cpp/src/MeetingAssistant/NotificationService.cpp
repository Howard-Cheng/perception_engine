#include "NotificationService.h"
#include <iostream>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

namespace MeetingAssistant {

NotificationService::NotificationService(
    NotificationCallback onStartClicked,
    NotificationCallback onDismissClicked,
    NotificationCallback onStartSummaryClicked,
    NotificationCallback onCancelSummaryClicked)
    : onStartClicked_(onStartClicked)
    , onDismissClicked_(onDismissClicked)
    , onStartSummaryClicked_(onStartSummaryClicked)
    , onCancelSummaryClicked_(onCancelSummaryClicked) {
}

NotificationService::~NotificationService() {
}

void NotificationService::ShowMeetingDetectedNotification(const std::string& appName) {
    try {
        std::wstring title = L"Meeting Detected!";
        std::wstring message = L"Want to pay attention to your " + 
            std::wstring(appName.begin(), appName.end()) + L" meeting?";
        
        ShowToastInternal(title, message, L"Start", L"start", L"Dismiss", L"dismiss");
        
        std::cout << "[Notification] Shown for " << appName << "\n";
    } catch (const std::exception& ex) {
        std::cout << "[Notification] Failed to show: " << ex.what() << "\n";
    }
}

void NotificationService::ShowMeetingSummaryNotification() {
    try {
        std::wstring title = L"Meeting ended Detected!";
        std::wstring message = L"Want to summarize your last meeting?";
        
        ShowToastInternal(title, message, L"Start", L"startsummarize", L"Dismiss", L"cancelsummarize");
        
        std::cout << "[Notification] Shown for summarize\n";
    } catch (const std::exception& ex) {
        std::cout << "[Notification] Failed to show: " << ex.what() << "\n";
    }
}

void NotificationService::ShowToastInternal(
    const std::wstring& title, 
    const std::wstring& message,
    const std::wstring& button1Text, 
    const std::wstring& button1Action,
    const std::wstring& button2Text, 
    const std::wstring& button2Action) {
    
    // Create toast XML
    std::wstring toastXml = LR"(
<toast>
    <visual>
        <binding template="ToastGeneric">
            <text>)" + title + LR"(</text>
            <text>)" + message + LR"(</text>
        </binding>
    </visual>
    <actions>
        <action content=")" + button1Text + LR"(" arguments="action=)" + button1Action + LR"(" />
        <action content=")" + button2Text + LR"(" arguments="action=)" + button2Action + LR"(" />
    </actions>
</toast>
)";

    try {
        // Parse XML
        XmlDocument doc;
        doc.LoadXml(toastXml);

        // Create notification
        ToastNotification toast(doc);
        
        // Set up activated handler
        toast.Activated([this](ToastNotification const&, winrt::Windows::Foundation::IInspectable const& args) {
            // Get arguments from activation
            auto activatedArgs = args.try_as<ToastActivatedEventArgs>();
            if (activatedArgs) {
                // Convert winrt::hstring to std::wstring
                std::wstring argsStr = activatedArgs.Arguments().c_str();
                OnNotificationActivated(argsStr);
            }
        });

        // Show notification
        ToastNotifier notifier = ToastNotificationManager::CreateToastNotifier(L"MeetingAssistant");
        notifier.Show(toast);
        
    } catch (const hresult_error& ex) {
        std::wcout << L"[Notification] Error: " << ex.message().c_str() << L"\n";
    }
}

void NotificationService::OnNotificationActivated(const std::wstring& arguments) {
    std::wcout << L"[Notification] User clicked with arguments: " << arguments << L"\n";

    // Parse arguments (simple parsing for "action=value" format)
    if (arguments.find(L"action=start") != std::wstring::npos) {
        if (onStartClicked_) {
            onStartClicked_();
        }
    } else if (arguments.find(L"action=dismiss") != std::wstring::npos) {
        if (onDismissClicked_) {
            onDismissClicked_();
        }
    } else if (arguments.find(L"action=startsummarize") != std::wstring::npos) {
        if (onStartSummaryClicked_) {
            onStartSummaryClicked_();
        }
    } else if (arguments.find(L"action=cancelsummarize") != std::wstring::npos) {
        if (onCancelSummaryClicked_) {
            onCancelSummaryClicked_();
        }
    }
}

} // namespace MeetingAssistant
