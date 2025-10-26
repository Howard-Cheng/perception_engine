/**
 * test_mic_monitor.cpp - Standalone test for MicrophoneMonitor
 *
 * Purpose: Verify that MicrophoneMonitor can detect audio device usage
 *
 * Test Instructions:
 * 1. Build: cl test_mic_monitor.cpp MicrophoneMonitor.cpp Logger.cpp /I. /Fe:test_mic_monitor.exe
 * 2. Run: test_mic_monitor.exe
 * 3. Join a Zoom/Teams meeting
 * 4. Verify console shows "MEETING APP DETECTED"
 * 5. Leave meeting
 * 6. Verify console shows no meeting apps after 10 seconds
 *
 * Expected Output:
 *   --- Checking audio devices ---
 *   Microphone sessions: 2
 *     - Zoom.exe (PID: 12345) [MEETING APP DETECTED]
 *     - chrome.exe (PID: 67890)
 *   Speaker sessions: 1
 *     - Zoom.exe (PID: 12345) [MEETING APP DETECTED]
 *
 *   *** MEETING DETECTED! ***
 */

#include "MicrophoneMonitor.h"
#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <conio.h>  // For _kbhit() to detect key press

// Helper: format timestamp
std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &time_t_now);

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    return std::string(buffer);
}

// Helper: print colored console output
void PrintColored(const std::string& text, int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
    std::cout << text;
    SetConsoleTextAttribute(hConsole, 7);  // Reset to white
}

int main() {
    // Initialize COM (required for Windows Audio APIs)
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "ERROR: Failed to initialize COM (HRESULT: 0x"
                  << std::hex << hr << ")\n";
        std::cerr << "Make sure you're running on Windows with audio devices enabled.\n";
        return 1;
    }

    // Initialize Logger
    Logger::GetInstance().Initialize("test_mic_monitor.log", LogLevel::DEBUG_L);
    LOG_INFO("=== MicrophoneMonitor Test Started ===");

    std::cout << "\n";
    PrintColored("╔═══════════════════════════════════════════════════════════╗\n", 11);
    PrintColored("║         MicrophoneMonitor - Standalone Test              ║\n", 11);
    PrintColored("╚═══════════════════════════════════════════════════════════╝\n", 11);
    std::cout << "\n";

    std::cout << "This test monitors audio device usage in real-time.\n";
    std::cout << "It will show which apps are using your microphone and speakers.\n\n";

    PrintColored("Test Instructions:\n", 14);
    std::cout << "1. Leave this program running\n";
    std::cout << "2. Join a Zoom/Teams/Google Meet call\n";
    std::cout << "3. Watch for \"*** MEETING DETECTED! ***\" message\n";
    std::cout << "4. Verify your meeting app appears in the list\n";
    std::cout << "5. Leave the meeting and verify detection stops\n";
    std::cout << "6. Press 'Q' to quit\n\n";

    // Create MicrophoneMonitor
    std::cout << "Initializing MicrophoneMonitor...\n";
    MicrophoneMonitor monitor;
    std::cout << "✓ MicrophoneMonitor initialized successfully\n\n";

    LOG_INFO("MicrophoneMonitor initialized - starting monitoring loop");

    int pollCount = 0;
    bool lastMeetingState = false;

    // Main monitoring loop
    while (true) {
        // Check for 'Q' key press to quit
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                std::cout << "\n\nQuitting...\n";
                break;
            }
        }

        pollCount++;

        // Clear screen (optional, comment out if you want scrolling history)
        // system("cls");

        std::cout << "\n";
        PrintColored("═══════════════════════════════════════════════════════════\n", 8);
        std::cout << "Poll #" << pollCount << " - " << GetTimestamp() << "\n";
        PrintColored("═══════════════════════════════════════════════════════════\n", 8);

        // === Get Microphone Sessions ===
        auto micSessions = monitor.GetActiveMicrophoneSessions();

        PrintColored("\n🎤 MICROPHONE SESSIONS: ", 13);
        std::cout << micSessions.size() << "\n";

        if (micSessions.empty()) {
            std::cout << "   (No apps using microphone)\n";
        } else {
            for (const auto& session : micSessions) {
                std::cout << "   • " << session.processName
                          << " (PID: " << session.processId << ")";

                // Highlight meeting apps
                if (MicrophoneMonitor::IsMeetingApp(session.processName)) {
                    PrintColored(" [MEETING APP] ✓", 10);
                }

                std::cout << "\n";
                std::cout << "     Display: " << session.displayName << "\n";
            }
        }

        // === Get Speaker Sessions ===
        auto speakerSessions = monitor.GetActiveSpeakerSessions();

        PrintColored("\n🔊 SPEAKER SESSIONS: ", 13);
        std::cout << speakerSessions.size() << "\n";

        if (speakerSessions.empty()) {
            std::cout << "   (No apps using speakers)\n";
        } else {
            for (const auto& session : speakerSessions) {
                std::cout << "   • " << session.processName
                          << " (PID: " << session.processId << ")";

                // Highlight meeting apps
                if (MicrophoneMonitor::IsMeetingApp(session.processName)) {
                    PrintColored(" [MEETING APP] ✓", 10);
                }

                std::cout << "\n";
                std::cout << "     Display: " << session.displayName << "\n";
            }
        }

        // === Meeting Detection Status ===
        bool meetingDetected = monitor.IsMeetingAppUsingMicrophone();
        bool bidirectionalAudio = monitor.IsMeetingAppUsingMicrophone() &&
                                  monitor.IsMeetingAppUsingSpeakers();

        std::cout << "\n";
        PrintColored("═══════════════════════════════════════════════════════════\n", 8);
        PrintColored("MEETING DETECTION STATUS:\n", 14);

        std::cout << "   Meeting app using MIC:     ";
        if (monitor.IsMeetingAppUsingMicrophone()) {
            PrintColored("YES ✓\n", 10);
        } else {
            PrintColored("NO\n", 7);
        }

        std::cout << "   Meeting app using SPEAKER: ";
        if (monitor.IsMeetingAppUsingSpeakers()) {
            PrintColored("YES ✓\n", 10);
        } else {
            PrintColored("NO\n", 7);
        }

        std::cout << "   Bidirectional audio:       ";
        if (bidirectionalAudio) {
            PrintColored("YES ✓ (High confidence meeting)\n", 10);
        } else {
            PrintColored("NO\n", 7);
        }

        // === Final Verdict ===
        std::cout << "\n";
        if (meetingDetected && bidirectionalAudio) {
            PrintColored("╔═══════════════════════════════════════════════════════╗\n", 10);
            PrintColored("║                                                       ║\n", 10);
            PrintColored("║          *** MEETING DETECTED! ***                   ║\n", 10);
            PrintColored("║                                                       ║\n", 10);
            PrintColored("╚═══════════════════════════════════════════════════════╝\n", 10);

            if (!lastMeetingState) {
                LOG_INFO("MEETING STARTED - Bidirectional audio detected");
                lastMeetingState = true;
            }
        } else if (meetingDetected) {
            PrintColored("⚠️  MEETING APP USING MIC (waiting for bidirectional audio)\n", 14);

            if (!lastMeetingState) {
                LOG_INFO("Meeting app detected using microphone (partial signal)");
            }
        } else {
            std::cout << "ℹ️  No meeting detected\n";

            if (lastMeetingState) {
                LOG_INFO("MEETING ENDED - No meeting apps detected");
                lastMeetingState = false;
            }
        }

        PrintColored("\nPress 'Q' to quit | Refreshing in 5 seconds...\n", 8);

        // Wait 5 seconds before next poll
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Cleanup
    LOG_INFO("=== MicrophoneMonitor Test Ended ===");
    Logger::GetInstance().Shutdown();
    CoUninitialize();

    std::cout << "\n✓ Test completed successfully\n";
    std::cout << "Check 'test_mic_monitor.log' for detailed logs\n\n";

    return 0;
}
