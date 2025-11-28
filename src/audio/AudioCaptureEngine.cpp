#define NOMINMAX // Prevent Windows.h from defining min/max macros
#include "audio/AudioCaptureEngine.h"
#include "pe_base/logger.h" // NEW: Add Logger
#include "audio/AsyncWhisperQueue.h"
#include "audio/SileroVAD.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cmath>

// Include whisper.cpp header
#include "whisper.h"

// Link required Windows audio libraries
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

// ============================================================================
// Constructor / Destructor
// ============================================================================

AudioCaptureEngine::AudioCaptureEngine()
    : deviceEnumerator(nullptr), microphoneDevice(nullptr), systemAudioDevice(nullptr), microphoneClient(nullptr), systemAudioClient(nullptr), microphoneCaptureClient(nullptr), systemAudioCaptureClient(nullptr), whisperContext(nullptr), useSimpleVAD(true), vadThreshold(0.0001f) // Very low threshold for testing
    ,
    isRunning(false), inMeetingMode(false) // NEW: meeting mode off by default
{
    // Initialize COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    PE_INFO("AudioCaptureEngine created");
}

AudioCaptureEngine::~AudioCaptureEngine()
{
    Stop();

    // Cleanup whisper
    if (whisperContext)
    {
        whisper_free(whisperContext);
        whisperContext = nullptr;
    }

    // Cleanup WASAPI
    if (microphoneCaptureClient)
        microphoneCaptureClient->Release();
    if (systemAudioCaptureClient)
        systemAudioCaptureClient->Release();
    if (microphoneClient)
        microphoneClient->Release();
    if (systemAudioClient)
        systemAudioClient->Release();
    if (microphoneDevice)
        microphoneDevice->Release();
    if (systemAudioDevice)
        systemAudioDevice->Release();
    if (deviceEnumerator)
        deviceEnumerator->Release();

    CoUninitialize();

    PE_INFO("AudioCaptureEngine destroyed");
}

// ============================================================================
// Initialization
// ============================================================================

bool AudioCaptureEngine::Initialize(const std::string& modelPath)
{
    PE_INFO("Initializing AudioCaptureEngine...");

    // 1. Initialize Whisper
    if (!InitializeWhisper(modelPath))
    {
        PE_ERROR("Failed to initialize Whisper");
        return false;
    }

    // 2. Initialize Silero VAD
    sileroVAD = std::make_unique<SileroVAD>();
    std::wstring vadModelPath = L"models/vad/silero_vad.onnx";
    if (sileroVAD->Initialize(vadModelPath))
    {
        PE_INFO("Silero VAD initialized successfully");
        useSimpleVAD = false; // Use neural VAD
    }
    else
    {
        PE_INFO("Silero VAD failed, falling back to energy-based VAD");
        useSimpleVAD = true; // Fall back to energy-based
    }

    // 3. Initialize Microphone
    if (!InitializeMicrophoneCapture())
    {
        PE_ERROR("Failed to initialize microphone capture");
        return false;
    }

    // 4. Initialize System Audio (optional - may fail if no audio playing)
    if (!InitializeSystemAudioCapture())
    {
        PE_INFO("System audio capture not available (non-critical)");
    }

    PE_INFO("AudioCaptureEngine initialized successfully");
    return true;
}

bool AudioCaptureEngine::InitializeWhisper(const std::string& modelPath)
{
    PE_INFO_THIS("Loading Whisper model:" << modelPath.c_str())

        // Initialize whisper context parameters
        struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true; // Try GPU first

    // Load model with GPU
    whisperContext = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

    if (!whisperContext)
    {
        PE_INFO("GPU initialization failed, falling back to CPU");
        cparams.use_gpu = false;
        whisperContext = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

        if (!whisperContext)
        {
            PE_ERROR_THIS("Failed to load Whisper model from:" << modelPath.c_str())
                return false;
        }
        PE_INFO("Whisper model loaded successfully (CPU mode)");
    }
    else
    {
        PE_INFO("Whisper model loaded successfully (GPU accelerated)");
    }

    // Create async whisper queue
    try
    {
        asyncWhisperQueue = std::make_unique<AsyncWhisperQueue>(whisperContext);
        PE_INFO("Async whisper queue created");
    }
    catch (const std::exception& e)
    {
        PE_ERROR_THIS("Failed to create async whisper queue:" << e.what())
            return false;
    }

    return true;
}

bool AudioCaptureEngine::InitializeMicrophoneCapture()
{
    PE_INFO("Initializing microphone capture...");

    HRESULT hr;

    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to create device enumerator");
        return false;
    }

    // Get default microphone device
    hr = deviceEnumerator->GetDefaultAudioEndpoint(
        eCapture,
        eConsole,
        &microphoneDevice);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to get default microphone");
        return false;
    }

    // Activate audio client
    hr = microphoneDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&microphoneClient);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to activate microphone audio client");
        return false;
    }

    // Get mix format (use device's native format)
    WAVEFORMATEX* pwfx = nullptr;
    hr = microphoneClient->GetMixFormat(&pwfx);
    if (FAILED(hr))
    {
        PE_ERROR("Failed to get mix format");
        return false;
    }

    PE_INFO_THIS("Device format:" << pwfx->nSamplesPerSec << " Hz, " << pwfx->nChannels << " channels")

        // Initialize audio client with device's native format (shared mode requirement)
        REFERENCE_TIME requestedDuration = 10000000; // 1 second
    hr = microphoneClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        requestedDuration,
        0,
        pwfx,
        nullptr);

    // Store format for later conversion
    deviceFormat = *pwfx;
    CoTaskMemFree(pwfx);

    if (FAILED(hr))
    {
        PE_ERROR_THIS("Failed to initialize microphone audio client HRESULT:" << hr);
        return false;
    }

    // Get capture client
    hr = microphoneClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&microphoneCaptureClient);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to get microphone capture client");
        return false;
    }

    PE_INFO("Microphone capture initialized successfully");
    return true;
}

bool AudioCaptureEngine::InitializeSystemAudioCapture()
{
    PE_INFO("Initializing system audio capture (loopback)...");

    HRESULT hr;

    // Get default render device (speakers/headphones)
    hr = deviceEnumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &systemAudioDevice);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to get default render device");
        return false;
    }

    // Log device information
    IPropertyStore* pProps = nullptr;
    hr = systemAudioDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (SUCCEEDED(hr))
    {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (SUCCEEDED(hr))
        {
            PE_INFO_THIS("System audio loopback device:" << varName.pwszVal)
                PropVariantClear(&varName);
        }
        pProps->Release();
    }

    // Activate audio client
    hr = systemAudioDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&systemAudioClient);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to activate system audio client");
        return false;
    }

    // Get mix format
    WAVEFORMATEX* pwfx = nullptr;
    hr = systemAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr))
    {
        PE_ERROR("Failed to get system audio mix format");
        return false;
    }

    // Initialize in loopback mode
    REFERENCE_TIME requestedDuration = 10000000; // 1 second
    hr = systemAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK, // Loopback mode
        requestedDuration,
        0,
        pwfx,
        nullptr);

    CoTaskMemFree(pwfx);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to initialize system audio client in loopback mode");
        return false;
    }

    // Get capture client
    hr = systemAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&systemAudioCaptureClient);

    if (FAILED(hr))
    {
        PE_ERROR("Failed to get system audio capture client");
        return false;
    }

    PE_INFO("System audio capture initialized successfully");
    return true;
}

// ============================================================================
// Start / Stop
// ============================================================================

bool AudioCaptureEngine::Start()
{
    if (isRunning.load())
    {
        PE_INFO("AudioCaptureEngine already running");
        return true;
    }

    PE_INFO("Starting AudioCaptureEngine...");

    // Start microphone capture
    HRESULT hr = microphoneClient->Start();
    if (FAILED(hr))
    {
        PE_ERROR("Failed to start microphone capture");
        return false;
    }

    // Start system audio capture (if available)
    if (systemAudioClient)
    {
        hr = systemAudioClient->Start();
        if (FAILED(hr))
        {
            PE_INFO("Failed to start system audio capture (non-critical)");
        }
    }

    // Start threads
    isRunning.store(true);

    micThreadPtr = std::make_unique<std::thread>(&AudioCaptureEngine::MicrophoneCaptureThread, this);

    if (systemAudioClient)
    {
        systemAudioThreadPtr = std::make_unique<std::thread>(&AudioCaptureEngine::SystemAudioCaptureThread, this);
    }

    processingThreadPtr = std::make_unique<std::thread>(&AudioCaptureEngine::ProcessingThread, this);

    PE_INFO("AudioCaptureEngine started successfully");
    return true;
}

void AudioCaptureEngine::Stop()
{
    if (!isRunning.load())
    {
        return;
    }

    PE_INFO("Stopping AudioCaptureEngine...");

    // Signal threads to stop
    isRunning.store(false);

    // Wait for threads to finish
    if (micThreadPtr && micThreadPtr->joinable())
    {
        micThreadPtr->join();
    }

    if (systemAudioThreadPtr && systemAudioThreadPtr->joinable())
    {
        systemAudioThreadPtr->join();
    }

    if (processingThreadPtr && processingThreadPtr->joinable())
    {
        processingThreadPtr->join();
    }

    // Stop WASAPI clients
    if (microphoneClient)
    {
        microphoneClient->Stop();
    }

    if (systemAudioClient)
    {
        systemAudioClient->Stop();
    }

    PE_INFO("AudioCaptureEngine stopped");
}

// ============================================================================
// Capture Threads
// ============================================================================

void AudioCaptureEngine::MicrophoneCaptureThread()
{
    PE_INFO("Microphone capture thread started");

    while (isRunning.load())
    {
        UINT32 packetLength = 0;
        HRESULT hr = microphoneCaptureClient->GetNextPacketSize(&packetLength);

        if (FAILED(hr))
        {
            PE_ERROR("Failed to get packet size");
            break;
        }

        while (packetLength != 0)
        {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = microphoneCaptureClient->GetBuffer(
                &pData,
                &numFramesAvailable,
                &flags,
                nullptr,
                nullptr);

            if (FAILED(hr))
            {
                PE_ERROR("Failed to get buffer");
                break;
            }

            // Convert PCM to float and add to buffer
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
            {
                std::vector<float> audioData = ConvertPCMToFloat(pData, numFramesAvailable);
                AddMicrophoneData(audioData);
            }

            hr = microphoneCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr))
            {
                PE_ERROR("Failed to release buffer");
                break;
            }

            hr = microphoneCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr))
            {
                break;
            }
        }

        Sleep(10); // Sleep 10ms between checks
    }

    PE_INFO("Microphone capture thread stopped");
}

void AudioCaptureEngine::SystemAudioCaptureThread()
{
    PE_INFO("System audio capture thread started");

    int loopCount = 0;
    int totalPackets = 0;

    while (isRunning.load())
    {
        UINT32 packetLength = 0;
        HRESULT hr = systemAudioCaptureClient->GetNextPacketSize(&packetLength);

        if (FAILED(hr))
        {
            PE_ERROR("Failed to get system audio packet size");
            break;
        }

        // Diagnostic logging every 100 loops
        if (++loopCount % 100 == 0)
        {
            PE_DEBUG_THIS("System audio:" << loopCount << "loops, " << totalPackets << " packets captured so far")
        }

        while (packetLength != 0)
        {
            totalPackets++;
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = systemAudioCaptureClient->GetBuffer(
                &pData,
                &numFramesAvailable,
                &flags,
                nullptr,
                nullptr);

            if (FAILED(hr))
            {
                PE_ERROR("Failed to get system audio buffer");
                break;
            }

            // Convert and add to buffer
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
            {
                std::vector<float> audioData = ConvertPCMToFloat(pData, numFramesAvailable);
                AddSystemAudioData(audioData);
            }

            hr = systemAudioCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr))
            {
                PE_ERROR("Failed to release system audio buffer");
                break;
            }

            hr = systemAudioCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr))
            {
                break;
            }
        }

        Sleep(10);
    }

    PE_INFO("System audio capture thread stopped");
}

// ============================================================================
// Processing Thread (VAD + Whisper)
// ============================================================================

void AudioCaptureEngine::ProcessingThread()
{
    PE_INFO("Processing thread started with speech segmentation");

    // Speech segmentation parameters (tuned for natural conversation)
    const int VAD_WINDOW_SAMPLES = (SAMPLE_RATE * VAD_CHUNK_MS) / 1000; // 30ms windows for VAD
    const int SILENCE_THRESHOLD_MS = 1000;                              // 1000ms silence = end of utterance (prevents mid-sentence cutoff)
    const int MIN_SPEECH_MS = 400;                                      // 400ms minimum speech (filters noise, captures short words)
    const int MAX_SPEECH_SEC = 30;                                      // 30 seconds max per utterance (prevents buffer overflow)

    const int SILENCE_THRESHOLD_SAMPLES = (SAMPLE_RATE * SILENCE_THRESHOLD_MS) / 1000;
    const int MIN_SPEECH_SAMPLES = (SAMPLE_RATE * MIN_SPEECH_MS) / 1000;
    const int MAX_SPEECH_SAMPLES = SAMPLE_RATE * MAX_SPEECH_SEC;

    // State machine
    enum SpeechState
    {
        SILENCE,
        SPEAKING
    };
    SpeechState currentState = SILENCE;

    std::vector<float> speechBuffer;
    size_t silenceDurationSamples = 0;
    size_t speechDurationSamples = 0;

    PE_INFO_THIS("Speech segmentation: min=" << MIN_SPEECH_MS << " ms, pause = " << SILENCE_THRESHOLD_MS << "ms, max = " << MAX_SPEECH_SEC << "s")

        while (isRunning.load())
        {
            // Get audio buffers
            std::vector<float> micBuffer = GetMicrophoneBuffer();
            std::vector<float> audioToProcess;

            // MEETING MODE: Mix microphone + system audio (meeting participants)
            if (inMeetingMode.load())
            {
                std::vector<float> deviceBuffer = GetSystemAudioBuffer();
                audioToProcess = MixAudioSources(micBuffer, deviceBuffer);

                PE_DEBUG_THIS("Meeting mode: mixed " << micBuffer.size() << "mic + " << deviceBuffer.size() << " device samples = " << audioToProcess.size() << " total")
            }
            else
            {
                // NORMAL MODE: Just use microphone
                audioToProcess = micBuffer;
            }

            if (audioToProcess.empty())
            {
                Sleep(50);
                continue;
            }

            // Process in VAD_WINDOW_SAMPLES chunks
            if (audioToProcess.size() < VAD_WINDOW_SAMPLES)
            {
                Sleep(10);
                continue;
            }

            // Check VAD on latest window
            std::vector<float> vadWindow(
                audioToProcess.end() - VAD_WINDOW_SAMPLES,
                audioToProcess.end());
            bool isSpeech = IsSpeechDetected(vadWindow);

            if (currentState == SILENCE)
            {
                if (isSpeech)
                {
                    // Speech started!
                    PE_DEBUG("Speech STARTED");
                    currentState = SPEAKING;
                    speechBuffer.clear();
                    speechBuffer.insert(speechBuffer.end(), audioToProcess.begin(), audioToProcess.end());
                    speechDurationSamples = audioToProcess.size();
                    silenceDurationSamples = 0;

                    // Clear processed buffer
                    {
                        std::lock_guard<std::mutex> lock(micBufferMutex);
                        microphoneBuffer.clear();
                    }
                    // In meeting mode, also clear system audio buffer
                    if (inMeetingMode.load())
                    {
                        std::lock_guard<std::mutex> lock(systemAudioBufferMutex);
                        systemAudioBuffer.clear();
                    }
                }
            }
            else if (currentState == SPEAKING)
            {
                if (isSpeech)
                {
                    // Continue speaking
                    silenceDurationSamples = 0;
                    speechDurationSamples += audioToProcess.size();

                    // Append new audio to speech buffer
                    speechBuffer.insert(speechBuffer.end(), audioToProcess.begin(), audioToProcess.end());

                    // Clear processed buffer
                    {
                        std::lock_guard<std::mutex> lock(micBufferMutex);
                        microphoneBuffer.clear();
                    }
                    // In meeting mode, also clear system audio buffer
                    if (inMeetingMode.load())
                    {
                        std::lock_guard<std::mutex> lock(systemAudioBufferMutex);
                        systemAudioBuffer.clear();
                    }

                    // Safety: Max utterance length
                    if (speechBuffer.size() >= MAX_SPEECH_SAMPLES)
                    {
                        PE_DEBUG("Max utterance length reached, forcing transcription");
                        goto transcribe_now;
                    }
                }
                else
                {
                    // Silence detected during speech
                    silenceDurationSamples += audioToProcess.size();
                    speechBuffer.insert(speechBuffer.end(), audioToProcess.begin(), audioToProcess.end());

                    // Clear processed buffer
                    {
                        std::lock_guard<std::mutex> lock(micBufferMutex);
                        microphoneBuffer.clear();
                    }
                    // In meeting mode, also clear system audio buffer
                    if (inMeetingMode.load())
                    {
                        std::lock_guard<std::mutex> lock(systemAudioBufferMutex);
                        systemAudioBuffer.clear();
                    }

                    // Check if silence duration exceeded threshold
                    if (silenceDurationSamples >= SILENCE_THRESHOLD_SAMPLES)
                    {
                        PE_DEBUG_THIS("Speech ENDED (silence detected: " << silenceDurationSamples * 1000 / SAMPLE_RATE << " ms)")

                            transcribe_now:
                        // Check minimum speech duration
                        if (speechBuffer.size() >= MIN_SPEECH_SAMPLES)
                        {
                            PE_DEBUG_THIS("Queuing " << static_cast<float>(speechBuffer.size()) / SAMPLE_RATE << " s of speech for async transcription")

                                // Queue for async transcription (non-blocking!)
                                if (asyncWhisperQueue)
                                {
                                    asyncWhisperQueue->QueueAudio(speechBuffer);
                                }
                        }
                        else
                        {
                            PE_DEBUG_THIS("Speech too short (" << speechBuffer.size() * 1000 / SAMPLE_RATE << "ms), ignoring")
                        }

                        // Reset state
                        currentState = SILENCE;
                        speechBuffer.clear();
                        silenceDurationSamples = 0;
                        speechDurationSamples = 0;
                    }
                }
            }

            Sleep(10); // Check every 10ms
        }

    PE_INFO("Processing thread stopped");
}

// ============================================================================
// VAD
// ============================================================================

bool AudioCaptureEngine::IsSpeechDetected(const std::vector<float>& audioChunk)
{
    bool isSpeech = false;

    if (!useSimpleVAD && sileroVAD)
    {
        // Use Silero VAD (neural network-based)
        const size_t SILERO_CHUNK_SIZE = 512;

        // Process in 512-sample chunks
        float maxProbability = 0.0f;
        for (size_t i = 0; i + SILERO_CHUNK_SIZE <= audioChunk.size(); i += SILERO_CHUNK_SIZE)
        {
            float probability = sileroVAD->Process(&audioChunk[i], SILERO_CHUNK_SIZE);
            maxProbability = (std::max)(maxProbability, probability);
        }

        // Threshold at 0.5 for balanced detection
        isSpeech = maxProbability > 0.5f;

        // Only log significant changes
        static bool lastSpeechState = false;
        if (isSpeech != lastSpeechState)
        {
            PE_DEBUG_THIS("Silero VAD: " << (isSpeech ? "SPEECH" : "SILENCE") << " (probability: )" << maxProbability)
                lastSpeechState = isSpeech;
        }
    }
    else
    {
        // Fallback: Simple energy-based VAD
        float energy = 0.0f;
        for (float sample : audioChunk)
        {
            energy += sample * sample;
        }
        energy /= audioChunk.size();

        isSpeech = energy > vadThreshold;

        // Only log significant changes
        static bool lastEnergyState = false;
        if (isSpeech != lastEnergyState)
        {
            PE_DEBUG_THIS("Energy VAD " << (isSpeech ? "SPEECH" : "SILENCE") <<" energy:" << energy)
            lastEnergyState = isSpeech;
        }
    }

    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        metrics.isSpeechDetected = isSpeech;
    }

    return isSpeech;
}

// ============================================================================
// Whisper Transcription
// ============================================================================

std::string AudioCaptureEngine::TranscribeAudio(const std::vector<float>& audioData)
{
    if (!whisperContext)
    {
        return "";
    }

    // Set up whisper parameters
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = "en";
    params.n_threads = 4; // Use 4 CPU threads
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.translate = false;
    params.no_context = true;
    params.single_segment = false;

    // Run inference
    int result = whisper_full(
        whisperContext,
        params,
        audioData.data(),
        static_cast<int>(audioData.size()));

    if (result != 0)
    {
        PE_ERROR("Whisper transcription failed");
        return "";
    }

    // Extract text
    std::string transcription;
    int n_segments = whisper_full_n_segments(whisperContext);

    for (int i = 0; i < n_segments; ++i)
    {
        const char* text = whisper_full_get_segment_text(whisperContext, i);
        if (text)
        {
            transcription += text;
        }
    }

    // Clean up whitespace
    if (!transcription.empty())
    {
        // Trim leading/trailing whitespace
        size_t start = transcription.find_first_not_of(" \t\n\r");
        size_t end = transcription.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos)
        {
            transcription = transcription.substr(start, end - start + 1);
        }
    }

    return transcription;
}

// ============================================================================
// Buffer Management
// ============================================================================

void AudioCaptureEngine::AddMicrophoneData(const std::vector<float>& data)
{
    std::lock_guard<std::mutex> lock(micBufferMutex);

    microphoneBuffer.insert(microphoneBuffer.end(), data.begin(), data.end());

    // Prevent buffer overflow
    if (microphoneBuffer.size() > MAX_BUFFER_SAMPLES)
    {
        microphoneBuffer.erase(
            microphoneBuffer.begin(),
            microphoneBuffer.begin() + (microphoneBuffer.size() - MAX_BUFFER_SAMPLES));
    }
}

void AudioCaptureEngine::AddSystemAudioData(const std::vector<float>& data)
{
    std::lock_guard<std::mutex> lock(systemAudioBufferMutex);

    systemAudioBuffer.insert(systemAudioBuffer.end(), data.begin(), data.end());

    if (systemAudioBuffer.size() > MAX_BUFFER_SAMPLES)
    {
        systemAudioBuffer.erase(
            systemAudioBuffer.begin(),
            systemAudioBuffer.begin() + (systemAudioBuffer.size() - MAX_BUFFER_SAMPLES));
    }
}

std::vector<float> AudioCaptureEngine::GetMicrophoneBuffer()
{
    std::lock_guard<std::mutex> lock(micBufferMutex);
    return microphoneBuffer;
}

std::vector<float> AudioCaptureEngine::GetSystemAudioBuffer()
{
    std::lock_guard<std::mutex> lock(systemAudioBufferMutex);
    return systemAudioBuffer;
}

// ============================================================================
// Public Getters
// ============================================================================

std::string AudioCaptureEngine::GetLatestUserSpeech()
{
    // Get result from async queue
    if (asyncWhisperQueue)
    {
        std::string result = asyncWhisperQueue->GetLatestResult();
        if (!result.empty())
        {
            // Cache for legacy support
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                latestUserSpeech = result;
            }

            // Trigger callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex);
                if (transcriptionCallback)
                {
                    transcriptionCallback(result);
                }
            }

            // Store in meeting transcript if in meeting mode
            if (inMeetingMode.load())
            {
                TranscriptSegment segment;
                segment.timestamp = std::chrono::system_clock::now();
                segment.text = result;
                segment.confidence = 1.0f; // Whisper doesn't provide confidence, default to 1.0

                std::lock_guard<std::mutex> lock(meetingTranscriptMutex);
                meetingTranscriptSegments.push_back(segment);

                PE_DEBUG_THIS("Meeting transcript segment added: " << result.c_str() << " (total segments : % zu)" << meetingTranscriptSegments.size())
            }

            return result;
        }
    }

    // Fallback to cached result
    std::lock_guard<std::mutex> lock(resultsMutex);
    return latestUserSpeech;
}

void AudioCaptureEngine::SetTranscriptionCallback(TranscriptionCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    transcriptionCallback = callback;
}

std::string AudioCaptureEngine::GetLatestSystemAudio()
{
    std::lock_guard<std::mutex> lock(resultsMutex);
    return latestSystemAudio;
}

AudioCaptureEngine::PerformanceMetrics AudioCaptureEngine::GetMetrics() const
{
    std::lock_guard<std::mutex> lock(metricsMutex);
    return metrics;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::vector<float> AudioCaptureEngine::ConvertPCMToFloat(const BYTE* pcmData, UINT32 numFrames)
{
    // Handle different audio formats
    std::vector<float> floatData;

    // Check if format is float32 (common for WASAPI)
    if (deviceFormat.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        deviceFormat.wBitsPerSample == 32)
    {
        // Already float format
        const float* samples = reinterpret_cast<const float*>(pcmData);

        // Convert to mono if needed (average channels)
        if (deviceFormat.nChannels == 1)
        {
            floatData.assign(samples, samples + numFrames);
        }
        else
        {
            // Convert stereo/multi-channel to mono by averaging
            floatData.reserve(numFrames);
            for (UINT32 i = 0; i < numFrames; ++i)
            {
                float mono = 0.0f;
                for (UINT32 ch = 0; ch < deviceFormat.nChannels; ++ch)
                {
                    mono += samples[i * deviceFormat.nChannels + ch];
                }
                floatData.push_back(mono / deviceFormat.nChannels);
            }
        }
    }
    else if (deviceFormat.wBitsPerSample == 16)
    {
        // 16-bit PCM format
        const int16_t* samples = reinterpret_cast<const int16_t*>(pcmData);

        // Convert to mono if needed
        if (deviceFormat.nChannels == 1)
        {
            floatData.reserve(numFrames);
            for (UINT32 i = 0; i < numFrames; ++i)
            {
                floatData.push_back(samples[i] / 32768.0f);
            }
        }
        else
        {
            // Convert stereo/multi-channel to mono
            floatData.reserve(numFrames);
            for (UINT32 i = 0; i < numFrames; ++i)
            {
                float mono = 0.0f;
                for (UINT32 ch = 0; ch < deviceFormat.nChannels; ++ch)
                {
                    mono += samples[i * deviceFormat.nChannels + ch] / 32768.0f;
                }
                floatData.push_back(mono / deviceFormat.nChannels);
            }
        }
    }

    // Resample to 16kHz if needed
    if (deviceFormat.nSamplesPerSec != SAMPLE_RATE)
    {
        floatData = ResampleAudio(floatData, deviceFormat.nSamplesPerSec, SAMPLE_RATE);
    }

    return floatData;
}

std::vector<float> AudioCaptureEngine::ResampleAudio(const std::vector<float>& input,
    int inputSampleRate,
    int outputSampleRate)
{
    if (inputSampleRate == outputSampleRate)
    {
        return input;
    }

    // Simple linear resampling
    float ratio = static_cast<float>(inputSampleRate) / outputSampleRate;
    size_t outputSize = static_cast<size_t>(input.size() / ratio);
    std::vector<float> output;
    output.reserve(outputSize);

    for (size_t i = 0; i < outputSize; ++i)
    {
        float srcIndex = i * ratio;
        size_t index0 = static_cast<size_t>(srcIndex);
        size_t index1 = (index0 + 1 < input.size()) ? index0 + 1 : input.size() - 1;
        float fraction = srcIndex - index0;

        // Linear interpolation
        float sample = input[index0] * (1.0f - fraction) + input[index1] * fraction;
        output.push_back(sample);
    }

    return output;
}

// std::vector<float> AudioCaptureEngine::MixAudioSources(const std::vector<float>& mic, const std::vector<float>& device) {
//     // Mix microphone and system audio by averaging samples
//     // This is used in meeting mode to capture both the user and other participants

//     size_t minSize = std::min(mic.size(), device.size());
//     std::vector<float> mixed;
//     mixed.reserve(minSize);

//     for (size_t i = 0; i < minSize; ++i) {
//         // Average the two sources to prevent clipping
//         mixed.push_back((mic[i] + device[i]) * 0.5f);
//     }

//     return mixed;
// }

// Mix two audio sources (microphone + system audio)
std::vector<float> AudioCaptureEngine::MixAudioSources(const std::vector<float>& mic, const std::vector<float>& device)
{
    // Use the longer buffer's size
    size_t maxSize = (mic.size() > device.size()) ? mic.size() : device.size();
    std::vector<float> mixed;
    mixed.reserve(maxSize);

    for (size_t i = 0; i < maxSize; ++i)
    {
        float micSample = (i < mic.size()) ? mic[i] : 0.0f;
        float deviceSample = (i < device.size()) ? device[i] : 0.0f;

        // Mix with equal weight (0.5 + 0.5 = 1.0)
        // Apply soft clipping to prevent overflow
        float mixed_sample = (micSample * 0.5f) + (deviceSample * 0.5f);

        // Clamp to [-1.0, 1.0] range
        if (mixed_sample > 1.0f)
            mixed_sample = 1.0f;
        if (mixed_sample < -1.0f)
            mixed_sample = -1.0f;

        mixed.push_back(mixed_sample);
    }

    return mixed;
}

// Delete the old LogDebug and LogError functions (they are now replaced by Logger)
