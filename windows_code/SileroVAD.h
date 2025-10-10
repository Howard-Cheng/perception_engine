#pragma once

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>

/**
 * SileroVAD - Neural network-based Voice Activity Detection
 *
 * Uses Silero VAD ONNX model for robust speech detection.
 * 95%+ accuracy vs 60-70% for energy-based VAD.
 *
 * Features:
 * - Distinguishes speech from keyboard clicks, breathing, background music
 * - Low latency: processes 512 samples (32ms @ 16kHz)
 * - Stateful: maintains context across chunks
 *
 * Usage:
 *   SileroVAD vad;
 *   vad.Initialize(L"models/vad/silero_vad.onnx");
 *
 *   // Process 512-sample chunks
 *   float probability = vad.Process(audioChunk);
 *   if (probability > 0.5f) {
 *       // Speech detected!
 *   }
 */
class SileroVAD {
public:
    SileroVAD();
    ~SileroVAD();

    // Initialize with ONNX model path
    bool Initialize(const std::wstring& modelPath);

    // Process audio chunk and return speech probability [0.0-1.0]
    // Audio must be 512 samples @ 16kHz, float32
    float Process(const float* audioData, size_t length);

    // Reset internal state (call between utterances)
    void Reset();

    // Check if initialized
    bool IsInitialized() const { return session != nullptr; }

private:
    // ONNX Runtime
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions sessionOptions;
    Ort::MemoryInfo memoryInfo;

    // Model expects 512 samples @ 16kHz
    static constexpr size_t CHUNK_SIZE = 512;
    static constexpr size_t SAMPLE_RATE = 16000;

    // Internal state (Silero VAD is stateful)
    std::vector<float> state;  // Combined LSTM state (2, 1, 128)
    int64_t sr;                // Sample rate

    // Helper functions
    void LogDebug(const std::string& message);
    void LogError(const std::string& message);
};
