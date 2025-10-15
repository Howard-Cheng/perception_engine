#include "CameraVisionEngine.h"
#include "Logger.h"  // NEW: Add Logger
#include "FastVLMTokenizer.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <windows.h>

CameraVisionEngine::CameraVisionEngine()
    : isInitialized(false), lastLatencyMs(0.0f) {
}

CameraVisionEngine::~CameraVisionEngine() {
    if (camera.isOpened()) {
        camera.release();
    }
}

bool CameraVisionEngine::Initialize(const std::string& modelPath, int cameraIndex) {
    try {
        LOG_INFO("Initializing CameraVisionEngine...");

        // Initialize ONNX Runtime environment
        ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "CameraVisionEngine");

        sessionOptions = std::make_unique<Ort::SessionOptions>();
        sessionOptions->SetIntraOpNumThreads(4);  // 4 threads for CPU inference
        sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

        memoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)
        );

        // Load ONNX models
        LOG_INFO("Loading vision encoder...");
        std::string visionPath = modelPath + "/onnx/vision_encoder_simplified.onnx";
        visionEncoder = LoadOnnxModel(visionPath);
        if (!visionEncoder) {
            LOG_ERROR("Failed to load vision encoder");
            return false;
        }

        LOG_INFO("Loading embed tokens model...");
        std::string embedPath = modelPath + "/onnx/embed_tokens_q4f16.onnx";
        embedTokens = LoadOnnxModel(embedPath);
        if (!embedTokens) {
            LOG_ERROR("Failed to load embed tokens model");
            return false;
        }

        LOG_INFO("Loading decoder model...");
        std::string decoderPath = modelPath + "/onnx/decoder_model_merged_q4f16.onnx";
        decoder = LoadOnnxModel(decoderPath);
        if (!decoder) {
            LOG_ERROR("Failed to load decoder model");
            return false;
        }

        // Initialize camera
        LOG_INFO_FMT("Opening camera %d...", cameraIndex);
        camera.open(cameraIndex);
        if (!camera.isOpened()) {
            LOG_ERROR("Failed to open camera");
            return false;
        }

        // Set camera resolution (smaller = faster)
        camera.set(cv::CAP_PROP_FRAME_WIDTH, 320);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, 240);

        isInitialized = true;
        LOG_INFO("Initialization complete!");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Initialization error: %s", e.what());
        return false;
    }
}

std::unique_ptr<Ort::Session> CameraVisionEngine::LoadOnnxModel(const std::string& modelPath) {
    try {
        LOG_INFO_FMT("Loading model from: %s", modelPath.c_str());

        // Convert UTF-8 string to wide string for Windows
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0);
        if (wideSize == 0) {
            LOG_ERROR_FMT("Failed to convert path: %s", modelPath.c_str());
            return nullptr;
        }

        std::wstring wModelPath(wideSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, &wModelPath[0], wideSize);

        LOG_DEBUG("Attempting to load ONNX session...");
        auto session = std::make_unique<Ort::Session>(*ortEnv, wModelPath.c_str(), *sessionOptions);
        LOG_INFO("Model loaded successfully!");
        return session;
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Error loading model %s: %s", modelPath.c_str(), e.what());
        return nullptr;
    }
}

void CameraVisionEngine::PreprocessImage(const cv::Mat& frame, std::vector<float>& output) {
    // Resize to 224x224 (FastVLM input size)
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(224, 224));

    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    // Convert to float and normalize to [0, 1]
    cv::Mat floatImage;
    rgb.convertTo(floatImage, CV_32F, 1.0 / 255.0);

    // ImageNet normalization: mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]
    std::vector<float> mean = {0.485f, 0.456f, 0.406f};
    std::vector<float> std = {0.229f, 0.224f, 0.225f};

    // Convert to CHW format (channels, height, width) and normalize
    output.resize(3 * 224 * 224);

    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < 224; ++h) {
            for (int w = 0; w < 224; ++w) {
                float pixel = floatImage.at<cv::Vec3f>(h, w)[c];
                pixel = (pixel - mean[c]) / std[c];
                output[c * 224 * 224 + h * 224 + w] = pixel;
            }
        }
    }
}

std::vector<float> CameraVisionEngine::RunVisionEncoder(const std::vector<float>& imageData) {
    try {
        LOG_DEBUG_FMT("Vision encoder input data size: %zu", imageData.size());

        // Input shape: [1, 3, 224, 224]
        std::vector<int64_t> inputShape = {1, 3, 224, 224};

        // Verify data size matches shape
        size_t expectedSize = 1 * 3 * 224 * 224;
        if (imageData.size() != expectedSize) {
            LOG_ERROR_FMT("Image data size mismatch! Got %zu, expected %zu", 
                         imageData.size(), expectedSize);
            return {};
        }

        LOG_DEBUG("Creating input tensor with shape [1, 3, 224, 224]");

        // Create input tensor
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *memoryInfo,
            const_cast<float*>(imageData.data()),
            imageData.size(),
            inputShape.data(),
            inputShape.size()
        );

        LOG_DEBUG("Input tensor created, running inference...");

        // Run inference
        const char* inputNames[] = {"pixel_values"};
        const char* outputNames[] = {"image_features"};

        auto outputTensors = visionEncoder->Run(
            Ort::RunOptions{nullptr},
            inputNames,
            &inputTensor,
            1,
            outputNames,
            1
        );

        // Get output (shape: [1, 16, 896])
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto tensorInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        size_t outputSize = tensorInfo.GetElementCount();

        std::vector<float> imageFeatures(outputData, outputData + outputSize);
        return imageFeatures;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Vision encoder error: %s", e.what());
        return {};
    }
}

std::string CameraVisionEngine::DescribeScene() {
    if (!isInitialized) {
        LOG_ERROR("Engine not initialized");
        return "";
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Step 1: Capture frame
        cv::Mat frame;
        if (!camera.read(frame) || frame.empty()) {
            LOG_ERROR("Failed to capture frame");
            return "";
        }

        // Validate frame dimensions
        if (frame.rows == 0 || frame.cols == 0) {
            LOG_ERROR_FMT("Invalid frame dimensions: %dx%d", frame.rows, frame.cols);
            return "";
        }

        LOG_DEBUG_FMT("Captured frame: %dx%d", frame.cols, frame.rows);

        // Step 2: Preprocess image
        std::vector<float> imageData;
        PreprocessImage(frame, imageData);

        LOG_DEBUG_FMT("Preprocessed image data size: %zu (expected: %d)", 
                     imageData.size(), 3 * 224 * 224);
        if (imageData.empty()) {
            LOG_ERROR("Image preprocessing returned empty data!");
            return "";
        }

        // Step 3: Run vision encoder
        std::vector<float> imageFeatures = RunVisionEncoder(imageData);
        if (imageFeatures.empty()) {
            LOG_ERROR("Vision encoder failed");
            return "";
        }

        // Step 4: Get prompt tokens and embed them with image features
        std::vector<int64_t> promptTokens = FastVLMTokenizer::GetPromptTokens();
        std::vector<float> inputEmbeds = TokenizeAndEmbed(promptTokens, imageFeatures);
        if (inputEmbeds.empty()) {
            LOG_ERROR("Token embedding failed");
            return "";
        }

        // Step 5: Generate description tokens
        std::vector<int64_t> generatedTokens = Generate(inputEmbeds, 50);
        if (generatedTokens.empty()) {
            LOG_ERROR("Generation failed");
            return "";
        }

        // Step 6: Decode tokens to text
        std::string description = DecodeTokens(generatedTokens);

        auto endTime = std::chrono::high_resolution_clock::now();
        lastLatencyMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        LOG_INFO_FMT("Total latency: %.0f ms", lastLatencyMs);
        LOG_INFO_FMT("Description: %s", description.c_str());

        return description;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Error in DescribeScene: %s", e.what());
        return "";
    }
}

std::vector<float> CameraVisionEngine::TokenizeAndEmbed(
    const std::vector<int64_t>& tokens,
    const std::vector<float>& imageFeatures) {

    try {
        // Embed text tokens
        std::vector<int64_t> tokenShape = {1, static_cast<int64_t>(tokens.size())};

        Ort::Value tokenTensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo,
            const_cast<int64_t*>(tokens.data()),
            tokens.size(),
            tokenShape.data(),
            tokenShape.size()
        );

        const char* inputNames[] = {"input_ids"};
        const char* outputNames[] = {"inputs_embeds"};

        auto outputTensors = embedTokens->Run(
            Ort::RunOptions{nullptr},
            inputNames,
            &tokenTensor,
            1,
            outputNames,
            1
        );

        float* embedData = outputTensors[0].GetTensorMutableData<float>();
        auto tensorInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        size_t embedSize = tensorInfo.GetElementCount();

        std::vector<float> tokenEmbeds(embedData, embedData + embedSize);

        // Combine token embeds with image features
        std::vector<float> combined;
        combined.reserve(tokenEmbeds.size() + imageFeatures.size());

        // Add image features first (replaces <image> token)
        combined.insert(combined.end(), imageFeatures.begin(), imageFeatures.end());

        // Add remaining token embeddings (skip first token which is <image>)
        size_t embedDim = 896;
        combined.insert(combined.end(),
                       tokenEmbeds.begin() + embedDim,  // Skip first token embedding
                       tokenEmbeds.end());

        return combined;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Token embedding error: %s", e.what());
        return {};
    }
}

std::vector<int64_t> CameraVisionEngine::Generate(
    const std::vector<float>& inputEmbeds,
    int maxTokens) {

    std::vector<int64_t> generatedTokens;

    try {
        LOG_INFO_FMT("Starting auto-regressive generation (max %d tokens)...", maxTokens);

        // Calculate initial sequence length
        int seqLen = (inputEmbeds.size() / HIDDEN_SIZE);
        LOG_INFO_FMT("Initial sequence length: %d", seqLen);

        // STEP 1: First forward pass with full prompt
        // =============================================

        // Create attention mask (all ones for input sequence)
        std::vector<int64_t> attentionMask(seqLen, 1);

        // Create position IDs
        std::vector<int64_t> positionIds(seqLen);
        for (int i = 0; i < seqLen; ++i) {
            positionIds[i] = i;
        }

        // Prepare input tensors for first pass
        std::vector<int64_t> embedShape = {1, static_cast<int64_t>(seqLen), HIDDEN_SIZE};
        std::vector<int64_t> maskShape = {1, static_cast<int64_t>(seqLen)};
        std::vector<int64_t> posShape = {1, static_cast<int64_t>(seqLen)};

        Ort::Value embedTensor = Ort::Value::CreateTensor<float>(
            *memoryInfo,
            const_cast<float*>(inputEmbeds.data()),
            inputEmbeds.size(),
            embedShape.data(),
            embedShape.size()
        );

        Ort::Value maskTensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo,
            attentionMask.data(),
            attentionMask.size(),
            maskShape.data(),
            maskShape.size()
        );

        Ort::Value posTensor = Ort::Value::CreateTensor<int64_t>(
            *memoryInfo,
            positionIds.data(),
            positionIds.size(),
            posShape.data(),
            posShape.size()
        );

        // For first pass: empty KV cache (sequence length = 0)
        std::vector<int64_t> emptyKVShape = {1, NUM_HEADS, 0, HEAD_DIM};
        std::vector<float> emptyKVData;

        // Build input names and tensors
        std::vector<const char*> inputNames = {"inputs_embeds", "attention_mask", "position_ids"};
        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(std::move(embedTensor));
        inputTensors.push_back(std::move(maskTensor));
        inputTensors.push_back(std::move(posTensor));

        // Add empty past_key_values for all 24 layers
        std::vector<std::string> kvInputNames;  // Need to keep strings alive
        for (int layer = 0; layer < NUM_LAYERS; ++layer) {
            kvInputNames.push_back("past_key_values." + std::to_string(layer) + ".key");
            kvInputNames.push_back("past_key_values." + std::to_string(layer) + ".value");
            inputNames.push_back(kvInputNames[layer * 2].c_str());
            inputNames.push_back(kvInputNames[layer * 2 + 1].c_str());

            // Empty tensors for first pass
            inputTensors.push_back(Ort::Value::CreateTensor<float>(
                *memoryInfo, emptyKVData.data(), 0, emptyKVShape.data(), emptyKVShape.size()
            ));
            inputTensors.push_back(Ort::Value::CreateTensor<float>(
                *memoryInfo, emptyKVData.data(), 0, emptyKVShape.data(), emptyKVShape.size()
            ));
        }

        // Build output names (logits + present KV cache for all layers)
        std::vector<std::string> kvOutputNames;
        std::vector<const char*> outputNames = {"logits"};
        for (int layer = 0; layer < NUM_LAYERS; ++layer) {
            kvOutputNames.push_back("present." + std::to_string(layer) + ".key");
            kvOutputNames.push_back("present." + std::to_string(layer) + ".value");
            outputNames.push_back(kvOutputNames[layer * 2].c_str());
            outputNames.push_back(kvOutputNames[layer * 2 + 1].c_str());
        }

        LOG_INFO("Running first forward pass...");

        // Run first forward pass
        auto outputTensors = decoder->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            inputTensors.data(),
            inputTensors.size(),
            outputNames.data(),
            outputNames.size()
        );

        // Extract first token from logits
        float* logitsData = outputTensors[0].GetTensorMutableData<float>();
        auto logitsInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        auto logitsShape = logitsInfo.GetShape();

        int vocabSize = logitsShape[2];
        int lastTokenIdx = (logitsShape[1] - 1) * vocabSize;

        // Find argmax (greedy selection)
        float maxLogit = logitsData[lastTokenIdx];
        int64_t nextToken = 0;
        for (int i = 1; i < vocabSize; ++i) {
            if (logitsData[lastTokenIdx + i] > maxLogit) {
                maxLogit = logitsData[lastTokenIdx + i];
                nextToken = i;
            }
        }

        generatedTokens.push_back(nextToken);
        LOG_INFO_FMT("Generated token 1/%d: %d", maxTokens, nextToken);

        // Check for EOS
        if (nextToken == FastVLMTokenizer::EOS_TOKEN_ID) {
            LOG_INFO("EOS token reached");
            return generatedTokens;
        }

        // STEP 2: Auto-regressive loop
        // ==============================

        // Store KV cache from first pass
        std::vector<std::vector<float>> kvCache;
        for (int i = 1; i < outputTensors.size(); ++i) {
            float* kvData = outputTensors[i].GetTensorMutableData<float>();
            auto kvInfo = outputTensors[i].GetTensorTypeAndShapeInfo();
            size_t kvSize = kvInfo.GetElementCount();
            kvCache.push_back(std::vector<float>(kvData, kvData + kvSize));
        }

        int currentPos = seqLen;

        // Generate remaining tokens
        for (int tokenIdx = 1; tokenIdx < maxTokens; ++tokenIdx) {
            // Embed the last generated token
            std::vector<int64_t> tokenVec = {nextToken};
            std::vector<int64_t> tokenShape = {1, 1};

            Ort::Value tokenTensor = Ort::Value::CreateTensor<int64_t>(
                *memoryInfo,
                tokenVec.data(),
                1,
                tokenShape.data(),
                tokenShape.size()
            );

            // Get token embedding
            const char* embedInputNames[] = {"input_ids"};
            const char* embedOutputNames[] = {"inputs_embeds"};

            auto embedOutputs = embedTokens->Run(
                Ort::RunOptions{nullptr},
                embedInputNames,
                &tokenTensor,
                1,
                embedOutputNames,
                1
            );

            float* newEmbedData = embedOutputs[0].GetTensorMutableData<float>();
            size_t newEmbedSize = HIDDEN_SIZE;

            // Prepare inputs for next decoder pass
            std::vector<int64_t> newEmbedShape = {1, 1, HIDDEN_SIZE};
            std::vector<int64_t> newMask = {1};
            std::vector<int64_t> newMaskShape = {1, 1};
            std::vector<int64_t> newPos = {currentPos};
            std::vector<int64_t> newPosShape = {1, 1};

            Ort::Value newEmbedTensor = Ort::Value::CreateTensor<float>(
                *memoryInfo,
                newEmbedData,
                newEmbedSize,
                newEmbedShape.data(),
                newEmbedShape.size()
            );

            Ort::Value newMaskTensor = Ort::Value::CreateTensor<int64_t>(
                *memoryInfo,
                newMask.data(),
                1,
                newMaskShape.data(),
                newMaskShape.size()
            );

            Ort::Value newPosTensor = Ort::Value::CreateTensor<int64_t>(
                *memoryInfo,
                newPos.data(),
                1,
                newPosShape.data(),
                newPosShape.size()
            );

            // Build inputs with KV cache
            std::vector<Ort::Value> newInputTensors;
            newInputTensors.push_back(std::move(newEmbedTensor));
            newInputTensors.push_back(std::move(newMaskTensor));
            newInputTensors.push_back(std::move(newPosTensor));

            // Add past KV cache
            std::vector<int64_t> kvShape = {1, NUM_HEADS, currentPos, HEAD_DIM};
            for (size_t i = 0; i < kvCache.size(); ++i) {
                newInputTensors.push_back(Ort::Value::CreateTensor<float>(
                    *memoryInfo,
                    kvCache[i].data(),
                    kvCache[i].size(),
                    kvShape.data(),
                    kvShape.size()
                ));
            }

            // Run decoder with KV cache
            auto newOutputs = decoder->Run(
                Ort::RunOptions{nullptr},
                inputNames.data(),
                newInputTensors.data(),
                newInputTensors.size(),
                outputNames.data(),
                outputNames.size()
            );

            // Extract next token
            float* newLogitsData = newOutputs[0].GetTensorMutableData<float>();

            // Find argmax
            maxLogit = newLogitsData[0];
            nextToken = 0;
            for (int i = 1; i < vocabSize; ++i) {
                if (newLogitsData[i] > maxLogit) {
                    maxLogit = newLogitsData[i];
                    nextToken = i;
                }
            }

            generatedTokens.push_back(nextToken);
            LOG_INFO_FMT("Generated token %d/%d: %d", tokenIdx + 1, maxTokens, nextToken);

            // Check for EOS
            if (nextToken == FastVLMTokenizer::EOS_TOKEN_ID) {
                LOG_INFO_FMT("EOS token reached after %d tokens", tokenIdx + 1);
                break;
            }

            // Update KV cache with new present values
            kvCache.clear();
            for (int i = 1; i < newOutputs.size(); ++i) {
                float* newKVData = newOutputs[i].GetTensorMutableData<float>();
                auto newKVInfo = newOutputs[i].GetTensorTypeAndShapeInfo();
                size_t newKVSize = newKVInfo.GetElementCount();
                kvCache.push_back(std::vector<float>(newKVData, newKVData + newKVSize));
            }

            currentPos++;
        }

        LOG_INFO_FMT("Generation complete! Generated %zu tokens", generatedTokens.size());
        return generatedTokens;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Generation error: %s", e.what());
        return {};
    }
}

std::string CameraVisionEngine::DecodeTokens(const std::vector<int64_t>& tokenIds) {
    // Use FastVLMTokenizer to decode tokens
    FastVLMTokenizer tokenizer;

    // Load vocabulary
    std::string vocabPath = "models/fastvlm/vocab.json";
    if (!tokenizer.LoadVocab(vocabPath)) {
        LOG_ERROR_FMT("Failed to load vocabulary from %s", vocabPath.c_str());
        return "[Error: Could not load vocabulary]";
    }

    // Decode token IDs to text
    std::string decoded = tokenizer.Decode(tokenIds);

    // Clean up the output
    size_t start = decoded.find_first_not_of(" \t\n\r");
    size_t end = decoded.find_last_not_of(" \t\n\r");

    if (start != std::string::npos && end != std::string::npos) {
        decoded = decoded.substr(start, end - start + 1);
    }

    return decoded;
}
