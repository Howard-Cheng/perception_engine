//
// SpeechRecognition.h
// Real-time Speech Recognition Module - Based on Microsoft Azure Speech SDK
//

#pragma once

#include "SpeechRecognitionExport.h"
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

// Forward declarations for Microsoft Azure Speech SDK types
namespace Microsoft {
    namespace CognitiveServices {
        namespace Speech {
            class EmbeddedSpeechConfig;
            class KeywordRecognitionModel;
            class SpeechRecognizer;
            class SpeechConfig;
        }
    }
}

namespace SpeechRecognitionModule
{
    // Speech recognition state enum
    enum class SPEECHRECOGNITION_API SpeechStateEnum
    {
        WaitInitialize = 0,
        Initializing = 1,
        InitializeFail = 2,
        InitializeSuccess = 3
    };

    // Callback function type definitions
    using TimeOutCallback = std::function<void()>;
    using UpdateTextCallback = std::function<void(const std::string&)>;
    using RecognizingKeyWordCallback = std::function<void()>;
    using RecognizedKeyWordCallback = std::function<void()>;
    using RecognizeStartCallback = std::function<void()>;
    using RecognizeStopCallback = std::function<void()>;
    using RecognizedCallback = std::function<void(const std::string&)>;

    class SPEECHRECOGNITION_API SpeechRecognition
    {
    public:
        SpeechRecognition();
        ~SpeechRecognition();

        // Initialize using SpeechHelper configuration (embedded mode, recommended)
        // Call SpeechHelper::EnvironmentVerify() first to verify environment
        bool Initialize();

        // Initialize speech recognizer (online mode)
        // subscriptionKey: Azure Speech Service subscription key
        // serviceRegion: Service region (e.g., "eastasia", "westus")
        // language: Recognition language (e.g., "zh-CN", "en-US")
        bool InitializeOnline(const std::string& subscriptionKey,
            const std::string& serviceRegion,
            const std::string& language = "zh-CN");

        // Initialize with embedded model (offline mode)
        // modelPath: Embedded model path
        // modelName: Model name
        bool InitializeWithEmbeddedModel(const std::string& modelPath,
            const std::string& modelName,
            const std::string& modelLicense = "");

        // Start continuous speech recognition
        bool BeginRecognizer();

        // Start continuous speech recognition (alias for compatibility)
        bool StartRecognition() { return BeginRecognizer(); }

        // Stop speech recognition
        void StopRecognizer();

        // Stop speech recognition (alias for compatibility)
        void StopRecognition() { StopRecognizer(); }

        // Start keyword recognition
        bool BeginKeyWordRecognizer();

        // Start keyword recognition (alias)
        bool StartKeywordRecognition(const std::string& keywordModelPath);

        // Stop keyword recognition
        void StopKeyWordRecognizer();

        // Stop keyword recognition (alias)
        void StopKeywordRecognition() { StopKeyWordRecognizer(); }

        // Get current recognition state
        SpeechStateEnum GetState() const { return m_state; }

        // Check if recognition is running
        bool IsRunning() const { return m_isRunning; }

        // Set callback functions
        void SetTimeOutCallback(TimeOutCallback callback) { m_timeOutCallback = callback; }
        void SetUpdateTextCallback(UpdateTextCallback callback) { m_updateTextCallback = callback; }
        void SetRecognizingKeyWordCallback(RecognizingKeyWordCallback callback) { m_recognizingKeyWordCallback = callback; }
        void SetRecognizedKeyWordCallback(RecognizedKeyWordCallback callback) { m_recognizedKeyWordCallback = callback; }
        void SetRecognizeStartCallback(RecognizeStartCallback callback) { m_recognizeStartCallback = callback; }
        void SetRecognizeStopCallback(RecognizeStopCallback callback) { m_recognizeStopCallback = callback; }
        void SetRecognizedCallback(RecognizedCallback callback) { m_recognizedCallback = callback; }

        // Set speech recognition parameters
        void SetSilenceTimeout(int milliseconds) { m_silenceTimeoutMs = milliseconds; }
        void SetEndSilenceTimeout(int milliseconds) { m_endSilenceTimeoutMs = milliseconds; }

        // Set keyword model path
        void SetKeywordModelPath(const std::string& path) { m_keywordModelPath = path; }

    private:
        // Setup event handlers
        void SetupEventHandlers();

        // Create speech config (online mode)
        std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechConfig> CreateSpeechConfig(
            const std::string& subscriptionKey,
            const std::string& serviceRegion,
            const std::string& language);

        // Create embedded speech config (offline mode)
        std::shared_ptr<Microsoft::CognitiveServices::Speech::EmbeddedSpeechConfig> CreateEmbeddedSpeechConfig(
            const std::string& modelPath,
            const std::string& modelName,
            const std::string& modelLicense);

    private:
        std::shared_ptr<Microsoft::CognitiveServices::Speech::SpeechRecognizer> m_recognizer;
        std::shared_ptr<Microsoft::CognitiveServices::Speech::KeywordRecognitionModel> m_keywordModel;

        std::atomic<SpeechStateEnum> m_state;
        std::atomic<bool> m_isRunning;
        std::string m_accumulatedText;  // Accumulated recognition text
        std::mutex m_textMutex;

        // Keyword model path
        std::string m_keywordModelPath;

        // Callback functions
        TimeOutCallback m_timeOutCallback;
        UpdateTextCallback m_updateTextCallback;
        RecognizingKeyWordCallback m_recognizingKeyWordCallback;
        RecognizedKeyWordCallback m_recognizedKeyWordCallback;
        RecognizeStartCallback m_recognizeStartCallback;
        RecognizeStopCallback m_recognizeStopCallback;
        RecognizedCallback m_recognizedCallback;

        // Configuration parameters
        int m_silenceTimeoutMs = 1500;
        int m_endSilenceTimeoutMs = 3000;
    };

} // namespace SpeechRecognitionModule
