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
#include "NamedPipeServer.h"

using namespace MeetingAssistant;

// Global variables
std::atomic<bool> g_running(true);
MicrophoneMonitorHandle g_monitor = nullptr;
std::unique_ptr<MeetingStateMachine> g_stateMachine;
std::unique_ptr<NotificationService> g_notificationService;
std::unique_ptr<NamedPipeServer> g_pipeServer;

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
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  USER ACTION: Clicked 'Start'                              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

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
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  USER ACTION: Clicked 'Dismiss'                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    // Update state
    g_stateMachine->OnUserDismissed();

    std::cout << "\n";
}

void OnUserClickedStartSummarize() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  USER ACTION: Clicked 'Start summarize'                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    // Call Pay Attention SDK
    PayAttentionBridge::StartMeetingSummarization();
    std::cout << "Summarize finished...\n";

    std::cout << "\n";
}

void OnUserClickedCancelSummarize() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  USER ACTION: Clicked 'Cancel summarize'                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

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

                    // Notify pipe clients if connected
                    if (g_pipeServer && g_pipeServer->IsRunning()) {
                        UserResponse response = g_pipeServer->NotifyMeetingEvent(
                            MeetingEvent::Started,
                            meetingInfo->appName,
                            meetingInfo->processId
                        );

                        if (response == UserResponse::Accept) {
                            std::cout << "[MeetingAssistant] Pipe client accepted - starting transcription\n";
                            //PayAttentionBridge::StartMeetingTranscription(meetingInfo.value());
                            g_stateMachine->OnUserConfirmed();
                        } else if (response == UserResponse::Decline) {
                            std::cout << "[MeetingAssistant] Pipe client declined\n";
                            g_stateMachine->OnUserDismissed();
                        } else {
                            std::cout << "[MeetingAssistant] No pipe client response\n";
                            // No pipe client response, show notification as fallback
                            //g_notificationService->ShowMeetingDetectedNotification(meetingInfo->appName);
                        }
                    } else {
                        std::cout << "[MeetingAssistant] No pipe server, use notification\n";
                        // No pipe server, use notification
                        //g_notificationService->ShowMeetingDetectedNotification(meetingInfo->appName);
                    }

                } else if (g_stateMachine->GetCurrentState() == MeetingState::Idle) {
                    if (g_stateMachine->GetLastState() == MeetingState::PayingAttention) {
                        PayAttentionBridge::StopRecord();

                        // Get last meeting info
                        auto lastMeeting = g_stateMachine->GetCurrentMeeting();

                        // Notify pipe clients if connected
                        if (g_pipeServer && g_pipeServer->IsRunning() && lastMeeting.has_value()) {
                            UserResponse response = g_pipeServer->NotifyMeetingEvent(
                                MeetingEvent::Ended,
                                lastMeeting->appName,
                                lastMeeting->processId
                            );

                            if (response == UserResponse::Accept) {
                                std::cout << "[MeetingAssistant] Pipe client accepted - starting summarization\n";
                                //PayAttentionBridge::StartMeetingSummarization();
                            } else if (response == UserResponse::Decline) {
                                std::cout << "[MeetingAssistant] Pipe client declined summarization\n";
                            } else {
                                std::cout << "[MeetingAssistant] No pipe client response, show notification\n";
                                // No pipe client response, show notification as fallback
                                //g_notificationService->ShowMeetingSummaryNotification();
                            }
                        } else {
                            // No pipe server, use notification
                            std::cout << "[MeetingAssistant] No pipe server, use notification\n";
                            //g_notificationService->ShowMeetingSummaryNotification();
                        }
                        
                        std::cout << "[MeetingAssistant] [State] PayingAttention → Idle, stopped recording\n";
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

    // Set Ctrl+C handler
    if (!SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE)) {
        std::cerr << "[ERROR] Failed to set console control handler\n";
        winrt::uninit_apartment();
        return 1;
    }

    try {
        // Initialize components
        std::cout << "[MeetingAssistant] Initializing...\n";

        g_monitor = MicrophoneMonitor_Create();
        if (g_monitor == nullptr) {
            throw std::runtime_error("Failed to create MicrophoneMonitor instance");
        }
        std::cout << "[✓] MicrophoneMonitor initialized\n";

        g_stateMachine = std::make_unique<MeetingStateMachine>();
        std::cout << "[✓] State machine initialized\n";

        g_notificationService = std::make_unique<NotificationService>(
            OnUserClickedStart,
            OnUserClickedDismiss,
            OnUserClickedStartSummarize,
            OnUserClickedCancelSummarize
        );
        std::cout << "[✓] Notification service initialized\n";

        // Initialize named pipe server
        g_pipeServer = std::make_unique<NamedPipeServer>();
        if (g_pipeServer->Start()) {
            std::cout << "[✓] Named pipe server started\n";
        } else {
            std::cout << "[WARNING] Failed to start named pipe server\n";
        }

        PayAttentionBridge::Initialize();
        std::cout << "[✓] PayAttentionBridge SDK initialized\n";

        std::cout << "\n";
        std::cout << "[MeetingAssistant] Running...\n";
        std::cout << "\n";

        // Main service loop
        RunServiceLoop();

    } catch (const std::exception& ex) {
        std::cout << "[ERROR] " << ex.what() << "\n";
        winrt::uninit_apartment();
        return 1;
    }

    // Cleanup
    std::cout << "\n";
    std::cout << "[MeetingAssistant] Cleaning up...\n";
    
    if (g_pipeServer) {
        g_pipeServer->Stop();
        g_pipeServer.reset();
    }
    
    if (g_monitor) {
        MicrophoneMonitor_Destroy(g_monitor);
    }
    
    g_notificationService.reset();
    g_stateMachine.reset();
    
    std::cout << "[MeetingAssistant] Goodbye!\n";

    winrt::uninit_apartment();
    return 0;
}
