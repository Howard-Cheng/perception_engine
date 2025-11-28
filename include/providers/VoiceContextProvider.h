#pragma once

#include "providers/IContextProvider.h"
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
    
    bool initialize() override;
    void update() override;
    void collectContext(pe_base::Json& context) const override;
    std::string getName() const override;
    bool isAvailable() const override;
    void shutdown() override;
    
    // Custom methods for voice context
    void updateTranscription(const std::string& transcription, float latencyMs = 0.0f);
    std::string getTranscription() const;
    
private:
    mutable std::mutex mutex_;
    std::string transcription_;
    float latencyMs_ = 0.0f;
};
