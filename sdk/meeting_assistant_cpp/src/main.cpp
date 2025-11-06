#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <Windows.h>
#include <winrt/base.h>

#include "MeetingStateMachine.h"
#include "NotificationService.h"
#include "PayAttentionBridge.h"
#include "MicrophoneMonitorDLL.h"

using namespace MeetingAssistant;

// Global variables
std::atomic<bool> g_running(true);
MicrophoneMonitorHandle g_monitor = nullptr;
std::unique_ptr<MeetingStateMachine> g_stateMachine;
std::unique_ptr<NotificationService> g_notificationService;

// Signal handler for Ctrl+C
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        g_running = false;
        std::cout << "\n[MeetingAssistant] Shutting down...\n";
        return TRUE;
    }
    return FALSE;
}

// Callback functions
void OnUserClickedStart() {
    std::cout << "\n¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[\n";
    std::cout << "¨U  USER ACTION: Clicked 'Start'                              ¨U\n";
    std::cout << "¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a\n";

    if (g_stateMachine->GetCurrentMeeting().has_value()) {
        // Call Pay Attention SDK
        PayAttentionBridge::StartMeetingTranscription(g_stateMachine->GetCurrentMeeting().value());

        // Update state
        g_stateMachine->OnUserConfirmed();
    } else {
        std::cout << "[WARNING] No current meeting to start transcription for\n";
    }

    std::cout << "\n";
}

void OnUserClickedDismiss() {
    std::cout << "\n¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[\n";
    std::cout << "¨U  USER ACTION: Clicked 'Dismiss'                            ¨U\n";
    std::cout << "¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a\n";

    // Update state
    g_stateMachine->OnUserDismissed();

    std::cout << "\n";
}

void OnUserClickedStartSummarize() {
    std::cout << "\n¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[\n";
    std::cout << "¨U  USER ACTION: Clicked 'Start summarize'                     ¨U\n";
    std::cout << "¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a\n";

    // Call Pay Attention SDK
    PayAttentionBridge::StartMeetingSummarization();
    std::cout << "Summarize finished...\n";

    std::cout << "\n";
}

void OnUserClickedCancelSummarize() {
    std::cout << "\n¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[\n";
    std::cout << "¨U  USER ACTION: Clicked 'Cancel summarize'                     ¨U\n";
    std::cout << "¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a\n";

    std::cout << "\n";
}

void RunServiceLoop() {
    while (g_running) {
        try {
            // Check for meeting apps
            bool meetingDetected = MicrophoneMonitor_IsMeetingAppUsingMicrophone(g_monitor) != 0;
            std::optional<MeetingInfo> meetingInfo;

            if (meetingDetected) {
                char buffer[256] = { 0 };
                int length = MicrophoneMonitor_GetMeetingAppName(g_monitor, buffer, sizeof(buffer));
                unsigned long pid = MicrophoneMonitor_GetMeetingAppPID(g_monitor);

                if (length > 0) {
                    MeetingInfo info;
                    info.appName = buffer;
                    info.processId = pid;
                    info.detectedAt = std::chrono::system_clock::now();
                    meetingInfo = info;
                }
            }

            // Update state machine
            bool stateChanged = g_stateMachine->Update(meetingDetected, meetingInfo);

            // Handle state transitions
            if (stateChanged) {
                if (g_stateMachine->GetCurrentState() == MeetingState::Detected && meetingInfo.has_value()) {
                    // NEW MEETING DETECTED - Show notification
                    std::cout << "[MeetingAssistant] Meeting detected: " << meetingInfo->ToString() << "\n";
                    g_notificationService->ShowMeetingDetectedNotification(meetingInfo->appName);
                } else if (g_stateMachine->GetCurrentState() == MeetingState::Idle) {
                    if (g_stateMachine->GetLastState() == MeetingState::PayingAttention) {
                        PayAttentionBridge::StopRecord();
                        g_notificationService->ShowMeetingSummaryNotification();
                        std::cout << "[MeetingAssistant] [State] Detected ¡ú Idle, try to stop the record...)\n";
                    } else {
                        std::cout << "[MeetingAssistant] No meeting detected (state: Idle)\n";
                    }
                }
            }

            // Wait before next check
            std::this_thread::sleep_for(std::chrono::seconds(2));  // Poll every 2 seconds
        } catch (const std::exception& ex) {
            std::cout << "[ERROR] Loop iteration failed: " << ex.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait longer on error
        }
    }
}

int main() {
    // Initialize Windows Runtime
    winrt::init_apartment();

    std::cout << "================================================================================\n";
    std::cout << "Proactive Pay Attention\n";
    std::cout << "================================================================================\n";
    std::cout << "\n";
    std::cout << "This service monitors meeting apps and offers to start\n";
    std::cout << "Pay Attention feature when a meeting is detected.\n";
    std::cout << "\n";
    std::cout << "Supported apps: Teams, Zoom, Webex, Google Meet, and more...\n";
    std::cout << "\n";
    std::cout << "Press Ctrl+C to exit\n";
    std::cout << "================================================================================\n";
    std::cout << "\n";

    // Set up Ctrl+C handler
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[ERROR] Failed to set console control handler\n";
        return 1;
    }

    try {
        // Initialize components
        std::cout << "[MeetingAssistant] Initializing...\n";

        g_monitor = MicrophoneMonitor_Create();
        if (g_monitor == nullptr) {
            throw std::runtime_error("Failed to create MicrophoneMonitor instance");
        }
        std::cout << "[?] MicrophoneMonitor initialized\n";

        g_stateMachine = std::make_unique<MeetingStateMachine>();
        std::cout << "[?] State machine initialized\n";

        g_notificationService = std::make_unique<NotificationService>(
            OnUserClickedStart,
            OnUserClickedDismiss,
            OnUserClickedStartSummarize,
            OnUserClickedCancelSummarize
        );
        std::cout << "[?] Notification service initialized\n";

        PayAttentionBridge::Initialize();
        std::cout << "[?] PayAttentionBridge SDK initialized.\n";

        std::cout << "\n";
        std::cout << "[MeetingAssistant] Running...\n";
        std::cout << "\n";

        // Main service loop
        RunServiceLoop();

    } catch (const std::exception& ex) {
        std::cout << "[ERROR] " << ex.what() << "\n";
        return 1;
    }

    // Cleanup
    std::cout << "\n";
    std::cout << "[MeetingAssistant] Cleaning up...\n";
    
    if (g_monitor) {
        MicrophoneMonitor_Destroy(g_monitor);
    }
    
    g_notificationService.reset();
    g_stateMachine.reset();
    
    std::cout << "[MeetingAssistant] Goodbye!\n";

    winrt::uninit_apartment();
    return 0;
}
