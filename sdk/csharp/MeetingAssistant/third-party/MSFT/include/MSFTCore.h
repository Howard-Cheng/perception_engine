#pragma once
#include <functional>
#include <string>
#include "Summary.h"

#ifdef DLL_EXPORTS
#    define DLL_API __declspec(dllexport)
#else
#    define DLL_API __declspec(dllimport)
#endif

/**
 * @brief Callback for ASR (Automatic Speech Recognition) results.
 * @param role The role associated with the transcribed text (e.g., "speaker1", "speaker2").
 * @param msg The transcribed message.
 * @param speakerStartTime The timestamp when the speaker started talking.
 * @param isFinal Whether the transcription is final (true) or interim (false).
 */
typedef void(__cdecl* MSFT_ASRCallback)(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
    uint64_t offset, uint64_t duration, bool isFinal);

typedef void(__cdecl* ShortRecord_Callback)(const wchar_t* role, const wchar_t* msg, const wchar_t* targetLang,
    uint64_t offset, uint64_t duration, bool isFinal);

typedef void(__cdecl* ChatCallback)(const wchar_t* msg);

/**
 * @brief Callback for speech state changes (start/stop speaking).
 * @param state True if speech is detected, false if speech ends.
 */
typedef void(__cdecl* MSFT_ST_Callback)(bool state);

extern "C" {
	/**
	 * @brief create ipc server pipe.
	*/
	DLL_API wchar_t* CreaIPCServer();
}

namespace QAASR
{
    extern "C" {
        /**
         * @brief Sets the current listening device.
         * @param deviceID 1: Microphone only, 2: Speaker only, 3: Microphone + Speaker.
         */
        DLL_API void SetListeningDevice(int deviceID);

        /**
         * @brief Sets the language for ASR (Automatic Speech Recognition).
         * @param language The language code (e.g., "en-US", "zh-CN").
         */
        DLL_API void SetASRLanguage(const wchar_t* language);
        /**
         * @brief Sets the callback for real-time ASR transcription results.
         * @param callback The function pointer to handle ASR results.
         */
        DLL_API void SetASRCallback(MSFT_ASRCallback callback);

        /**
         * @brief Sets the callback for speech activity detection (start/stop speaking).
         * @param callback The function pointer to handle speech state changes.
         */
        DLL_API void SetSpeechStateCallback(MSFT_ST_Callback callback);

        /**
         * @brief Sets the model request source type.
         * @param type 1: Local, 2: Cloud.
         */
        DLL_API void SetMSFTRequireType(int type);

        /**
         * @brief Sets the end-of-speech silence timeout threshold (for online models).
         * @param time Timeout in milliseconds.
         */
        DLL_API void SetEndSilenceTimeout(int time);

        /**
         * @brief Starts audio recording based on the current listening mode.
         */
        DLL_API void StartRecord();

        /**
         * @brief Stops audio recording.
         */
        DLL_API void StopRecord();

        /**
         * @brief Pauses audio recording.
         */
        DLL_API void PauseRecord();

        /**
         * @brief Resumes a paused recording.
         */
        DLL_API void ResumeRecord();

        /**
         * @brief Sets whether to save the recorded audio file.
         * @param save True to save, false otherwise.
         * @param filename The path and name of the file to save (optional).
         */
        DLL_API void IsSaveRecordingFile(bool save = false, const wchar_t* filename = nullptr);
    
        DLL_API wchar_t* GetAudioPath();
    };
}
    namespace QAASRSHORT
    {
        extern "C" {
        /**
         * @brief Sets the callback for real-time ASR transcription results.
         * @param callback The function pointer to handle ASR results.
         */
        DLL_API void SetShortRecordCallback(ShortRecord_Callback callback);

        /**
         * @brief Starts audio recording based on the current listening mode.
         */
        DLL_API void StartShortRecord();

        /**
         * @brief Stops audio recording.
         */
        DLL_API void StopShortRecord();

        DLL_API void ChatMessage(const wchar_t* msg);

        DLL_API void SetChatCallback(ChatCallback callback);
    };
}

namespace QATTS
{
    extern "C" {
        /**
         * @brief Sets the language for TTS (Text-to-Speech).
         * @param language The language code (e.g., "en-US", "zh-CN").
         */
        DLL_API void SetTTSLanguage(const wchar_t* language);

        /**
         * @brief Initializes the TTS engine.
         */
        DLL_API void InitializeTTS();

        /**
         * @brief Uninitializes the TTS engine.
         */
        DLL_API void UNInitializeTTS();

        /**
         * @brief Sets the callback for synthesis started event.
         * @param callback The callback function.
         */
        DLL_API void onSynthesisStarted(std::function<void()> callback);

        /**
         * @brief Sets the callback for synthesis completed event.
         * @param callback The callback function.
         */
        DLL_API void onSynthesisCompleted(std::function<void()> callback);

        /**
         * @brief Speaks the given text.
         * @param text The text to be spoken.
         * @return True if successful, false otherwise.
         */
        DLL_API bool SpeakText(const wchar_t* text);

        /**
         * @brief Stops the current speech synthesis.
         * @return True if successful, false otherwise.
         */
        DLL_API bool StopSpeaking();
    };
}

namespace QATranslation
{
    extern "C" {
        /**
         * @brief Turn real-time translation on or off.
         * @param state True to on, false off.
         * @param sourceLanguage translation form source.
         * @param targetLanguage translation to target language.
         */
        DLL_API void EnableTranslation(bool state, const wchar_t* sourceLanguage, const wchar_t* targetLanguage);

    }
}

namespace QTSummary {

    extern "C" {
        DLL_API void EnableSummary(const wchar_t* prompt, const wchar_t* content);
        DLL_API wchar_t* StartSummary();
    }
}
namespace QTLocalASR {

    extern "C" {
        DLL_API wchar_t* GetRecordResult();
    }
}

