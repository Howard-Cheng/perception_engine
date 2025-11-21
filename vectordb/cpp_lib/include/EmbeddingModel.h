#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace vectordb {

// Forward declaration
class EmbeddingModelImpl;

/**
 * Embedding Model for text vectorization using ONNX models.
 * 
 * This class provides an interface for generating vector embeddings from text
 * using ONNX-based models (e.g., E5-small exported to ONNX format).
 * 
 * Usage:
 *     EmbeddingModel model("../models/e5-small/model.onnx");
 *     auto embedding = model.encode("Hello world");
 *     auto embeddings = model.encodeBatch({"text1", "text2"});
 */
class EmbeddingModel {
public:
    /**
     * Constructor.
     * 
     * @param modelPath Path to the ONNX model file
     * @param normalize Whether to normalize embeddings (L2 normalization, default: true)
     */
    explicit EmbeddingModel(
        const std::string& modelPath,
        bool normalize = true
    );
    
    /**
     * Destructor.
     */
    ~EmbeddingModel();
    
    // Delete copy constructor and assignment
    EmbeddingModel(const EmbeddingModel&) = delete;
    EmbeddingModel& operator=(const EmbeddingModel&) = delete;
    
    // Move constructor and assignment
    EmbeddingModel(EmbeddingModel&&) noexcept;
    EmbeddingModel& operator=(EmbeddingModel&&) noexcept;
    
    /**
     * Encode a single text into an embedding vector.
     * 
     * Note: For E5 models, text should include prefix:
     *   - "query: " for search queries
     *   - "passage: " for documents to be searched
     * 
     * @param text Input text to encode
     * @return Vector of floats representing the embedding
     * @throws std::runtime_error if encoding fails
     */
    std::vector<float> encode(const std::string& text);
    
    /**
     * Encode a batch of texts into embedding vectors.
     * 
     * @param texts Vector of input texts to encode
     * @return Vector of embedding vectors, each as a vector of floats
     * @throws std::runtime_error if encoding fails
     */
    std::vector<std::vector<float>> encodeBatch(const std::vector<std::string>& texts);
    
    /**
     * Get the dimension of embeddings produced by this model.
     * 
     * @return Embedding dimension, or 0 if model is not loaded
     */
    size_t getDimension() const;
    
    /**
     * Check if the model is loaded and ready to use.
     * 
     * @return true if model is loaded
     */
    bool isLoaded() const;
    
    /**
     * Get the last error message (if any).
     * 
     * @return Error message string
     */
    std::string getLastError() const;
    
    /**
     * Reload the model (useful if model file was updated).
     * 
     * @return true if reload was successful
     */
    bool reload();

private:
    std::unique_ptr<EmbeddingModelImpl> impl_;
};

} // namespace vectordb

