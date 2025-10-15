#include "Logger.h"  // NEW: Add Logger
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "AudioCaptureEngine.h"

int main() {
    // Initialize Logger FIRST
    Logger::GetInstance().Initialize("test_audio.log", LogLevel::DEBUG_L);
    
    LOG_INFO("=====================================");
    LOG_INFO("Audio Capture Engine Test");
    LOG_INFO("=====================================");

    // Create audio engine
    AudioCaptureEngine audioEngine;

    // Initialize with whisper model
    std::string modelPath = "models/whisper/ggml-small.bin";
    LOG_INFO_FMT("Initializing whisper model: %s", modelPath.c_str());

    if (!audioEngine.Initialize(modelPath)) {
        LOG_FATAL("Failed to initialize audio engine!");
        LOG_ERROR_FMT("Make sure the model exists at: %s", modelPath.c_str());
        Logger::GetInstance().Shutdown();
        return 1;
    }

    LOG_INFO("Audio engine initialized!");

    // Start audio capture
    LOG_INFO("Starting audio capture...");
    if (!audioEngine.Start()) {
        LOG_FATAL("Failed to start audio capture!");
        LOG_ERROR("Possible issues:");
        LOG_ERROR("  1. Microphone not accessible (check permissions)");
        LOG_ERROR("  2. Audio device not found");
        LOG_ERROR("  3. WASAPI initialization failed");
        Logger::GetInstance().Shutdown();
        return 1;
    }

    LOG_INFO("Audio capture started!");
    LOG_INFO("=====================================");
    LOG_INFO("Speak into your microphone...");
    LOG_INFO("Press Ctrl+C to stop");
    LOG_INFO("=====================================");

    // Main loop - print NEW transcriptions only
    std::string lastUserSpeech = "";
    std::string lastSystemAudio = "";

    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Get latest transcription
            std::string userSpeech = audioEngine.GetLatestUserSpeech();

            // Only print if it changed
            if (!userSpeech.empty() && userSpeech != lastUserSpeech) {
                LOG_INFO_FMT("[USER] %s", userSpeech.c_str());
                lastUserSpeech = userSpeech;
            }

            // Optionally get system audio (if implemented)
            std::string systemAudio = audioEngine.GetLatestSystemAudio();
            if (!systemAudio.empty() && systemAudio != lastSystemAudio) {
                LOG_INFO_FMT("[SYSTEM] %s", systemAudio.c_str());
                lastSystemAudio = systemAudio;
            }
        }
    }
    catch (const std::exception& e) {
        LOG_FATAL_FMT("Exception: %s", e.what());
    }

    // Cleanup
    LOG_INFO("Stopping audio capture...");
    audioEngine.Stop();
    LOG_INFO("Cleanup complete");
    
    Logger::GetInstance().Shutdown();
    return 0;
}
