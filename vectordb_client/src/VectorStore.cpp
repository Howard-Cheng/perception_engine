#include "VectorStore.h"
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vectordb
{

    VectorStore::VectorStore(
        const std::string &collectionName,
        const std::string &embeddingModelPath,
        const QdrantClient::Config &qdrantConfig)
        : collectionName_(collectionName), embeddingModelPath_(embeddingModelPath), qdrantConfig_(qdrantConfig), nextPointId_(1)
    {

        // Initialize Qdrant client
        client_ = std::make_unique<QdrantClient>(qdrantConfig_);
    }

    VectorStore::~VectorStore() = default;

    VectorStore::VectorStore(VectorStore &&) noexcept = default;

    bool VectorStore::initialize()
    {
        try
        {
            // Test Qdrant connection
            if (!client_->testConnection())
            {
                return false;
            }

            // Resolve model path - handle both absolute and relative paths
            std::filesystem::path modelPath(embeddingModelPath_);

            // If path is relative, try multiple resolution strategies
            if (!modelPath.is_absolute())
            {
                // Strategy 1: Try relative to current working directory
                std::filesystem::path cwdPath = std::filesystem::current_path() / modelPath;

                // Strategy 2: Try relative to executable directory (Windows)
                std::filesystem::path exePath;
                bool hasExePath = false;

#ifdef _WIN32
                char exePathBuf[MAX_PATH];
                if (GetModuleFileNameA(NULL, exePathBuf, MAX_PATH) != 0)
                {
                    exePath = std::filesystem::path(exePathBuf).parent_path() / modelPath;
                    hasExePath = true;
                }
#endif

                // Choose the path that exists
                if (std::filesystem::exists(cwdPath))
                {
                    modelPath = cwdPath;
                }
                else if (hasExePath && std::filesystem::exists(exePath))
                {
                    modelPath = exePath;
                }
                else
                {
                    // Neither path exists - use cwd path for error message
                    modelPath = cwdPath;
                }
            }

            // Load embedding model
            if (!std::filesystem::exists(modelPath))
            {
                // Log the paths we tried for debugging
                std::cerr << "Embedding model not found at: " << modelPath.string() << std::endl;
                std::cerr << "Current working directory: " << std::filesystem::current_path().string() << std::endl;
                std::cerr << "Original path provided: " << embeddingModelPath_ << std::endl;
                return false;
            }

            try
            {
                embeddingModel_ = std::make_unique<EmbeddingModel>(modelPath.string());

                if (!embeddingModel_->isLoaded())
                {
                    return false;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Failed to load embedding model: " << e.what() << std::endl;
                return false;
            }

            // Create collection if it doesn't exist
            if (!client_->collectionExists(collectionName_))
            {
                size_t vectorSize = embeddingModel_->getDimension();
                if (!client_->createCollection(collectionName_, vectorSize, DistanceMetric::COSINE))
                {
                    return false;
                }
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception in VectorStore::initialize(): " << e.what() << std::endl;
            return false;
        }
    }

    bool VectorStore::storeText(
        const std::string &text,
        const Payload &payload,
        std::optional<PointId> pointId)
    {

        try
        {
            // Generate embedding
            if (!embeddingModel_ || !embeddingModel_->isLoaded())
            {
                return false;
            }

            std::vector<float> embedding = embeddingModel_->encode(text);

            // Use provided point ID or generate one
            PointId id = pointId.has_value() ? pointId.value() : generatePointId();

            // Create vector point
            VectorPoint point(id, embedding, payload);

            // Store in Qdrant
            return client_->upsert(collectionName_, point);
        }
        catch (const std::exception &e)
        {
            return false;
        }
    }

    bool VectorStore::storeTexts(
        const std::vector<std::string> &texts,
        const std::vector<Payload> &payloads,
        const std::vector<PointId> &pointIds)
    {

        try
        {
            if (texts.empty())
            {
                return true;
            }

            if (!embeddingModel_ || !embeddingModel_->isLoaded())
            {
                return false;
            }

            // Generate embeddings
            std::vector<std::vector<float>> embeddings = embeddingModel_->encodeBatch(texts);

            // Create vector points
            std::vector<VectorPoint> points;
            points.reserve(texts.size());

            for (size_t i = 0; i < texts.size(); ++i)
            {
                PointId id;
                if (i < pointIds.size())
                {
                    id = pointIds[i];
                }
                else
                {
                    id = generatePointId();
                }

                Payload payload;
                if (i < payloads.size())
                {
                    payload = payloads[i];
                }

                points.emplace_back(id, embeddings[i], payload);
            }

            // Store in Qdrant
            return client_->upsert(collectionName_, points);
        }
        catch (const std::exception &e)
        {
            return false;
        }
    }

    std::vector<SearchResult> VectorStore::search(
        const std::string &queryText,
        size_t limit,
        std::optional<float> scoreThreshold,
        const std::optional<Filter> &filter)
    {

        try
        {
            if (!embeddingModel_ || !embeddingModel_->isLoaded())
            {
                return {};
            }

            // Generate query embedding
            std::vector<float> queryVector = embeddingModel_->encode(queryText);

            // Search in Qdrant
            return client_->search(collectionName_, queryVector, limit, scoreThreshold, filter);
        }
        catch (const std::exception &e)
        {
            return {};
        }
    }

    std::vector<SearchResult> VectorStore::searchByVector(
        const std::vector<float> &queryVector,
        size_t limit,
        std::optional<float> scoreThreshold,
        const std::optional<Filter> &filter)
    {

        return client_->search(collectionName_, queryVector, limit, scoreThreshold, filter);
    }

    size_t VectorStore::getEmbeddingDimension() const
    {
        if (embeddingModel_ && embeddingModel_->isLoaded())
        {
            return embeddingModel_->getDimension();
        }
        return 0;
    }

    uint64_t VectorStore::generatePointId()
    {
        return nextPointId_++;
    }

} // namespace vectordb
