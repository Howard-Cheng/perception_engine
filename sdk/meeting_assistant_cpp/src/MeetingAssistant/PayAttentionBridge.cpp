#include "PayAttentionBridge.h"
#include "MeetingStateMachine.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace MeetingAssistant {

// Use the MSFTCore SDK namespaces
//using namespace QAASR;
//using namespace QTSummary;

// Static members
std::wstring PayAttentionBridge::meetingContent_;
std::wstring PayAttentionBridge::lastMsg_;
std::string PayAttentionBridge::logFilePath_ = "meeting_assistant.log";

void PayAttentionBridge::Initialize() {
    // Set listening device (1=Microphone, 2=Speaker, 3=Microphone+Speaker)
    //QAASR::SetListeningDevice(2);

    //// Set ASR callback
    //QAASR::SetASRCallback(OnASRResult);

    //// Set speech state callback
    //QAASR::SetSpeechStateCallback(OnSpeechStateChanged);

    std::cout << "[PayAttentionBridge] SDK Initialized.\n";
}

void PayAttentionBridge::StartMeetingTranscription(const MeetingInfo& meeting) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &now_c);

    // Format log message
    std::ostringstream logMessage;
    logMessage << "\n================================================================================\n";
    logMessage << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] CALLING PAY ATTENTION SDK\n";
    logMessage << "================================================================================\n";
    logMessage << "Action: Start Meeting Transcription\n";
    logMessage << "App Name: " << meeting.appName << "\n";
    logMessage << "Process ID: " << meeting.processId << "\n";
    
    std::time_t detected_c = std::chrono::system_clock::to_time_t(meeting.detectedAt);
    std::tm detected_tm;
    localtime_s(&detected_tm, &detected_c);
    logMessage << "Meeting Started: " << std::put_time(&detected_tm, "%Y-%m-%d %H:%M:%S") << "\n";
    logMessage << "User Confirmed: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "\n";
    logMessage << "================================================================================\n\n";

    std::cout << "Starting record...\n";
    //QAASR::StartRecord();

    // Log to file
    try {
        std::ofstream logFile(logFilePath_, std::ios::app);
        if (logFile.is_open()) {
            logFile << logMessage.str();
            logFile.close();
            std::cout << "[PayAttentionBridge] Logged to: " << logFilePath_ << "\n";
        }
    } catch (const std::exception& ex) {
        std::cout << "[PayAttentionBridge] Failed to write log: " << ex.what() << "\n";
    }

    // Log to console
    std::cout << logMessage.str();
}

void PayAttentionBridge::StopMeetingTranscription(const MeetingInfo& meeting) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &now_c);

    std::ostringstream logMessage;
    logMessage << "\n[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
               << "] CALLING PAY ATTENTION SDK: Stop Transcription\n";
    logMessage << "App: " << meeting.appName << ", PID: " << meeting.processId << "\n";

    std::cout << "Stopping record...\n";
   /* QAASR::StopRecord();*/
    meetingContent_.clear();
    lastMsg_.clear();

    try {
        std::ofstream logFile(logFilePath_, std::ios::app);
        if (logFile.is_open()) {
            logFile << logMessage.str();
            logFile.close();
        }
    } catch (...) {}

    std::cout << logMessage.str();
}

void PayAttentionBridge::StartMeetingSummarization() {
    const wchar_t* prompt = L"You are a voice assistant. When given a piece of dialogue, "
                            L"analyze it and extract the main points. Organize the key ideas as "
                            L"a clear outline. Keep the summary within 300 words. Output in English only.";
    
    /*QTSummary::EnableSummary(prompt, meetingContent_.c_str());

    wchar_t* summaryPtr = QTSummary::StartSummary();
    std::wstring summary = summaryPtr ? summaryPtr : L"";
    */
    //std::wcout << L"[Meeting Summary]\n" << summary << L"\n";
}

void PayAttentionBridge::StopRecord() {
    //QAASR::StopRecord();
}

void PayAttentionBridge::OnASRResult(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
    uint64_t offset, uint64_t duration, bool isFinal) {
    
    if (isFinal) {
        // Check if current msg does NOT fully contain last msg (case-insensitive)
        std::wstring msgStr(msg);
        std::wstring lastMsgLower = lastMsg_;
        std::wstring msgLower = msgStr;
        
        // Convert to lowercase for case-insensitive comparison
        std::transform(lastMsgLower.begin(), lastMsgLower.end(), lastMsgLower.begin(), ::towlower);
        std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::towlower);
        
        if (lastMsg_.empty() || msgLower.find(lastMsgLower) == std::wstring::npos) {
            std::wcout << L"[ASR] Role: " << role << L", Msg: " << msg 
                       << L", Lang: " << targetLang << L", Offset: " << offset 
                       << L", Duration: " << duration << L", Final: " << (isFinal ? L"true" : L"false") << L"\n";
            
            meetingContent_ += msg;
            meetingContent_ += L"\n";
        }
        
        lastMsg_ = msgStr;
    }
}

void PayAttentionBridge::OnSpeechStateChanged(bool state) {
    std::cout << "[SpeechState] State: " << (state ? "Speaking" : "Silent") << "\n";
}

} // namespace MeetingAssistant
