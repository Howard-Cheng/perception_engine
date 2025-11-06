#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <Windows.h>
#include <winrt/base.h>

#include "MeetingStateMachine.h"
#include "MicrophoneMonitorDLL.h"
#include "NamedPipeServer.h"

using namespace MeetingAssistant;

// Global variables
std::atomic<bool> g_running(true);
MicrophoneMonitorHandle g_monitor = nullptr;
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

void RunServiceLoop() {
    while (g_running) {
        try {
            // Check for meeting apps
            bool meetingDetected = MicrophoneMonitor_IsMeetingAppUsingMicrophone(g_monitor) != 0;
            std::optional<MeetingInfo> meetingInfo;
            MeetingInfo info;
            info.appName = "empty";
            info.processId = 0;
            info.detectedAt = std::chrono::system_clock::now();
            meetingInfo = info;

            if (meetingDetected) {
                char buffer[256] = { 0 };
                int length = MicrophoneMonitor_GetMeetingAppName(g_monitor, buffer, sizeof(buffer));
                unsigned long pid = MicrophoneMonitor_GetMeetingAppPID(g_monitor);

                if (length > 0) {
                    info.appName = buffer;
                    info.processId = pid;
                    info.detectedAt = std::chrono::system_clock::now();
                    meetingInfo = info;
                }
            }

            // ✅ 只有在 meetingInfo 有值时才通知
            if (meetingDetected && meetingInfo.has_value()) {
                UserResponse response = g_pipeServer->NotifyMeetingEvent(
                    MeetingEvent::Started,
                    meetingInfo->appName,
                    meetingInfo->processId
                );
            }
            else {
                UserResponse response = g_pipeServer->NotifyMeetingEvent(
                    MeetingEvent::Ended,
                    meetingInfo->appName,
                    meetingInfo->processId
                );
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(2));  // Poll every 2 seconds
            continue;

            // ========== 以下代码被 continue 跳过，保留以便后续恢复 ==========

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

        // Initialize named pipe server
        g_pipeServer = std::make_unique<NamedPipeServer>();
        if (g_pipeServer->Start()) {
            std::cout << "[✓] Named pipe server started\n";
        } else {
            std::cout << "[WARNING] Failed to start named pipe server\n";
        }


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
    
    std::cout << "[MeetingAssistant] Goodbye!\n";

    winrt::uninit_apartment();
    return 0;
}
