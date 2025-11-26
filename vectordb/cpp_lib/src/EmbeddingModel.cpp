#include "EmbeddingModel.h"
#include <onnxruntime_cxx_api.h>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace vectordb
{

    // Forward declaration of implementation class
    class EmbeddingModelImpl
    {
    public:
        std::string modelPath;
        std::string modelDir;
        bool normalize;
        size_t dimension;
        bool loaded;
        std::string lastError;

        Ort::Env env;
        Ort::SessionOptions sessionOptions;
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memoryInfo;
        Ort::AllocatorWithDefaultOptions allocator;

        // Input/output names
        std::vector<const char *> inputNames;
        std::vector<const char *> outputNames;
        std::vector<std::string> inputNamesStr;
        std::vector<std::string> outputNamesStr;

        EmbeddingModelImpl(const std::string &path, bool norm)
            : modelPath(path), normalize(norm), dimension(0), loaded(false),
              env(ORT_LOGGING_LEVEL_WARNING, "EmbeddingModel"),
              memoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
        {
            // Get model directory
            std::filesystem::path modelFilePath(path);
            modelDir = modelFilePath.parent_path().string();

            // Check if model file exists
            if (!std::filesystem::exists(path))
            {
                lastError = "Model file not found: " + path;
                return;
            }

            try
            {
                // Configure session options
                sessionOptions.SetIntraOpNumThreads(1);
                sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

                // Convert path to wide string for Windows (ONNX Runtime requires wide string on Windows)
                std::wstring wpath(path.begin(), path.end());
                session = std::make_unique<Ort::Session>(env, wpath.c_str(), sessionOptions);

                // Get input/output names
                size_t numInputNodes = session->GetInputCount();
                size_t numOutputNodes = session->GetOutputCount();

                inputNamesStr.resize(numInputNodes);
                outputNamesStr.resize(numOutputNodes);
                inputNames.resize(numInputNodes);
                outputNames.resize(numOutputNodes);

                for (size_t i = 0; i < numInputNodes; i++)
                {
                    auto name = session->GetInputNameAllocated(i, allocator);
                    inputNamesStr[i] = std::string(name.get());
                    inputNames[i] = inputNamesStr[i].c_str();
                }

                for (size_t i = 0; i < numOutputNodes; i++)
                {
                    auto name = session->GetOutputNameAllocated(i, allocator);
                    outputNamesStr[i] = std::string(name.get());
                    outputNames[i] = outputNamesStr[i].c_str();
                }

                // Get output shape to determine dimension
                auto outputTypeInfo = session->GetOutputTypeInfo(0);
                auto tensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
                auto outputShape = tensorInfo.GetShape();

                // E5 models typically output [batch, seq_len, hidden_size]
                // We need to do mean pooling to get [batch, hidden_size]
                if (outputShape.size() >= 2)
                {
                    dimension = static_cast<size_t>(outputShape[outputShape.size() - 1]);
                }
                else
                {
                    dimension = static_cast<size_t>(outputShape[0]);
                }

                // For E5-small, dimension should be 384
                if (dimension == 0)
                {
                    dimension = 384; // Fallback to known dimension
                }

                loaded = true;
                lastError.clear();
            }
            catch (const std::exception &e)
            {
                lastError = "Failed to load ONNX model: " + std::string(e.what());
                loaded = false;
            }
        }

        std::vector<float> encode(const std::string &text)
        {
            if (!loaded)
            {
                throw std::runtime_error("Model not loaded: " + lastError);
            }

            try
            {
                // Simple tokenization: split by spaces and create basic token IDs
                // Note: This is a simplified version. For production, use proper tokenizer
                std::vector<int64_t> tokenIds;
                std::string processedText = text;

                // Basic tokenization (split by spaces)
                std::istringstream iss(processedText);
                std::string word;
                while (iss >> word)
                {
                    // Simple hash-based token ID (not accurate, but works for testing)
                    int64_t tokenId = std::hash<std::string>{}(word) % 250037; // vocab_size from config
                    tokenIds.push_back(tokenId);
                }

                if (tokenIds.empty())
                {
                    tokenIds.push_back(0); // pad_token_id
                }

                // Limit to max_position_embeddings (512)
                if (tokenIds.size() > 512)
                {
                    tokenIds.resize(512);
                }

                // Pad to at least 1 token
                size_t seqLen = std::max<size_t>(tokenIds.size(), 1);

                // Create input tensor: [1, seq_len]
                std::vector<int64_t> inputShape = {1, static_cast<int64_t>(seqLen)};
                Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
                    memoryInfo, tokenIds.data(), tokenIds.size(), inputShape.data(), inputShape.size());

                // Create attention mask (all ones)
                std::vector<int64_t> attentionMask(seqLen, 1);
                Ort::Value attentionTensor = Ort::Value::CreateTensor<int64_t>(
                    memoryInfo, attentionMask.data(), attentionMask.size(), inputShape.data(), inputShape.size());

                // Run inference
                std::vector<Ort::Value> inputs;
                inputs.push_back(std::move(inputTensor));
                inputs.push_back(std::move(attentionTensor));

                auto outputs = session->Run(Ort::RunOptions{nullptr},
                                            inputNames.data(), inputs.data(), inputs.size(),
                                            outputNames.data(), outputNames.size());

                // Get output tensor
                float *outputData = outputs[0].GetTensorMutableData<float>();
                auto outputShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();

                // E5 models output [batch, seq_len, hidden_size]
                // Need mean pooling: average over sequence length
                size_t batchSize = outputShape[0];
                size_t seqLength = outputShape[1];
                size_t hiddenSize = outputShape[2];

                std::vector<float> embedding(hiddenSize, 0.0f);

                // Mean pooling: average over sequence dimension
                for (size_t i = 0; i < seqLength; i++)
                {
                    for (size_t j = 0; j < hiddenSize; j++)
                    {
                        embedding[j] += outputData[i * hiddenSize + j];
                    }
                }

                // Divide by sequence length
                for (size_t j = 0; j < hiddenSize; j++)
                {
                    embedding[j] /= static_cast<float>(seqLength);
                }

                // Normalize if requested
                if (normalize)
                {
                    float norm = 0.0f;
                    for (float val : embedding)
                    {
                        norm += val * val;
                    }
                    norm = std::sqrt(norm);
                    if (norm > 0.0f)
                    {
                        for (float &val : embedding)
                        {
                            val /= norm;
                        }
                    }
                }

                return embedding;
            }
            catch (const std::exception &e)
            {
                lastError = "Encoding failed: " + std::string(e.what());
                throw std::runtime_error(lastError);
            }
        }

        std::vector<std::vector<float>> encodeBatch(const std::vector<std::string> &texts)
        {
            if (!loaded)
            {
                throw std::runtime_error("Model not loaded: " + lastError);
            }

            std::vector<std::vector<float>> results;
            results.reserve(texts.size());

            // Encode one by one (can be optimized for batch processing later)
            for (const auto &text : texts)
            {
                results.push_back(encode(text));
            }

            return results;
        }
    };

    // ========================================================================
    // EmbeddingModel Implementation
    // ========================================================================

    EmbeddingModel::EmbeddingModel(const std::string &modelPath, bool normalize)
        : impl_(std::make_unique<EmbeddingModelImpl>(modelPath, normalize))
    {
    }

    EmbeddingModel::~EmbeddingModel() = default;

    EmbeddingModel::EmbeddingModel(EmbeddingModel &&) noexcept = default;
    EmbeddingModel &EmbeddingModel::operator=(EmbeddingModel &&) noexcept = default;

    std::vector<float> EmbeddingModel::encode(const std::string &text)
    {
        return impl_->encode(text);
    }

    std::vector<std::vector<float>> EmbeddingModel::encodeBatch(const std::vector<std::string> &texts)
    {
        if (texts.empty())
        {
            return {};
        }
        return impl_->encodeBatch(texts);
    }

    size_t EmbeddingModel::getDimension() const
    {
        return impl_->dimension;
    }

    bool EmbeddingModel::isLoaded() const
    {
        return impl_->loaded;
    }

    std::string EmbeddingModel::getLastError() const
    {
        return impl_->lastError;
    }

    bool EmbeddingModel::reload()
    {
        // TODO: Implement model reloading
        impl_->lastError = "Reload not implemented";
        return false;
    }

} // namespace vectordb
