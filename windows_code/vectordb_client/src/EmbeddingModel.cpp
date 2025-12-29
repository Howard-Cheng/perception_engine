#include "EmbeddingModel.h"
#include "E5EmbeddingDLL.h"  // Use E5EmbeddingDLL for embedding operations
#include <stdexcept>
#include <filesystem>
#include <iostream>

namespace vectordb
{
    // Forward declaration of implementation class
    class EmbeddingModelImpl
    {
    public:
        std::string modelPath;
        bool normalize;
        size_t dimension;
        bool loaded;
        std::string lastError;

        EmbeddingModelImpl(const std::string &path, bool norm)
            : modelPath(path), normalize(norm), dimension(0), loaded(false)
        {
            // Check if model file exists
            if (!std::filesystem::exists(path))
            {
                lastError = "Model file not found: " + path;
                return;
            }

            try
            {
                // Convert path to wide string for E5_Initialize
                std::wstring wpath(path.begin(), path.end());
                
                // Initialize E5 DLL
                int result = E5_Initialize(wpath.c_str());
                
                if (result == 0 && E5_IsInitialized()) {
                    dimension = E5_GetEmbeddingDimension();
                    loaded = true;
                    lastError.clear();
                    
                    std::cout << "EmbeddingModel: Successfully initialized using E5EmbeddingDLL" << std::endl;
                    std::cout << "  Model path: " << path << std::endl;
                    std::cout << "  Dimension: " << dimension << std::endl;
                } else {
                    lastError = "Failed to initialize E5 model: " + std::string(E5_GetLastError());
                    loaded = false;
                }
            }
            catch (const std::exception &e)
            {
                lastError = "Exception during initialization: " + std::string(e.what());
                loaded = false;
            }
        }

        ~EmbeddingModelImpl()
        {
            // E5_Cleanup() is called when the DLL unloads
            // No explicit cleanup needed here
        }

        std::vector<float> encode(const std::string &text)
        {
            if (!loaded)
            {
                throw std::runtime_error("Model not loaded: " + lastError);
            }

            try
            {
                // ? DEBUG: Log encoding attempt
                std::cout << "[EmbeddingModel::encode] Encoding text: '" << text.substr(0, std::min(size_t(50), text.length())) << "...'" << std::endl;
                std::cout << "[EmbeddingModel::encode] Text length: " << text.length() << " chars" << std::endl;
                std::cout << "[EmbeddingModel::encode] Expected dimension: " << dimension << std::endl;
                
                std::vector<float> embedding(dimension);
                
                // Use E5_ComputeEmbeddingFromText which handles tokenization internally
                int result = E5_ComputeEmbeddingFromText(
                    text.c_str(),
                    embedding.data()
                );

                if (result != 0) {
                    lastError = "Encoding failed: " + std::string(E5_GetLastError());
                    std::cerr << "[EmbeddingModel::encode] ERROR: " << lastError << std::endl;
                    throw std::runtime_error(lastError);
                }
                
                // ? DEBUG: Check if embedding is all zeros
                bool allZero = std::all_of(embedding.begin(), embedding.end(), 
                                           [](float v) { return v == 0.0f; });
                if (allZero) {
                    std::cerr << "[EmbeddingModel::encode] WARNING: Embedding is all zeros!" << std::endl;
                    std::cerr << "[EmbeddingModel::encode] This usually means E5_ComputeEmbeddingFromText failed silently" << std::endl;
                    std::cerr << "[EmbeddingModel::encode] E5 last error: " << E5_GetLastError() << std::endl;
                } else {
                    std::cout << "[EmbeddingModel::encode] First 5 values: ";
                    for (size_t i = 0; i < std::min(size_t(5), embedding.size()); ++i) {
                        std::cout << embedding[i] << " ";
                    }
                    std::cout << std::endl;
                }

                return embedding;
            }
            catch (const std::exception &e)
            {
                lastError = "Encoding failed: " + std::string(e.what());
                std::cerr << "[EmbeddingModel::encode] Exception: " << lastError << std::endl;
                throw std::runtime_error(lastError);
            }
        }

        std::vector<std::vector<float>> encodeBatch(const std::vector<std::string> &texts)
        {
            if (!loaded)
            {
                throw std::runtime_error("Model not loaded: " + lastError);
            }

            if (texts.empty())
            {
                return {};
            }

            try
            {
                // Prepare text pointers for batch processing
                std::vector<const char*> textPtrs;
                textPtrs.reserve(texts.size());
                for (const auto& text : texts) {
                    textPtrs.push_back(text.c_str());
                }

                // Allocate buffer for all embeddings
                std::vector<float> allEmbeddings(texts.size() * dimension);
                
                // Use E5_ComputeEmbeddingFromTextBatch for batch processing
                int result = E5_ComputeEmbeddingFromTextBatch(
                    textPtrs.data(),
                    (int)texts.size(),
                    allEmbeddings.data()
                );

                if (result != 0) {
                    lastError = "Batch encoding failed: " + std::string(E5_GetLastError());
                    throw std::runtime_error(lastError);
                }

                // Convert flat array to vector<vector<float>>
                std::vector<std::vector<float>> results;
                results.reserve(texts.size());
                
                for (size_t i = 0; i < texts.size(); ++i) {
                    results.emplace_back(
                        allEmbeddings.begin() + i * dimension,
                        allEmbeddings.begin() + (i + 1) * dimension
                    );
                }

                return results;
            }
            catch (const std::exception &e)
            {
                lastError = "Batch encoding failed: " + std::string(e.what());
                throw std::runtime_error(lastError);
            }
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
        // Reload by recreating the implementation
        try {
            impl_ = std::make_unique<EmbeddingModelImpl>(impl_->modelPath, impl_->normalize);
            return impl_->loaded;
        } catch (const std::exception& e) {
            impl_->lastError = "Reload failed: " + std::string(e.what());
            return false;
        }
    }

} // namespace vectordb
