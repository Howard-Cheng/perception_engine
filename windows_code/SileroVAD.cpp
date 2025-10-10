#include "SileroVAD.h"
#include <iostream>
#include <algorithm>

SileroVAD::SileroVAD()
    : env(ORT_LOGGING_LEVEL_WARNING, "SileroVAD")
    , session(nullptr)
    , memoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , sr(SAMPLE_RATE)
{
    // Initialize state (2 layers, batch=1, hidden_size=128)
    state.resize(2 * 1 * 128, 0.0f);

    LogDebug("SileroVAD created");
}

SileroVAD::~SileroVAD() {
    session.reset();
    LogDebug("SileroVAD destroyed");
}

bool SileroVAD::Initialize(const std::wstring& modelPath) {
    try {
        LogDebug("Initializing Silero VAD...");

        // Convert wstring to string for logging
        std::string modelPathStr(modelPath.length(), 0);
        std::transform(modelPath.begin(), modelPath.end(), modelPathStr.begin(), [](wchar_t c) {
            return static_cast<char>(c);
        });
        LogDebug("Model path: " + modelPathStr);

        // Configure session options
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Create session
        session = std::make_unique<Ort::Session>(env, modelPath.c_str(), sessionOptions);

        LogDebug("Silero VAD model loaded successfully");

        // Print model info
        size_t numInputNodes = session->GetInputCount();
        size_t numOutputNodes = session->GetOutputCount();

        LogDebug("Model inputs: " + std::to_string(numInputNodes));
        LogDebug("Model outputs: " + std::to_string(numOutputNodes));

        // Print input names for debugging
        Ort::AllocatorWithDefaultOptions allocator;
        for (size_t i = 0; i < numInputNodes; i++) {
            auto input_name = session->GetInputNameAllocated(i, allocator);
            std::string name_str(input_name.get());
            LogDebug("  Input " + std::to_string(i) + ": " + name_str);
        }

        // Print output names for debugging
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto output_name = session->GetOutputNameAllocated(i, allocator);
            std::string name_str(output_name.get());
            LogDebug("  Output " + std::to_string(i) + ": " + name_str);
        }

        return true;
    }
    catch (const Ort::Exception& e) {
        LogError("ONNX Runtime error: " + std::string(e.what()));
        return false;
    }
    catch (const std::exception& e) {
        LogError("Failed to initialize Silero VAD: " + std::string(e.what()));
        return false;
    }
}

float SileroVAD::Process(const float* audioData, size_t length) {
    if (!session) {
        LogError("Silero VAD not initialized!");
        return 0.0f;
    }

    if (length != CHUNK_SIZE) {
        LogError("Audio chunk must be exactly " + std::to_string(CHUNK_SIZE) + " samples");
        return 0.0f;
    }

    try {
        // Prepare input tensors based on actual model signature:
        // Input 0: "input" - audio (1, 512)
        // Input 1: "state" - LSTM state (2, 1, 128)
        // Input 2: "sr" - sample rate (scalar)

        // Input 0: audio (1, 512)
        std::vector<int64_t> input_shape = {1, static_cast<int64_t>(CHUNK_SIZE)};
        std::vector<float> input_audio(audioData, audioData + length);

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            input_audio.data(),
            input_audio.size(),
            input_shape.data(),
            input_shape.size()
        );

        // Input 1: state (2, 1, 128)
        std::vector<int64_t> state_shape = {2, 1, 128};
        Ort::Value state_tensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            state.data(),
            state.size(),
            state_shape.data(),
            state_shape.size()
        );

        // Input 2: sr (sample rate) - scalar int64
        std::vector<int64_t> sr_shape = {1};
        std::vector<int64_t> sr_data = {sr};

        Ort::Value sr_tensor = Ort::Value::CreateTensor<int64_t>(
            memoryInfo,
            sr_data.data(),
            sr_data.size(),
            sr_shape.data(),
            sr_shape.size()
        );

        // Input/output names (order matters!)
        const char* input_names[] = {"input", "state", "sr"};
        const char* output_names[] = {"output", "stateN"};

        // Run inference
        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(std::move(input_tensor));
        input_tensors.push_back(std::move(state_tensor));
        input_tensors.push_back(std::move(sr_tensor));

        auto output_tensors = session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            input_tensors.data(),
            input_tensors.size(),
            output_names,
            2  // 2 outputs: output, stateN
        );

        // Extract output probability
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        float speech_probability = output_data[0];

        // Update state for next iteration
        float* stateN_data = output_tensors[1].GetTensorMutableData<float>();
        std::copy(stateN_data, stateN_data + state.size(), state.begin());

        return speech_probability;
    }
    catch (const Ort::Exception& e) {
        LogError("ONNX Runtime inference error: " + std::string(e.what()));
        return 0.0f;
    }
    catch (const std::exception& e) {
        LogError("Inference failed: " + std::string(e.what()));
        return 0.0f;
    }
}

void SileroVAD::Reset() {
    // Reset internal state
    std::fill(state.begin(), state.end(), 0.0f);
    LogDebug("VAD state reset");
}

void SileroVAD::LogDebug(const std::string& message) {
    std::cout << "[SileroVAD] " << message << std::endl;
}

void SileroVAD::LogError(const std::string& message) {
    std::cerr << "[SileroVAD ERROR] " << message << std::endl;
}
