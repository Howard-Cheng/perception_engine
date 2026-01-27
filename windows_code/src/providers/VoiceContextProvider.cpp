#include "providers/VoiceContextProvider.h"
#include "SpeechRecognition.h"
#include "pe_base/config_manager.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

// Static method to load config from ConfigManager
VoiceContextProvider::Config VoiceContextProvider::Config::LoadFromConfigManager() {
    Config config;
    auto& configManager = pe_base::ConfigManager::GetInstance();
    
    // Online mode configuration
    config.subscriptionKey = configManager.GetVoiceContextSubscriptionKey();
    config.serviceRegion = configManager.GetVoiceContextServiceRegion();
    config.language = configManager.GetVoiceContextLanguage();
    
    // Embedded mode configuration
    config.embeddedModelPath = configManager.GetVoiceContextEmbeddedModelPath();
    config.embeddedModelName = configManager.GetVoiceContextEmbeddedModelName();
    config.embeddedModelLicense = configManager.GetVoiceContextEmbeddedModelLicense();
    
    // Recognition mode
    config.useEmbeddedModel = configManager.GetVoiceContextUseEmbeddedModel();
    config.autoStart = configManager.GetVoiceContextAutoStart();
    
    // Recognition parameters
    config.silenceTimeoutMs = configManager.GetVoiceContextSilenceTimeoutMs();
    config.endSilenceTimeoutMs = configManager.GetVoiceContextEndSilenceTimeoutMs();
    
    // Keyword recognition
    config.enableKeyword = configManager.GetVoiceContextEnableKeyword();
    config.keywordModelPath = configManager.GetVoiceContextKeywordModelPath();
    
    return config;
}

VoiceContextProvider::VoiceContextProvider() 
    : config_(Config::LoadFromConfigManager())
    , speechRecognition_(std::make_unique<SpeechRecognitionModule::SpeechRecognition>()) {
}

VoiceContextProvider::VoiceContextProvider(const Config& config)
    : config_(config)
    , speechRecognition_(std::make_unique<SpeechRecognitionModule::SpeechRecognition>()) {
}

VoiceContextProvider::~VoiceContextProvider() {
    shutdown();
}

bool VoiceContextProvider::initialize() {
    if (isInitialized_) {
        return true;
    }
    
    if (!speechRecognition_) {
        speechRecognition_ = std::make_unique<SpeechRecognitionModule::SpeechRecognition>();
    }
    
    // Setup callbacks before initialization
    setupCallbacks();
    
    // Set recognition parameters
    speechRecognition_->SetSilenceTimeout(config_.silenceTimeoutMs);
    speechRecognition_->SetEndSilenceTimeout(config_.endSilenceTimeoutMs);
    
    // Set keyword model path if enabled
    if (config_.enableKeyword && !config_.keywordModelPath.empty()) {
        speechRecognition_->SetKeywordModelPath(config_.keywordModelPath);
    }
    
    bool initResult = false;
    
    if (config_.useEmbeddedModel) {
        // Initialize with embedded model (offline mode)
        if (!config_.embeddedModelPath.empty() && !config_.embeddedModelName.empty()) {
            initResult = speechRecognition_->InitializeWithEmbeddedModel(
                config_.embeddedModelPath,
                config_.embeddedModelName,
                config_.embeddedModelLicense
            );
        } else {
            // Use default embedded model via SpeechHelper
            initResult = speechRecognition_->Initialize();
        }
    } else {
        // Initialize with online mode
        if (!config_.subscriptionKey.empty() && !config_.serviceRegion.empty()) {
            initResult = speechRecognition_->InitializeOnline(
                config_.subscriptionKey,
                config_.serviceRegion,
                config_.language
            );
        }
    }
    
    if (!initResult) {
        return false;
    }
    
    isInitialized_ = true;
    
    // Auto start recognition if configured
    if (config_.autoStart) {
        if (config_.enableKeyword) {
            startKeywordRecognition();
        } else {
            startRecognition();
        }
    }
    
    return true;
}

void VoiceContextProvider::update() {
    // Voice context is updated via callbacks, not via polling
    // This method can be used for periodic maintenance if needed
}

void VoiceContextProvider::collectContext(nlohmann::json& context) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!transcription_.empty()) {
        context["voiceTranscription"] = transcription_;
    } else {
        context["voiceTranscription"] = nullptr;
    }
    
    // Include partial/recognizing text if available
    if (!currentPartialText_.empty()) {
        context["voicePartialText"] = currentPartialText_;
    }
    
    context["voiceLatency"] = latencyMs_;
    context["voiceRecognitionActive"] = isRecognitionActive_.load();
}

std::string VoiceContextProvider::getName() const {
    return "VoiceContext";
}

bool VoiceContextProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isInitialized_ && (!transcription_.empty() || isRecognitionActive_);
}

void VoiceContextProvider::shutdown() {
    if (speechRecognition_) {
        stopRecognition();
        stopKeywordRecognition();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    transcription_.clear();
    currentPartialText_.clear();
    latencyMs_ = 0.0f;
    isInitialized_ = false;
    isRecognitionActive_ = false;
}

void VoiceContextProvider::setConfig(const Config& config) {
    bool wasRunning = isRecognitionActive_;
    
    if (wasRunning) {
        stopRecognition();
    }
    
    config_ = config;
    
    // Update recognition parameters if already initialized
    if (speechRecognition_ && isInitialized_) {
        speechRecognition_->SetSilenceTimeout(config_.silenceTimeoutMs);
        speechRecognition_->SetEndSilenceTimeout(config_.endSilenceTimeoutMs);
        
        if (config_.enableKeyword && !config_.keywordModelPath.empty()) {
            speechRecognition_->SetKeywordModelPath(config_.keywordModelPath);
        }
    }
    
    if (wasRunning && config_.autoStart) {
        startRecognition();
    }
}

bool VoiceContextProvider::startRecognition() {
    if (!isInitialized_ || !speechRecognition_) {
        return false;
    }
    
    if (speechRecognition_->IsRunning()) {
        return true;  // Already running
    }
    
    bool result = speechRecognition_->BeginRecognizer();
    if (result) {
        isRecognitionActive_ = true;
    }
    return result;
}

void VoiceContextProvider::stopRecognition() {
    if (speechRecognition_ && speechRecognition_->IsRunning()) {
        speechRecognition_->StopRecognizer();
        isRecognitionActive_ = false;
    }
}

bool VoiceContextProvider::isRecognitionRunning() const {
    return speechRecognition_ && speechRecognition_->IsRunning();
}

bool VoiceContextProvider::startKeywordRecognition() {
    if (!isInitialized_ || !speechRecognition_) {
        return false;
    }
    
    return speechRecognition_->BeginKeyWordRecognizer();
}

void VoiceContextProvider::stopKeywordRecognition() {
    if (speechRecognition_) {
        speechRecognition_->StopKeyWordRecognizer();
    }
}

void VoiceContextProvider::updateTranscription(const std::string& transcription, float latencyMs) {
    std::string cleaned = cleanTranscription(transcription);
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cleaned.empty()) {
        if (!transcription_.empty()) {
            transcription_ += " " + cleaned;
        } else {
            transcription_ = cleaned;
        }
    }
    latencyMs_ = latencyMs;
}

std::string VoiceContextProvider::getTranscription() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transcription_;
}

void VoiceContextProvider::clearTranscription() {
    std::lock_guard<std::mutex> lock(mutex_);
    transcription_.clear();
    currentPartialText_.clear();
}

void VoiceContextProvider::setupCallbacks() {
    if (!speechRecognition_) {
        return;
    }
    
    // Set recognized callback (final recognition result)
    speechRecognition_->SetRecognizedCallback([this](const std::string& text) {
        onRecognized(text);
    });
    
    // Set update text callback (partial recognition result)
    speechRecognition_->SetUpdateTextCallback([this](const std::string& text) {
        onRecognizing(text);
    });
    
    // Set recognition start callback
    speechRecognition_->SetRecognizeStartCallback([this]() {
        onRecognitionStarted();
    });
    
    // Set recognition stop callback
    speechRecognition_->SetRecognizeStopCallback([this]() {
        onRecognitionStopped();
    });
    
    // Set keyword recognizing callback
    speechRecognition_->SetRecognizingKeyWordCallback([this]() {
        onKeywordRecognizing();
    });
    
    // Set keyword recognized callback
    speechRecognition_->SetRecognizedKeyWordCallback([this]() {
        onKeywordRecognized();
    });
    
    // Set timeout callback
    speechRecognition_->SetTimeOutCallback([this]() {
        onTimeout();
    });
}

void VoiceContextProvider::onRecognized(const std::string& text) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::string cleaned = cleanTranscription(text);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cleaned.empty()) {
        if (!transcription_.empty()) {
            transcription_ += " " + cleaned;
        } else {
            transcription_ = cleaned;
        }
    }
    std::cout << "[VoiceContextProvider] Recognized text: " << cleaned << std::endl;
    currentPartialText_.clear();
    latencyMs_ = static_cast<float>(duration.count());
}

void VoiceContextProvider::onRecognizing(const std::string& text) {
    std::string cleaned = cleanTranscription(text);
    
    std::lock_guard<std::mutex> lock(mutex_);
    currentPartialText_ = cleaned;
}

void VoiceContextProvider::onRecognitionStarted() {
    isRecognitionActive_ = true;
}

void VoiceContextProvider::onRecognitionStopped() {
    isRecognitionActive_ = false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    currentPartialText_.clear();
}

void VoiceContextProvider::onKeywordRecognizing() {
    // Keyword is being recognized - can add logging or state update here
}

void VoiceContextProvider::onKeywordRecognized() {
    // Keyword recognized - typically start full speech recognition after this
    // Auto-start continuous recognition after keyword detected
    if (config_.enableKeyword && !speechRecognition_->IsRunning()) {
        startRecognition();
    }
}

void VoiceContextProvider::onTimeout() {
    // Recognition timed out due to silence
    // Can restart or take other action here
}

std::string VoiceContextProvider::cleanTranscription(const std::string& transcription) const {
    std::string cleaned = transcription;
    
    // Clean up common Whisper/Azure hallucinations
    const std::vector<std::string> hallucinations = {
        "[no audio]", "[NO AUDIO]",
        "[BLANK_AUDIO]", "[blank_audio]",
        "[BLANK AUDIO]", "[blank audio]",
        "(silence)", "(Silence)", "(SILENCE)",
        "(blank)", "(Blank)", "(BLANK)",
        "[Music]", "[music]", "(Music)", "(music)",
        "[Applause]", "[applause]",
        "Thanks for watching!", "Thank you for watching!",
        "(upbeat music)", "(soft music)",
        "[NOISE]", "[noise]",
        "(inaudible)", "[inaudible]",
        "...", "бн"
    };
    
    for (const auto& hallucination : hallucinations) {
        size_t pos = 0;
        while ((pos = cleaned.find(hallucination, pos)) != std::string::npos) {
            cleaned.erase(pos, hallucination.length());
        }
    }
    
    // Trim whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        cleaned = cleaned.substr(start, end - start + 1);
    } else {
        cleaned = "";
    }
    
    return cleaned;
}
