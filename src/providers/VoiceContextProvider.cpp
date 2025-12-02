#include "providers/VoiceContextProvider.h"
#include <sstream>
#include <iomanip>

bool VoiceContextProvider::initialize() {
    // Voice context is passive (updated via callbacks)
    return true;
}

void VoiceContextProvider::update() {
    // Voice context is updated via updateTranscription(), not via polling
}

void VoiceContextProvider::collectContext(pe_base::Json& context) const {
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

std::string VoiceContextProvider::getName() const {
    return "VoiceContext";
}

bool VoiceContextProvider::isAvailable() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !transcription_.empty();
}

void VoiceContextProvider::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    transcription_.clear();
    latencyMs_ = 0.0f;
}

void VoiceContextProvider::updateTranscription(const std::string& transcription, float latencyMs) {
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

std::string VoiceContextProvider::getTranscription() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transcription_;
}
