#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>

// Forward declarations for whisper.cpp
struct whisper_context;

// Forward declaration for async queue
class AsyncWhisperQueue;

// Forward declaration for Silero VAD
class SileroVAD;

// Callback type for transcription results
using TranscriptionCallback = std::function<void(const std::string& transcription)>;

/**
 * AudioCaptureEngine - Real-time audio capture and transcription
 *
 * Features:
 * - WASAPI microphone capture (user speech)
 * - WASAPI system audio loopback (device playback)
 * - Voice Activity Detection (VAD) for filtering
 * - whisper.cpp CPU inference for transcription
 *
 * Architecture:
 * - Capture thread: Records audio continuously
 * - Processing thread: VAD -> Whisper pipeline
 * - Thread-safe result queue
 */
class AudioCaptureEngine {
public:
    AudioCaptureEngine();
    ~AudioCaptureEngine();

    // Initialize audio capture and whisper model
    bool Initialize(const std::string& modelPath);

    // Start/stop audio capture and processing
    bool Start();
    void Stop();

    // Get latest transcription results
    std::string GetLatestUserSpeech();
    std::string GetLatestSystemAudio();

    // Check if engine is running
    bool IsRunning() const { return isRunning.load(); }

    // Set callback for transcription results
    void SetTranscriptionCallback(TranscriptionCallback callback);

    // Performance metrics
    struct PerformanceMetrics {
        float captureLatencyMs;
        float vadLatencyMs;
        float whisperLatencyMs;
        float totalLatencyMs;
        int audioBufferSize;
        bool isSpeechDetected;
    };

    PerformanceMetrics GetMetrics() const;

private:
    // === WASAPI Audio Capture ===
    bool InitializeMicrophoneCapture();
    bool InitializeSystemAudioCapture();
    void MicrophoneCaptureThread();
    void SystemAudioCaptureThread();

    // === VAD Processing ===
    bool InitializeVAD(const std::string& vadModelPath);
    bool IsSpeechDetected(const std::vector<float>& audioChunk);

    // === Whisper Transcription ===
    bool InitializeWhisper(const std::string& modelPath);
    std::string TranscribeAudio(const std::vector<float>& audioData);
    void ProcessingThread();

    // === Audio Buffer Management ===
    void AddMicrophoneData(const std::vector<float>& data);
    void AddSystemAudioData(const std::vector<float>& data);
    std::vector<float> GetMicrophoneBuffer();
    std::vector<float> GetSystemAudioBuffer();

    // === WASAPI Members ===
    IMMDeviceEnumerator* deviceEnumerator;
    IMMDevice* microphoneDevice;
    IMMDevice* systemAudioDevice;
    IAudioClient* microphoneClient;
    IAudioClient* systemAudioClient;
    IAudioCaptureClient* microphoneCaptureClient;
    IAudioCaptureClient* systemAudioCaptureClient;
    WAVEFORMATEX deviceFormat;

    // === Whisper.cpp ===
    whisper_context* whisperContext;
    std::unique_ptr<AsyncWhisperQueue> asyncWhisperQueue;

    // === VAD ===
    std::unique_ptr<SileroVAD> sileroVAD;
    bool useSimpleVAD;  // Fallback if Silero fails
    float vadThreshold;

    // === Threading ===
    std::atomic<bool> isRunning;
    std::unique_ptr<std::thread> micThreadPtr;
    std::unique_ptr<std::thread> systemAudioThreadPtr;
    std::unique_ptr<std::thread> processingThreadPtr;

    // === Audio Buffers (Thread-Safe) ===
    std::mutex micBufferMutex;
    std::vector<float> microphoneBuffer;
    const size_t MAX_BUFFER_SAMPLES = 16000 * 30; // 30 seconds @ 16kHz

    std::mutex systemAudioBufferMutex;
    std::vector<float> systemAudioBuffer;

    // === Transcription Results (Thread-Safe) ===
    std::mutex resultsMutex;
    std::string latestUserSpeech;
    std::string latestSystemAudio;

    // === Transcription Callback ===
    TranscriptionCallback transcriptionCallback;
    std::mutex callbackMutex;

    // === Performance Metrics ===
    mutable std::mutex metricsMutex;
    PerformanceMetrics metrics;

    // === Configuration ===
    const int SAMPLE_RATE = 16000;      // Whisper expects 16kHz
    const int CHANNELS = 1;             // Mono
    const int BITS_PER_SAMPLE = 16;     // 16-bit PCM
    const int VAD_CHUNK_MS = 32;        // 32ms chunks for VAD (512 samples for Silero)
    const int WHISPER_CHUNK_SEC = 3;    // Process 3-second chunks (for testing)

    // === Helper Functions ===
    void LogDebug(const std::string& message);
    void LogError(const std::string& message);
    std::vector<float> ConvertPCMToFloat(const BYTE* pcmData, UINT32 numFrames);
    std::vector<float> ResampleAudio(const std::vector<float>& input, int inputSampleRate, int outputSampleRate);
};
