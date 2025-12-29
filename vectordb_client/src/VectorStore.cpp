#include "VectorStore.h"
#include <stdexcept>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// Fix for Windows min/max macro conflicts
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max
#include <windows.h>
#include <wincrypt.h>  // For SHA1 hashing
#pragma comment(lib, "advapi32.lib")
#endif

namespace vectordb
{
    namespace {
        // Helper function to convert string to UUID using deterministic hashing
        // This mimics Python's uuid.uuid5() behavior
        std::string convertToUuid(const std::string& input) {
            // Standard UUID5 namespace (DNS namespace as used in Python)
            const unsigned char namespace_bytes[] = {
                0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
                0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
            };
            
            // Combine namespace + input string
            std::vector<unsigned char> data;
            data.insert(data.end(), namespace_bytes, namespace_bytes + 16);
            data.insert(data.end(), input.begin(), input.end());
            
#ifdef _WIN32
            // Use Windows CryptoAPI for SHA1
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash = 0;
            unsigned char hash[20] = {0};
            DWORD hashLen = 20;
            
            if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
                if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
                    if (CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
                        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
                    }
                    CryptDestroyHash(hHash);
                }
                CryptReleaseContext(hProv, 0);
            }
#else
            // For non-Windows platforms, use a simple hash-based approach
            // This is a fallback and won't match Python's uuid5 exactly
            unsigned char hash[20] = {0};
            std::hash<std::string> hasher;
            size_t h = hasher(input);
            std::memcpy(hash, &h, sizeof(h));
#endif
            
            // Take first 16 bytes of SHA1 hash for UUID
            // Set version (5) and variant bits as per RFC 4122
            hash[6] = (hash[6] & 0x0F) | 0x50;  // Version 5
            hash[8] = (hash[8] & 0x3F) | 0x80;  // Variant 10
            
            // Format as UUID string: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            
            for (int i = 0; i < 16; i++) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    oss << '-';
                }
                oss << std::setw(2) << static_cast<int>(hash[i]);
            }
            
            return oss.str();
        }
    }

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

            size_t vectorSize = embeddingModel_->getDimension();

            // Check if collection exists and has correct dimensions
            if (client_->collectionExists(collectionName_))
            {
                auto collectionInfo = client_->getCollectionInfo(collectionName_);
                if (collectionInfo.has_value())
                {
                    size_t existingVectorSize = collectionInfo->vectorSize;

                    if (existingVectorSize != vectorSize)
                    {
                        std::cerr << "WARNING: Collection '" << collectionName_
                                  << "' has incorrect vector size!" << std::endl;
                        std::cerr << "  Expected: " << vectorSize << std::endl;
                        std::cerr << "  Found: " << existingVectorSize << std::endl;
                        std::cerr << "  Recreating collection..." << std::endl;

                        // Delete and recreate collection
                        if (!client_->deleteCollection(collectionName_))
                        {
                            std::cerr << "Failed to delete collection with wrong dimensions" << std::endl;
                            return false;
                        }

                        if (!client_->createCollection(collectionName_, vectorSize, DistanceMetric::COSINE))
                        {
                            std::cerr << "Failed to recreate collection" << std::endl;
                            return false;
                        }

                        std::cerr << "Collection recreated successfully with correct dimensions" << std::endl;
                    }
                }
            }
            else
            {
                // Create collection if it doesn't exist
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
            PointId id;
            if (pointId.has_value()) {
                // Convert string IDs to UUID format for Qdrant compatibility
                if (std::holds_alternative<std::string>(pointId.value())) {
                    std::string stringId = std::get<std::string>(pointId.value());
                    std::string uuidStr = convertToUuid(stringId);
                    id = uuidStr;
                } else {
                    // For uint64_t IDs, use as-is
                    id = pointId.value();
                }
            } else {
                id = generatePointId();
            }

            // Create vector point
            VectorPoint point(id, embedding, payload);

            // Store in Qdrant
            return client_->upsert(collectionName_, point);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception in VectorStore::storeText: " << e.what() << std::endl;
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
                    // Convert string IDs to UUID format for Qdrant compatibility
                    if (std::holds_alternative<std::string>(pointIds[i])) {
                        std::string stringId = std::get<std::string>(pointIds[i]);
                        std::string uuidStr = convertToUuid(stringId);
                        id = uuidStr;
                    } else {
                        // For uint64_t IDs, use as-is
                        id = pointIds[i];
                    }
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
            std::cerr << "Exception in VectorStore::storeTexts: " << e.what() << std::endl;
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
                std::cerr << "[VectorStore::search] Embedding model not loaded" << std::endl;
                return {};
            }

            // ? DEBUG: Log query info
            std::cout << "[VectorStore::search] Query text: '" << queryText << "'" << std::endl;
            std::cout << "[VectorStore::search] Collection: " << collectionName_ << std::endl;
            std::cout << "[VectorStore::search] Limit: " << limit << std::endl;
            
            // Generate query embedding
            std::vector<float> queryVector = embeddingModel_->encode(queryText);
            
            // ? DEBUG: Check query vector
            std::cout << "[VectorStore::search] Query vector dimension: " << queryVector.size() << std::endl;
            
            // Check if vector is all zeros
            bool allZero = std::all_of(queryVector.begin(), queryVector.end(), 
                                       [](float v) { return v == 0.0f; });
            if (allZero) {
                std::cerr << "[VectorStore::search] ERROR: Query vector is all zeros!" << std::endl;
                std::cerr << "[VectorStore::search] This means embedding model failed to encode the text" << std::endl;
            }
            
            // Print first few values
            std::cout << "[VectorStore::search] First 5 vector values: ";
            for (size_t i = 0; i < std::min(size_t(5), queryVector.size()); ++i) {
                std::cout << queryVector[i] << " ";
            }
            std::cout << std::endl;

            // Search in Qdrant
            return client_->search(collectionName_, queryVector, limit, scoreThreshold, filter);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[VectorStore::search] Exception: " << e.what() << std::endl;
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

    std::vector<SearchResult> VectorStore::querySessionSummaries(
        const std::string& keyword,
        time_t startTime,
        time_t endTime,
        int maxResults,
        std::optional<float> scoreThreshold)
    {
        try {
            if (!embeddingModel_ || !embeddingModel_->isLoaded()) {
                std::cerr << "Embedding model not loaded" << std::endl;
                return {};
            }

            // Build filter for time range
            std::vector<FilterCondition> conditions;

            // Convert time_t to int64_t (Unix timestamps)
            int64_t startTimeInt = static_cast<int64_t>(startTime);
            int64_t endTimeInt = static_cast<int64_t>(endTime);

            // Add timestamp range filter
            conditions.push_back(
                FilterCondition::createRange(
                    "created_at",
                    std::nullopt,         // gt (greater than)
                    startTimeInt,         // gte (greater than or equal)
                    std::nullopt,         // lt (less than)
                    endTimeInt            // lte (less than or equal)
                )
            );

            Filter filter = Filter::createMust(conditions);

            // Perform semantic search
            std::vector<SearchResult> results = search(
                keyword,
                maxResults,
                scoreThreshold,
                filter
            );

            std::cout << "[QuerySessionSummaries] Found " << results.size()
                << " sessions matching query '" << keyword << "' "
                << "in time range [" << startTime << ", " << endTime << "]"
                << std::endl;

            return results;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in querySessionSummaries: " << e.what() << std::endl;
            return {};
        }
    }


} // namespace vectordb
