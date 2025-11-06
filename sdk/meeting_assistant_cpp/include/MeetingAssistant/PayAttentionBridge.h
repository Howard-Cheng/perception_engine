#pragma once
#include <string>
#include <functional>

namespace MeetingAssistant {

struct MeetingInfo; // Forward declaration

// Callback types for MSFTCore SDK
typedef void(__cdecl* MSFT_ASRCallback)(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
    uint64_t offset, uint64_t duration, bool isFinal);

typedef void(__cdecl* MSFT_ST_Callback)(bool state);

class PayAttentionBridge {
public:
    // Initialize SDK
    static void Initialize();

    // Start meeting transcription
    static void StartMeetingTranscription(const MeetingInfo& meeting);

    // Stop meeting transcription
    static void StopMeetingTranscription(const MeetingInfo& meeting);

    // Start meeting summarization
    static void StartMeetingSummarization();

    // Stop recording
    static void StopRecord();

private:
    // Internal callbacks
    static void OnASRResult(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
        uint64_t offset, uint64_t duration, bool isFinal);
    
    static void OnSpeechStateChanged(bool state);

    // Meeting content accumulator
    static std::wstring meetingContent_;
    static std::wstring lastMsg_;
    static std::string logFilePath_;
};

} // namespace MeetingAssistant
