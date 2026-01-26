#pragma once

#include "providers/IContextProvider.h"
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

// Forward declaration for SpeechRecognitionModule
namespace SpeechRecognitionModule {
    class SpeechRecognition;
}

/**
 * @brief Voice Context Provider
 * 
 * Collects:
 * - Voice transcription text
 * - Voice processing latency
 * 
 * Integrates with Microsoft Azure Speech SDK via SpeechRecognitionModule
 */
class VoiceContextProvider : public IContextProvider {
public:
    /**
     * @brief Configuration for voice recognition
     */
    struct Config {
        // Online mode configuration
        std::string subscriptionKey;
        std::string serviceRegion = "eastasia";
        std::string language = "zh-CN";
        
        // Embedded mode configuration
        std::string embeddedModelPath;
        std::string embeddedModelName;
        std::string embeddedModelLicense;
        
        // Recognition mode
        bool useEmbeddedModel = true;  // true: use embedded/offline mode, false: use online mode
        bool autoStart = true;          // Automatically start recognition after initialization
        
        // Recognition parameters
        int silenceTimeoutMs = 1500;
        int endSilenceTimeoutMs = 3000;
        
        // Keyword recognition
        bool enableKeyword = false;
        std::string keywordModelPath;
    };
    
    VoiceContextProvider();
    explicit VoiceContextProvider(const Config& config);
    ~VoiceContextProvider() override;
    
    // IContextProvider interface
    bool initialize() override;
    void update() override;
    void collectContext(nlohmann::json& context) const override;
    std::string getName() const override;
    bool isAvailable() const override;
    void shutdown() override;
    
    // Configuration
    void setConfig(const Config& config);
    const Config& getConfig() const { return config_; }
    
    // Speech recognition control
    bool startRecognition();
    void stopRecognition();
    bool isRecognitionRunning() const;
    
    // Keyword recognition control
    bool startKeywordRecognition();
    void stopKeywordRecognition();
    
    // Custom methods for voice context (can still be used for external updates)
    void updateTranscription(const std::string& transcription, float latencyMs = 0.0f);
    std::string getTranscription() const;
    
    // Clear accumulated transcription
    void clearTranscription();
    
private:
    // Setup callbacks for speech recognition
    void setupCallbacks();
    
    // Callback handlers
    void onRecognized(const std::string& text);
    void onRecognizing(const std::string& text);
    void onRecognitionStarted();
    void onRecognitionStopped();
    void onKeywordRecognizing();
    void onKeywordRecognized();
    void onTimeout();
    
    // Clean up common hallucinations and artifacts
    std::string cleanTranscription(const std::string& transcription) const;
    
private:
    Config config_;
    std::unique_ptr<SpeechRecognitionModule::SpeechRecognition> speechRecognition_;
    
    mutable std::mutex mutex_;
    std::string transcription_;
    std::string currentPartialText_;  // Current partial recognition result
    float latencyMs_ = 0.0f;
    
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isRecognitionActive_{false};
};
