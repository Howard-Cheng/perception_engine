/**
 * @file TestASRMain.cpp
 * @brief Test application for Microsoft Speech Foundation Core (MSFTCore) ASR library
 * @details This application demonstrates usage of the ASR library including:
 *          - Real-time speech recognition
 *          - Translation capabilities
 *          - Text summarization
 *          - Speech state callbacks
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <io.h>
#include <fcntl.h>
#include "MSFTCore.h"

// Global flag to control the main loop
//std::atomic<bool> g_running(true);
//
/**
 * @brief Callback function for ASR transcription results
 * @param role The role associated with the transcribed text
 * @param msg The transcribed message
 * @param targetLang The target language
 * @param offset The offset timestamp
 * @param duration The duration
 * @param isFinal Whether the transcription is final
 */
void ASRCallback(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
    uint64_t offset, uint64_t duration, int isFinal) {
    if (msg && wcslen(msg) > 0) {
        std::wcout << L"\n[ASR Result";
        if (role && wcslen(role) > 0) {
            std::wcout << L" - " << role;
        }
        if (isFinal) {
            std::wcout << L" (Final)";
        }
        std::wcout << L"]: " << msg << std::endl;
        std::wcout << L"> ";  // Re-print prompt
        std::wcout.flush();
    }
}

/**
 * @brief Callback function for speech state changes
 * @param state The current speech state (true = speaking, false = silent)
 */
void SpeechStateCallback(bool state) {
    if (state) {
        std::wcout << L"\n[State]: Speech detected (Speaking...)" << std::endl;
    } else {
        std::wcout << L"\n[State]: Speech ended (Silent)" << std::endl;
    }
    std::wcout << L"> ";  // Re-print prompt
    std::wcout.flush();
}

///**
// * @brief Callback function for summarization results
// * @param summary The summary text
// * @param sessionId The session identifier
// * @param success Indicates whether the summarization was successful
// */
//void OnSummarizedResult(const wchar_t* summary, const wchar_t* sessionId, bool success) {
//    if (summary && wcslen(summary) > 0) {
//        std::wcout << L"\n[Summary Result";
//        if (success) {
//            std::wcout << L" (Success)";
//        } else {
//            std::wcout << L" (Failed)";
//        }
//        std::wcout << L"]: " << summary << std::endl;
//        std::wcout << L"> ";  // Re-print prompt
//        std::wcout.flush();
//    }
//}
//
///**
// * @brief Callback function for summarization progress
// * @param progress Progress value (0-100)
// */
//void OnProgressUpdate(int progress) {
//    std::wcout << L"\r[Summary Progress]: " << progress << L"%";
//    std::wcout.flush();
//    if (progress >= 100) {
//        std::wcout << std::endl;
//    }
//}
//
///**
// * @brief Print available commands
// */
//void PrintHelp() {
//    std::wcout << L"\n========================================" << std::endl;
//    std::wcout << L"   TestASR - ASR Library Test Program" << std::endl;
//    std::wcout << L"========================================" << std::endl;
//    std::wcout << L"Available commands:" << std::endl;
//    std::wcout << L"  start   - Start audio recording" << std::endl;
//    std::wcout << L"  stop    - Stop audio recording and get summary" << std::endl;
//    std::wcout << L"  pause   - Pause audio recording" << std::endl;
//    std::wcout << L"  resume  - Resume audio recording" << std::endl;
//    std::wcout << L"  help    - Display this help message" << std::endl;
//    std::wcout << L"  quit    - Exit the program" << std::endl;
//    std::wcout << L"  exit    - Exit the program" << std::endl;
//    std::wcout << L"========================================\n" << std::endl;
//}

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "test" << std::endl;
    // Set console output to support wide characters (UTF-16)
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    std::wcout << L"Initializing ASR system..." << std::endl;

    // Set language for ASR
    //QAASR::SetASRLanguage(L"zh-CN");  // Chinese (China)
    
    // Register callback functions
    QAASR::SetASRCallback(ASRCallback);
    //QAASR::SetSpeechStateCallback(SpeechStateCallback);
    
    std::wcout << L"ASR Initialized!" << std::endl;
    //PrintHelp();

    //// Enable translation (optional)
    //QATranslation::EnableTranslation(true, L"zh-CN", L"en-US");
    //
    //// Configure summarization (optional)
    //const wchar_t* prompt = L"You are a voice assistant. When given a piece of dialogue, analyze it and extract the main points. Organize the key ideas as a clear outline. Keep the summary within 300 words. Output in English only.";
    //const wchar_t* content = L"";  // Initial empty content
    //QTSummary::EnableSummary(prompt, content);
    //QTSummary::SummaryOnResult(OnSummarizedResult);
    //QTSummary::SummaryProgress(OnProgressUpdate);
    //
    //// Command processing loop
    //std::wstring command;
    //while (g_running) {
    //    std::wcout << L"> ";
    //    std::wcin >> command;
    //    
    //    // Convert to lowercase for comparison
    //    std::transform(command.begin(), command.end(), command.begin(), ::towlower);
    //    
    //    if (command == L"start") {
    //        std::wcout << L"Starting recording..." << std::endl;
    //        QAASR::StartRecord();
    //    }
    //    else if (command == L"stop") {
    //        std::wcout << L"Stopping recording..." << std::endl;
    //        QAASR::StopRecord();
    //        
    //        // Start summarization if available
    //        //auto result = QTSummary::StartSummary(nullptr);
    //        //if (result && wcslen(result) > 0) {
    //        //    std::wcout << L"Summary result: " << result << std::endl;
    //        //}
    //    }
    //    else if (command == L"pause") {
    //        std::wcout << L"Pausing recording..." << std::endl;
    //        QAASR::PauseRecord();
    //    }
    //    else if (command == L"resume") {
    //        std::wcout << L"Resuming recording..." << std::endl;
    //        QAASR::ResumeRecord();
    //    }
    //    else if (command == L"help") {
    //        PrintHelp();
    //    }
    //    else if (command == L"quit" || command == L"exit") {
    //        std::wcout << L"Exiting..." << std::endl;
    //        g_running = false;
    //    }
    //    else {
    //        std::wcout << L"Unknown command. Type 'help' for available commands." << std::endl;
    //    }
    //    
    //    // Brief delay to avoid busy waiting
    //    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //}
    //
    //// Cleanup: stop recording if still active
    //QAASR::StopRecord();
    //std::wcout << L"Program terminated." << std::endl;
    
    return 0;
}
