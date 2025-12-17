#pragma once

#include "vectordb_export.h"
#include "QdrantClient.h"
#include "EmbeddingModel.h"
#include <memory>
#include <optional>
#include <functional>

namespace vectordb {

/**
 * Vector Store - High-level interface combining QdrantClient and EmbeddingModel.
 * 
 * This class provides a convenient way to work with vector storage and search,
 * automatically handling embedding generation and vector operations.
 * 
 * Usage:
 *     VectorStore store("my_collection", "../models/e5-small/model.onnx");
 *     store.initialize();
 *     
 *     // Store text with metadata
 *     store.storeText("collection_name", "text content", metadata);
 *     
 *     // Search
 *     auto results = store.search("collection_name", "query text", 10);
 */
class VECTORDB_API VectorStore {
public:
    /**
     * Constructor.
     * 
     * @param collectionName Name of the Qdrant collection
     * @param embeddingModelPath Path to the ONNX embedding model file
     * @param qdrantConfig Qdrant client configuration (optional, defaults to local mode)
     */
    VectorStore(
        const std::string& collectionName,
        const std::string& embeddingModelPath,
        const QdrantClient::Config& qdrantConfig = QdrantClient::Config::remote("http://localhost:6333")
    );
    
    /**
     * Destructor.
     */
    ~VectorStore();
    
    // Delete copy constructor and assignment
    VectorStore(const VectorStore&) = delete;
    VectorStore& operator=(const VectorStore&) = delete;
    
    // Move constructor and assignment
    VectorStore(VectorStore&&) noexcept;
    VectorStore& operator=(VectorStore&&) = delete; // Cannot move due to reference member
    
    /**
     * Initialize the vector store (load embedding model, connect to Qdrant).
     * 
     * @return true if initialization was successful
     */
    bool initialize();
    
    /**
     * Store a text with metadata in the collection.
     * 
     * @param text Text to store (will be embedded automatically)
     * @param payload Metadata payload
     * @param pointId Optional point ID (auto-generated if not provided)
     * @return true if storage was successful
     */
    bool storeText(
        const std::string& text,
        const Payload& payload = {},
        std::optional<PointId> pointId = {}
    );
    
    /**
     * Store multiple texts with metadata in the collection.
     * 
     * @param texts Vector of texts to store
     * @param payloads Vector of metadata payloads (one per text)
     * @param pointIds Optional vector of point IDs (auto-generated if not provided)
     * @return true if storage was successful
     */
    bool storeTexts(
        const std::vector<std::string>& texts,
        const std::vector<Payload>& payloads = {},
        const std::vector<PointId>& pointIds = {}
    );
    
    /**
     * Search for similar texts in the collection.
     * 
     * @param queryText Query text to search for
     * @param limit Maximum number of results
     * @param scoreThreshold Optional minimum similarity score
     * @param filter Optional metadata filter
     * @return Vector of search results
     */
    std::vector<SearchResult> search(
        const std::string& queryText,
        size_t limit = 10,
        std::optional<float> scoreThreshold = {},
        const std::optional<Filter>& filter = {}
    );
    
    /**
     * Search using a raw query vector (without text encoding).
     * 
     * @param queryVector Query vector
     * @param limit Maximum number of results
     * @param scoreThreshold Optional minimum similarity score
     * @param filter Optional metadata filter
     * @return Vector of search results
     */
    std::vector<SearchResult> searchByVector(
        const std::vector<float>& queryVector,
        size_t limit = 10,
        std::optional<float> scoreThreshold = {},
        const std::optional<Filter>& filter = {}
    );
    
    /**
     * Get the QdrantClient instance (for advanced operations).
     * 
     * @return Reference to the QdrantClient
     */
    QdrantClient& getClient() { return *client_; }
    
    /**
     * Get the EmbeddingModel instance (for advanced operations).
     * 
     * @return Reference to the EmbeddingModel, or nullopt if not loaded
     */
    std::optional<std::reference_wrapper<EmbeddingModel>> getEmbeddingModel() {
        if (embeddingModel_) {
            return std::ref(*embeddingModel_);
        }
        return std::nullopt;
    }
    
    /**
     * Get the embedding dimension.
     * 
     * @return Embedding dimension, or 0 if model is not loaded
     */
    size_t getEmbeddingDimension() const;
    
    /**
     * Get the collection name.
     * 
     * @return Collection name
     */
    const std::string& getCollectionName() const { return collectionName_; }

private:
    std::string collectionName_;
    std::string embeddingModelPath_;
    QdrantClient::Config qdrantConfig_;
    std::unique_ptr<QdrantClient> client_;
    std::unique_ptr<EmbeddingModel> embeddingModel_;
    
    // Generate next point ID
    uint64_t nextPointId_;
    uint64_t generatePointId();
};

} // namespace vectordb

