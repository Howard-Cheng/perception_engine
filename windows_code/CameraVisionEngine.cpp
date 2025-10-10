#include "CameraVisionEngine.h"
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
        std::cout << "[Camera] Initializing CameraVisionEngine..." << std::endl;

        // Initialize ONNX Runtime environment
        ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "CameraVisionEngine");

        sessionOptions = std::make_unique<Ort::SessionOptions>();
        sessionOptions->SetIntraOpNumThreads(4);  // 4 threads for CPU inference
        sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);  // Disable optimization to avoid dynamic shape issues

        memoryInfo = std::make_unique<Ort::MemoryInfo>(
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)
        );

        // Load ONNX models
        std::cout << "[Camera] Loading vision encoder..." << std::endl;
        std::string visionPath = modelPath + "/onnx/vision_encoder_simplified.onnx";
        visionEncoder = LoadOnnxModel(visionPath);
        if (!visionEncoder) {
            std::cerr << "[Camera] Failed to load vision encoder" << std::endl;
            return false;
        }

        std::cout << "[Camera] Loading embed tokens model..." << std::endl;
        std::string embedPath = modelPath + "/onnx/embed_tokens_q4f16.onnx";
        embedTokens = LoadOnnxModel(embedPath);
        if (!embedTokens) {
            std::cerr << "[Camera] Failed to load embed tokens model" << std::endl;
            return false;
        }

        std::cout << "[Camera] Loading decoder model..." << std::endl;
        std::string decoderPath = modelPath + "/onnx/decoder_model_merged_q4f16.onnx";
        decoder = LoadOnnxModel(decoderPath);
        if (!decoder) {
            std::cerr << "[Camera] Failed to load decoder model" << std::endl;
            return false;
        }

        // Initialize camera
        std::cout << "[Camera] Opening camera " << cameraIndex << "..." << std::endl;
        camera.open(cameraIndex);
        if (!camera.isOpened()) {
            std::cerr << "[Camera] Failed to open camera" << std::endl;
            return false;
        }

        // Set camera resolution (smaller = faster)
        camera.set(cv::CAP_PROP_FRAME_WIDTH, 320);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, 240);

        isInitialized = true;
        std::cout << "[Camera] Initialization complete!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Camera] Initialization error: " << e.what() << std::endl;
        return false;
    }
}

std::unique_ptr<Ort::Session> CameraVisionEngine::LoadOnnxModel(const std::string& modelPath) {
    try {
        std::cout << "[Camera] Loading model from: " << modelPath << std::endl;

        // Convert UTF-8 string to wide string for Windows
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0);
        if (wideSize == 0) {
            std::cerr << "[Camera] Failed to convert path: " << modelPath << std::endl;
            return nullptr;
        }

        std::wstring wModelPath(wideSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, &wModelPath[0], wideSize);

        std::cout << "[Camera] Attempting to load ONNX session..." << std::endl;
        auto session = std::make_unique<Ort::Session>(*ortEnv, wModelPath.c_str(), *sessionOptions);
        std::cout << "[Camera] Model loaded successfully!" << std::endl;
        return session;
    } catch (const std::exception& e) {
        std::cerr << "[Camera] Error loading model " << modelPath << ": " << e.what() << std::endl;
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
        std::cout << "[Camera] Vision encoder input data size: " << imageData.size() << std::endl;

        // Input shape: [1, 3, 224, 224]
        std::vector<int64_t> inputShape = {1, 3, 224, 224};

        // Verify data size matches shape
        size_t expectedSize = 1 * 3 * 224 * 224;
        if (imageData.size() != expectedSize) {
            std::cerr << "[Camera] ERROR: Image data size mismatch! Got " << imageData.size()
                      << ", expected " << expectedSize << std::endl;
            return {};
        }

        std::cout << "[Camera] Creating input tensor with shape [1, 3, 224, 224]" << std::endl;

        // Create input tensor
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            *memoryInfo,
            const_cast<float*>(imageData.data()),
            imageData.size(),
            inputShape.data(),
            inputShape.size()
        );

        std::cout << "[Camera] Input tensor created, running inference..." << std::endl;

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
        std::cerr << "[Camera] Vision encoder error: " << e.what() << std::endl;
        return {};
    }
}

std::string CameraVisionEngine::DescribeScene() {
    if (!isInitialized) {
        std::cerr << "[Camera] Engine not initialized" << std::endl;
        return "";
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Step 1: Capture frame
        cv::Mat frame;
        if (!camera.read(frame) || frame.empty()) {
            std::cerr << "[Camera] Failed to capture frame" << std::endl;
            return "";
        }

        // Validate frame dimensions
        if (frame.rows == 0 || frame.cols == 0) {
            std::cerr << "[Camera] Invalid frame dimensions: " << frame.rows << "x" << frame.cols << std::endl;
            return "";
        }

        std::cout << "[Camera] Captured frame: " << frame.cols << "x" << frame.rows << std::endl;

        // Step 2: Preprocess image
        std::vector<float> imageData;
        PreprocessImage(frame, imageData);

        std::cout << "[Camera] Preprocessed image data size: " << imageData.size() << " (expected: " << (3 * 224 * 224) << ")" << std::endl;
        if (imageData.empty()) {
            std::cerr << "[Camera] ERROR: Image preprocessing returned empty data!" << std::endl;
            return "";
        }

        // Step 3: Run vision encoder
        std::vector<float> imageFeatures = RunVisionEncoder(imageData);
        if (imageFeatures.empty()) {
            std::cerr << "[Camera] Vision encoder failed" << std::endl;
            return "";
        }

        // Step 4: Get prompt tokens and embed them with image features
        std::vector<int64_t> promptTokens = FastVLMTokenizer::GetPromptTokens();
        std::vector<float> inputEmbeds = TokenizeAndEmbed(promptTokens, imageFeatures);
        if (inputEmbeds.empty()) {
            std::cerr << "[Camera] Token embedding failed" << std::endl;
            return "";
        }

        // Step 5: Generate description tokens
        std::vector<int64_t> generatedTokens = Generate(inputEmbeds, 50);
        if (generatedTokens.empty()) {
            std::cerr << "[Camera] Generation failed" << std::endl;
            return "";
        }

        // Step 6: Decode tokens to text
        std::string description = DecodeTokens(generatedTokens);

        auto endTime = std::chrono::high_resolution_clock::now();
        lastLatencyMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        std::cout << "[Camera] Total latency: " << lastLatencyMs << "ms" << std::endl;
        std::cout << "[Camera] Description: " << description << std::endl;

        return description;

    } catch (const std::exception& e) {
        std::cerr << "[Camera] Error in DescribeScene: " << e.what() << std::endl;
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
        // Token embeds: [1, num_tokens, 896]
        // Image features: [1, 16, 896]
        // Need to insert image features at <image> token position (index 0)

        std::vector<float> combined;
        combined.reserve(tokenEmbeds.size() + imageFeatures.size());

        // Add image features first (replaces <image> token)
        combined.insert(combined.end(), imageFeatures.begin(), imageFeatures.end());

        // Add remaining token embeddings (skip first token which is <image>)
        size_t embedDim = 896;
        size_t tokensPerEmbed = embedDim;
        combined.insert(combined.end(),
                       tokenEmbeds.begin() + embedDim,  // Skip first token embedding
                       tokenEmbeds.end());

        return combined;

    } catch (const std::exception& e) {
        std::cerr << "[Camera] Token embedding error: " << e.what() << std::endl;
        return {};
    }
}

std::vector<int64_t> CameraVisionEngine::Generate(
    const std::vector<float>& inputEmbeds,
    int maxTokens) {

    std::vector<int64_t> generatedTokens;

    try {
        std::cout << "[Camera] Starting auto-regressive generation (max " << maxTokens << " tokens)..." << std::endl;

        // Calculate initial sequence length
        int seqLen = (inputEmbeds.size() / HIDDEN_SIZE);
        std::cout << "[Camera] Initial sequence length: " << seqLen << std::endl;

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

        std::cout << "[Camera] Running first forward pass..." << std::endl;

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
        std::cout << "[Camera] Generated token 1/" << maxTokens << ": " << nextToken << std::endl;

        // Check for EOS
        if (nextToken == FastVLMTokenizer::EOS_TOKEN_ID) {
            std::cout << "[Camera] EOS token reached" << std::endl;
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
            std::cout << "[Camera] Generated token " << (tokenIdx + 1) << "/" << maxTokens
                     << ": " << nextToken << std::endl;

            // Check for EOS
            if (nextToken == FastVLMTokenizer::EOS_TOKEN_ID) {
                std::cout << "[Camera] EOS token reached after " << (tokenIdx + 1) << " tokens" << std::endl;
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

        std::cout << "[Camera] Generation complete! Generated " << generatedTokens.size() << " tokens" << std::endl;
        return generatedTokens;

    } catch (const std::exception& e) {
        std::cerr << "[Camera] Generation error: " << e.what() << std::endl;
        return {};
    }
}

std::string CameraVisionEngine::DecodeTokens(const std::vector<int64_t>& tokenIds) {
    // Use FastVLMTokenizer to decode tokens
    FastVLMTokenizer tokenizer;

    // Load vocabulary
    std::string vocabPath = "models/fastvlm/vocab.json";
    if (!tokenizer.LoadVocab(vocabPath)) {
        std::cerr << "[Camera] Failed to load vocabulary from " << vocabPath << std::endl;
        return "[Error: Could not load vocabulary]";
    }

    // Decode token IDs to text
    std::string decoded = tokenizer.Decode(tokenIds);

    // Clean up the output (remove extra whitespace, trim, etc.)
    // Remove leading/trailing whitespace
    size_t start = decoded.find_first_not_of(" \t\n\r");
    size_t end = decoded.find_last_not_of(" \t\n\r");

    if (start != std::string::npos && end != std::string::npos) {
        decoded = decoded.substr(start, end - start + 1);
    }

    return decoded;
}
