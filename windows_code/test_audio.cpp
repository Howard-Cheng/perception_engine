#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "AudioCaptureEngine.h"

int main() {
    std::cout << "Audio Capture Engine Test" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << std::endl;

    // Create audio engine
    AudioCaptureEngine audioEngine;

    // Initialize with whisper model
    std::string modelPath = "models/whisper/ggml-tiny.en.bin";
    std::cout << "[INFO] Initializing whisper model: " << modelPath << std::endl;

    if (!audioEngine.Initialize(modelPath)) {
        std::cout << "[ERROR] Failed to initialize audio engine!" << std::endl;
        std::cout << "Make sure the model exists at: " << modelPath << std::endl;
        return 1;
    }

    std::cout << "[SUCCESS] Audio engine initialized!" << std::endl;
    std::cout << std::endl;

    // Start audio capture
    std::cout << "[INFO] Starting audio capture..." << std::endl;
    if (!audioEngine.Start()) {
        std::cout << "[ERROR] Failed to start audio capture!" << std::endl;
        std::cout << "Possible issues:" << std::endl;
        std::cout << "  1. Microphone not accessible (check permissions)" << std::endl;
        std::cout << "  2. Audio device not found" << std::endl;
        std::cout << "  3. WASAPI initialization failed" << std::endl;
        return 1;
    }

    std::cout << "[SUCCESS] Audio capture started!" << std::endl;
    std::cout << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "Speak into your microphone..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << std::endl;

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
                std::cout << "[USER] " << userSpeech << std::endl;
                lastUserSpeech = userSpeech;
            }

            // Optionally get system audio (if implemented)
            std::string systemAudio = audioEngine.GetLatestSystemAudio();
            if (!systemAudio.empty() && systemAudio != lastSystemAudio) {
                std::cout << "[SYSTEM] " << systemAudio << std::endl;
                lastSystemAudio = systemAudio;
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << std::endl;
        std::cout << "[ERROR] Exception: " << e.what() << std::endl;
    }

    // Cleanup
    std::cout << std::endl;
    std::cout << "[INFO] Stopping audio capture..." << std::endl;
    audioEngine.Stop();
    std::cout << "[INFO] Cleanup complete" << std::endl;

    return 0;
}
