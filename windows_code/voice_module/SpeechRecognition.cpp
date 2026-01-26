//
// SpeechRecognition.cpp
// Real-time Speech Recognition Module Implementation - Based on Microsoft Azure Speech SDK
//

#include "SpeechRecognition.h"
#include "SpeechHelper.h"
#include <speechapi_cxx.h>
#include <iostream>

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Audio;

namespace SpeechRecognitionModule
{

SpeechRecognition::SpeechRecognition()
    : m_state(SpeechStateEnum::WaitInitialize)
    , m_isRunning(false)
    , m_silenceTimeoutMs(1500)
    , m_endSilenceTimeoutMs(3000)
{
}

SpeechRecognition::~SpeechRecognition()
{
    StopRecognizer();
    m_recognizer.reset();
    m_keywordModel.reset();
}

bool SpeechRecognition::Initialize()
{
    m_state = SpeechStateEnum::Initializing;
    
    try
    {
        m_isRunning = false;

        // Create config using SpeechHelper
        auto speechConfig = SpeechHelper::CreateSpeechConfig();
        if (!speechConfig)
        {
            std::cerr << "Failed to create speech config from SpeechHelper" << std::endl;
            m_state = SpeechStateEnum::InitializeFail;
            return false;
        }

        // Set properties
        speechConfig->SetProperty(PropertyId::SpeechServiceResponse_PostProcessingOption, "True");
        speechConfig->SetProperty(PropertyId::Speech_SegmentationSilenceTimeoutMs, std::to_string(m_silenceTimeoutMs));
        speechConfig->SetProperty(PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs, std::to_string(m_endSilenceTimeoutMs));

        // Create audio config - use default microphone input
        // Note: Embedded speech SDK does not support AudioProcessingOptions, use simple default microphone input
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        // Create speech recognizer
        m_recognizer = SpeechRecognizer::FromConfig(speechConfig, audioConfig);

        // Setup event handlers
        SetupEventHandlers();

        m_state = SpeechStateEnum::InitializeSuccess;
        std::cout << "SpeechRecognition initialized successfully" << std::endl;
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition Initialize Exception: " << ex.what() << std::endl;
        m_state = SpeechStateEnum::InitializeFail;
        return false;
    }
}

bool SpeechRecognition::InitializeOnline(const std::string& subscriptionKey,
                                          const std::string& serviceRegion,
                                          const std::string& language)
{
    m_state = SpeechStateEnum::Initializing;
    
    try
    {
        m_isRunning = false;

        // Create speech config
        auto speechConfig = CreateSpeechConfig(subscriptionKey, serviceRegion, language);
        if (!speechConfig)
        {
            std::cerr << "Failed to create speech config" << std::endl;
            m_state = SpeechStateEnum::InitializeFail;
            return false;
        }

        // Set properties
        speechConfig->SetProperty(PropertyId::SpeechServiceResponse_PostProcessingOption, "True");
        speechConfig->SetProperty(PropertyId::Speech_SegmentationSilenceTimeoutMs, std::to_string(m_silenceTimeoutMs));
        speechConfig->SetProperty(PropertyId::SpeechServiceConnection_EndSilenceTimeoutMs, std::to_string(m_endSilenceTimeoutMs));

        // Create audio config - use default microphone input
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        // Create speech recognizer
        m_recognizer = SpeechRecognizer::FromConfig(speechConfig, audioConfig);

        // Setup event handlers
        SetupEventHandlers();

        m_state = SpeechStateEnum::InitializeSuccess;
        std::cout << "SpeechRecognition (Online) initialized successfully" << std::endl;
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition InitializeOnline Exception: " << ex.what() << std::endl;
        m_state = SpeechStateEnum::InitializeFail;
        return false;
    }
}

bool SpeechRecognition::InitializeWithEmbeddedModel(const std::string& modelPath,
                                                     const std::string& modelName,
                                                     const std::string& modelLicense)
{
    m_state = SpeechStateEnum::Initializing;

    try
    {
        m_isRunning = false;

        // Create embedded speech config
        auto embeddedConfig = CreateEmbeddedSpeechConfig(modelPath, modelName, modelLicense);
        if (!embeddedConfig)
        {
            std::cerr << "Failed to create embedded speech config" << std::endl;
            m_state = SpeechStateEnum::InitializeFail;
            return false;
        }

        // Set properties
        embeddedConfig->SetProperty(PropertyId::SpeechServiceResponse_PostProcessingOption, "True");
        embeddedConfig->SetProperty(PropertyId::Speech_SegmentationSilenceTimeoutMs, std::to_string(m_silenceTimeoutMs));

        // Create audio config - use default microphone input
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        // Create speech recognizer
        m_recognizer = SpeechRecognizer::FromConfig(embeddedConfig, audioConfig);

        // Setup event handlers
        SetupEventHandlers();

        m_state = SpeechStateEnum::InitializeSuccess;
        std::cout << "SpeechRecognition (Embedded) initialized successfully" << std::endl;
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition InitializeWithEmbeddedModel Exception: " << ex.what() << std::endl;
        m_state = SpeechStateEnum::InitializeFail;
        return false;
    }
}

std::shared_ptr<SpeechConfig> SpeechRecognition::CreateSpeechConfig(
    const std::string& subscriptionKey,
    const std::string& serviceRegion,
    const std::string& language)
{
    try
    {
        // Create speech config
        std::string endpoint = "https://" + serviceRegion + ".api.cognitive.microsoft.com";
        auto config = SpeechConfig::FromEndpoint(endpoint, subscriptionKey);
        
        // Set recognition language
        config->SetSpeechRecognitionLanguage(language);

        return config;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "CreateSpeechConfig Exception: " << ex.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<EmbeddedSpeechConfig> SpeechRecognition::CreateEmbeddedSpeechConfig(
    const std::string& modelPath,
    const std::string& modelName,
    const std::string& modelLicense)
{
    try
    {
        // Create embedded speech config from path
        auto config = EmbeddedSpeechConfig::FromPath(modelPath);

        // Get available speech recognition models
        auto models = config->GetSpeechRecognitionModels();
        if (models.empty())
        {
            std::cerr << "No embedded speech recognition models found in path: " << modelPath << std::endl;
            return nullptr;
        }

        // Find specified model or use first available model
        std::shared_ptr<SpeechRecognitionModel> targetModel = nullptr;
        for (const auto& model : models)
        {
            if (model->Name == modelName || 
                (!model->Locales.empty() && model->Locales[0] == modelName))
            {
                targetModel = model;
                break;
            }
        }

        if (!targetModel && !models.empty())
        {
            targetModel = models[0];
            std::cout << "Using default model: " << targetModel->Name << std::endl;
        }

        if (targetModel)
        {
            std::string license = modelLicense.empty() ? SpeechHelper::SpeechModelLicense : modelLicense;
            config->SetSpeechRecognitionModel(targetModel->Name, license);
        }

        return config;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "CreateEmbeddedSpeechConfig Exception: " << ex.what() << std::endl;
        return nullptr;
    }
}

void SpeechRecognition::SetupEventHandlers()
{
    if (!m_recognizer)
    {
        return;
    }

    // Recognizing event - recognition in progress (intermediate results)
    m_recognizer->Recognizing.Connect([this](const SpeechRecognitionEventArgs& e)
    {
        //std::cout << "SessionId: " << e.SessionId << std::endl;
        
        if (e.Result->Reason == ResultReason::RecognizingKeyword)
        {
            std::cout << "Recognizing Keyword..." << std::endl;
            if (m_recognizingKeyWordCallback)
            {
                m_recognizingKeyWordCallback();
            }
        }
        else if (e.Result->Reason == ResultReason::RecognizingSpeech)
        {
            std::string currentText;
            {
                std::lock_guard<std::mutex> lock(m_textMutex);
                currentText = m_accumulatedText + e.Result->Text;
            }
            
            //std::cout << "Recognizing: " << e.Result->Text << std::endl;
            
            if (m_updateTextCallback)
            {
                m_updateTextCallback(currentText);
            }
        }
    });

    // Recognized event - recognition completed (final results)
    m_recognizer->Recognized.Connect([this](const SpeechRecognitionEventArgs& e)
    {
        if (e.Result->Reason == ResultReason::RecognizedKeyword)
        {
            std::cout << "Recognized Keyword" << std::endl;
            if (m_recognizedKeyWordCallback)
            {
                m_recognizedKeyWordCallback();
            }
        }
        else if (e.Result->Reason == ResultReason::RecognizedSpeech)
        {
            std::string finalText;
            {
                std::lock_guard<std::mutex> lock(m_textMutex);
                m_accumulatedText += e.Result->Text;
                finalText = m_accumulatedText;
            }
            
            std::cout << "RECOGNIZED: Text=" << e.Result->Text << std::endl;
            
            if (m_updateTextCallback)
            {
                m_updateTextCallback(finalText);
            }
            
            if (m_recognizedCallback)
            {
                m_recognizedCallback(finalText);
            }
            
            // Clear accumulated text
            {
                std::lock_guard<std::mutex> lock(m_textMutex);
                m_accumulatedText.clear();
            }
        }
        else if (e.Result->Reason == ResultReason::NoMatch)
        {
            auto noMatchDetails = NoMatchDetails::FromResult(e.Result);
            std::cout << "NO MATCH: Reason=" << static_cast<int>(noMatchDetails->Reason) << std::endl;
            
            if (noMatchDetails->Reason == NoMatchReason::InitialSilenceTimeout)
            {
                std::cout << "Timeout" << std::endl;
                if (m_timeOutCallback)
                {
                    m_timeOutCallback();
                }
            }
        }
    });

    // SessionStarted event - session started
    m_recognizer->SessionStarted.Connect([this](const SessionEventArgs& e)
    {
        std::cout << "SpeechRecognition SessionStarted begin." << std::endl;
        m_isRunning = true;
        
        if (m_recognizeStartCallback)
        {
            m_recognizeStartCallback();
        }
        
        std::cout << "SpeechRecognition SessionStarted end." << std::endl;
    });

    // SessionStopped event - session stopped
    m_recognizer->SessionStopped.Connect([this](const SessionEventArgs& e)
    {
        std::cout << "SpeechRecognition SessionStopped begin." << std::endl;
        m_isRunning = false;
        
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_accumulatedText.clear();
        }
        
        if (m_recognizeStopCallback)
        {
            m_recognizeStopCallback();
        }
        
        std::cout << "SpeechRecognition SessionStopped end." << std::endl;
    });

    // Canceled event - recognition canceled
    m_recognizer->Canceled.Connect([this](const SpeechRecognitionCanceledEventArgs& e)
    {
        std::cout << "CANCELED: Reason=" << static_cast<int>(e.Reason) << std::endl;
        
        if (e.Reason == CancellationReason::Error)
        {
            std::cerr << "CANCELED: ErrorCode=" << static_cast<int>(e.ErrorCode) << std::endl;
            std::cerr << "CANCELED: ErrorDetails=" << e.ErrorDetails << std::endl;
        }
    });
}

bool SpeechRecognition::BeginRecognizer()
{
    std::cout << "SpeechRecognition BeginRecognizer IsRunning is " << (m_isRunning ? "true" : "false") << std::endl;
    
    if (m_isRunning)
    {
        return true;
    }

    bool result = false;
    try
    {
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_accumulatedText.clear();
        }

        if (m_state != SpeechStateEnum::InitializeSuccess)
        {
            std::cerr << "SpeechRecognition not initialized" << std::endl;
            return false;
        }

        // Start continuous recognition (non-blocking)
        m_recognizer->StartContinuousRecognitionAsync();
        result = true;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition BeginRecognizer Exception: " << ex.what() << std::endl;
        result = false;
    }
    
    std::cout << "SpeechRecognition result is " << (result ? "true" : "false") << std::endl;
    return result;
}

void SpeechRecognition::StopRecognizer()
{
    std::cout << "SpeechRecognition StopRecognizer IsRunning is " << (m_isRunning ? "true" : "false") << std::endl;
    
    if (!m_isRunning)
    {
        return;
    }

    m_isRunning = false;

    try
    {
        if (m_recognizer)
        {
            m_recognizer->StopContinuousRecognitionAsync().get();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition StopRecognizer Exception: " << ex.what() << std::endl;
    }
    
    std::cout << "SpeechRecognition StopRecognizer End" << std::endl;
}

bool SpeechRecognition::BeginKeyWordRecognizer()
{
    bool result = false;
    try
    {
        std::cout << "BeginKeyWordRecognizer begin." << std::endl;
        
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_accumulatedText.clear();
        }

        if (m_state != SpeechStateEnum::InitializeSuccess)
        {
            std::cerr << "SpeechRecognition not initialized" << std::endl;
            return false;
        }

        if (m_keywordModel)
        {
            m_recognizer->StartKeywordRecognitionAsync(m_keywordModel).get();
            result = true;
        }
        else
        {
            std::cerr << "Keyword model not loaded" << std::endl;
            result = false;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition BeginKeyWordRecognizer Exception: " << ex.what() << std::endl;
        result = false;
    }
    
    std::cout << "BeginKeyWordRecognizer end." << std::endl;
    return result;
}

bool SpeechRecognition::StartKeywordRecognition(const std::string& keywordModelPath)
{
    try
    {
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_accumulatedText.clear();
        }

        if (m_state != SpeechStateEnum::InitializeSuccess)
        {
            std::cerr << "SpeechRecognition not initialized" << std::endl;
            return false;
        }

        // Load keyword model
        m_keywordModel = KeywordRecognitionModel::FromFile(keywordModelPath);
        
        if (m_keywordModel)
        {
            m_recognizer->StartKeywordRecognitionAsync(m_keywordModel).get();
            std::cout << "Keyword Recognition Started" << std::endl;
            return true;
        }
        
        return false;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "StartKeywordRecognition Exception: " << ex.what() << std::endl;
        return false;
    }
}

void SpeechRecognition::StopKeyWordRecognizer()
{
    try
    {
        if (m_recognizer)
        {
            m_recognizer->StopKeywordRecognitionAsync().get();
            std::cout << "Keyword Recognition Stopped" << std::endl;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "SpeechRecognition StopKeyWordRecognizer Exception: " << ex.what() << std::endl;
    }
}

} // namespace SpeechRecognitionModule
