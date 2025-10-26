// test_meeting_detection_and_capture.cpp
//
// Simple test program to verify:
// 1. MicrophoneMonitor can detect Teams meetings
// 2. AudioCaptureEngine can capture and transcribe meeting audio
//
// Usage:
// 1. Join a Teams meeting with people speaking
// 2. Run this program
// 3. It will detect the meeting and capture 30 seconds of audio
// 4. Check the transcript to see if meeting audio was captured

#include "MicrophoneMonitor.h"
#include "AudioCaptureEngine.h"
#include "Logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <conio.h>

// Helper function to get timestamp
std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buffer[80];
    struct tm timeinfo;
    localtime_s(&timeinfo, &time);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    return std::string(buffer);
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Meeting Detection + Audio Capture Test\n";
    std::cout << "========================================\n\n";

    // Initialize logger
    Logger::GetInstance().Initialize("test_meeting_capture.log", LogLevel::DEBUG_L);
    LOG_INFO("=== Meeting Detection + Capture Test Started ===");

    // Step 1: Initialize MicrophoneMonitor
    std::cout << "[1/4] Initializing MicrophoneMonitor...\n";
    MicrophoneMonitor monitor;
    std::cout << "      ✓ MicrophoneMonitor initialized\n\n";

    // Step 2: Initialize AudioCaptureEngine
    std::cout << "[2/4] Initializing AudioCaptureEngine...\n";
    AudioCaptureEngine audioEngine;

    // Path to whisper model (adjust if needed)
    std::string modelPath = "../models/whisper/ggml-small.bin";

    if (!audioEngine.Initialize(modelPath)) {
        std::cerr << "      ✗ Failed to initialize AudioCaptureEngine\n";
        std::cerr << "      Check that whisper model exists at: " << modelPath << "\n";
        Logger::GetInstance().Shutdown();
        return 1;
    }
    std::cout << "      ✓ AudioCaptureEngine initialized\n\n";

    // Step 3: Wait for meeting detection
    std::cout << "[3/4] Waiting for meeting detection...\n";
    std::cout << "      (Join a Teams/Zoom meeting now)\n";
    std::cout << "      (Press Q to quit)\n\n";

    bool meetingDetected = false;
    int pollCount = 0;

    while (!meetingDetected) {
        // Check for quit
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'q' || ch == 'Q') {
                std::cout << "\n      User cancelled - exiting\n";
                Logger::GetInstance().Shutdown();
                return 0;
            }
        }

        pollCount++;

        // Check for meeting
        bool hasMeetingApp = monitor.IsMeetingAppUsingMicrophone();

        std::cout << "\r      Poll #" << pollCount << " [" << GetTimestamp() << "] - ";

        if (hasMeetingApp) {
            std::cout << "✓ MEETING APP DETECTED ON MICROPHONE!     \n";
            meetingDetected = true;

            // Log which app was detected
            auto micSessions = monitor.GetActiveMicrophoneSessions();
            for (const auto& session : micSessions) {
                if (MicrophoneMonitor::IsMeetingApp(session.processName)) {
                    std::cout << "      Detected: " << session.processName
                              << " (PID: " << session.processId << ")\n";
                    LOG_INFO_FMT("Meeting detected: %s (PID: %d)",
                                session.processName.c_str(), session.processId);
                }
            }
        } else {
            std::cout << "No meeting app detected...     " << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    std::cout << "\n";

    // Step 4: Start meeting mode and capture audio
    std::cout << "[4/4] Starting meeting audio capture...\n";
    std::cout << "      Capturing for 30 seconds...\n\n";

    // Generate meeting ID with timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char meetingId[64];
    struct tm timeinfo;
    localtime_s(&timeinfo, &time);
    strftime(meetingId, sizeof(meetingId), "meeting_%Y%m%d_%H%M%S", &timeinfo);

    // Start meeting mode
    audioEngine.StartMeetingMode(meetingId);

    if (!audioEngine.IsRunning()) {
        std::cerr << "      ✗ AudioCaptureEngine failed to start\n";
        Logger::GetInstance().Shutdown();
        return 1;
    }

    std::cout << "      ✓ Audio capture started (Meeting ID: " << meetingId << ")\n\n";

    // Capture for 30 seconds, showing progress
    for (int i = 0; i < 30; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Show progress bar
        int progress = ((i + 1) * 100) / 30;
        std::cout << "\r      Progress: [";
        int barWidth = 40;
        int pos = (barWidth * (i + 1)) / 30;
        for (int j = 0; j < barWidth; j++) {
            if (j < pos) std::cout << "=";
            else if (j == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << progress << "% (" << (i + 1) << "s)     " << std::flush;

        // Check if still in meeting
        if (i % 5 == 0) {  // Check every 5 seconds
            bool stillInMeeting = monitor.IsMeetingAppUsingMicrophone();
            if (!stillInMeeting) {
                std::cout << "\n\n      ⚠ Meeting app no longer detected!\n";
                std::cout << "      Stopping capture early...\n";
                break;
            }
        }
    }

    std::cout << "\n\n";

    // Stop meeting mode
    std::cout << "Stopping meeting mode...\n";
    std::string transcriptPath = audioEngine.StopMeetingMode();

    std::cout << "\n========================================\n";
    std::cout << "Capture Complete!\n";
    std::cout << "========================================\n\n";

    // Show transcript preview
    std::string transcript = audioEngine.GetMeetingTranscript();

    if (transcript.empty()) {
        std::cout << "⚠ No transcript generated\n";
        std::cout << "  Possible reasons:\n";
        std::cout << "  - No speech detected during capture\n";
        std::cout << "  - Microphone level too low\n";
        std::cout << "  - Whisper model failed to load\n";
        std::cout << "  - System audio loopback not capturing meeting audio\n\n";
        std::cout << "Check the log file for details: test_meeting_capture.log\n";
    } else {
        std::cout << "✓ Transcript generated!\n\n";
        std::cout << "Preview (first 500 chars):\n";
        std::cout << "─────────────────────────────────────\n";

        std::string preview = transcript.length() > 500
            ? transcript.substr(0, 500) + "..."
            : transcript;
        std::cout << preview << "\n";
        std::cout << "─────────────────────────────────────\n\n";

        std::cout << "Full transcript length: " << transcript.length() << " characters\n";
        std::cout << "Saved to: " << transcriptPath << "\n";
    }

    std::cout << "\n";

    // Show performance metrics
    auto metrics = audioEngine.GetMetrics();
    std::cout << "Performance Metrics:\n";
    std::cout << "  Whisper latency: " << metrics.whisperLatencyMs << " ms\n";
    std::cout << "  Total latency: " << metrics.totalLatencyMs << " ms\n";
    std::cout << "  Speech detected: " << (metrics.isSpeechDetected ? "Yes" : "No") << "\n";

    std::cout << "\n========================================\n";
    std::cout << "Test Complete\n";
    std::cout << "========================================\n";

    LOG_INFO("=== Meeting Detection + Capture Test Ended ===");
    Logger::GetInstance().Shutdown();

    std::cout << "\nPress any key to exit...";
    _getch();

    return 0;
}
