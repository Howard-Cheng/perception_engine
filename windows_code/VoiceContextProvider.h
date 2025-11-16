#pragma once

#include "IContextProvider.h"
#include <mutex>
#include <vector>

/**
 * @brief Voice Context Provider
 * 
 * Collects:
 * - Voice transcription text
 * - Voice processing latency
 */
class VoiceContextProvider : public IContextProvider {
public:
    VoiceContextProvider() = default;
    ~VoiceContextProvider() override = default;
    
    bool initialize() override {
        // Voice context is passive (updated via callbacks)
        return true;
    }
    
    void update() override {
        // Voice context is updated via updateTranscription(), not via polling
    }
    
    void collectContext(Json& context) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!transcription_.empty()) {
            context.set("voiceTranscription", transcription_);
        } else {
            context.setRaw("voiceTranscription", "null");
        }
        
        std::ostringstream latencyStream;
        latencyStream << std::fixed << std::setprecision(2) << latencyMs_;
        context.setRaw("voiceLatency", latencyStream.str());
    }
    
    std::string getName() const override {
        return "VoiceContext";
    }
    
    bool isAvailable() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return !transcription_.empty();
    }
    
    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);
        transcription_.clear();
        latencyMs_ = 0.0f;
    }
    
    // Custom methods for voice context
    void updateTranscription(const std::string& transcription, float latencyMs = 0.0f) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Clean up common Whisper hallucinations
        std::string cleaned = transcription;
        
        const std::vector<std::string> hallucinations = {
            "[no audio]", "[NO AUDIO]",
            "[BLANK_AUDIO]", "[blank_audio]",
            "[BLANK AUDIO]", "[blank audio]",
            "(silence)", "(Silence)", "(SILENCE)",
            "(blank)", "(Blank)", "(BLANK)",
            "[Music]", "[music]", "(Music)", "(music)",
            "[Applause]", "[applause]",
            "Thanks for watching!", "Thank you for watching!",
            "(upbeat music)", "(soft music)"
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
        
        transcription_ = cleaned;
        latencyMs_ = latencyMs;
    }
    
    std::string getTranscription() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transcription_;
    }
    
private:
    mutable std::mutex mutex_;
    std::string transcription_;
    float latencyMs_ = 0.0f;
};
