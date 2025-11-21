/**
 * Vector Store Usage Example with E5-Small Model
 * ==============================================
 *
 * This example demonstrates how to use the VectorStore with E5-small
 * embedding model for vector storage and search operations.
 *
 * Model files are automatically copied to output directory by CMake:
 *   build/bin/models/e5-small/model.onnx
 *
 * The code uses relative path "models/e5-small/model.onnx" which is
 * resolved relative to the executable directory at runtime.
 */

#include "VectorStore.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <filesystem>
#include <windows.h>

using namespace vectordb;

// Get the directory where the executable is located
std::filesystem::path getExeDirectory()
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path path(exePath);
    return path.parent_path();
}

int main()
{
    std::cout << "=== Vector Store Example with E5-Small Model ===" << std::endl;

    // ========================================================================
    // 1. Initialize Vector Store
    // ========================================================================

    // Get executable directory and construct model path
    std::filesystem::path exeDir = getExeDirectory();
    std::filesystem::path modelPath = exeDir / "models" / "e5-small" / "model.onnx";

    // Collection name
    std::string collectionName = "perception_context";

    // Qdrant configuration (connect to local Qdrant server)
    // Make sure Qdrant is running: docker run -p 6333:6333 qdrant/qdrant
    auto qdrantConfig = QdrantClient::Config::remote("http://localhost:6333");

    // Create vector store
    VectorStore store(collectionName, modelPath.string(), qdrantConfig);

    std::cout << "\nInitializing Vector Store..." << std::endl;
    std::cout << "Executable directory: " << exeDir.string() << std::endl;
    std::cout << "Model path: " << modelPath.string() << std::endl;
    std::cout << "Collection: " << collectionName << std::endl;
    std::cout.flush();

    if (!store.initialize())
    {
        std::cerr << "Failed to initialize Vector Store" << std::endl;
        std::cerr << "Qdrant error: " << (store.getClient().getLastError().empty() ? "(none)" : store.getClient().getLastError()) << std::endl;
        if (store.getEmbeddingModel().has_value())
        {
            std::cerr << "Embedding model error: "
                      << store.getEmbeddingModel().value().get().getLastError() << std::endl;
        }
        else
        {
            std::cerr << "Embedding model: Not created" << std::endl;
            std::cerr << "Model file exists: " << (std::filesystem::exists(modelPath) ? "Yes" : "No") << std::endl;
        }
        return 1;
    }

    std::cout << "✓ Vector Store initialized" << std::endl;
    std::cout << "  Collection: " << store.getCollectionName() << std::endl;
    std::cout << "  Embedding dimension: " << store.getEmbeddingDimension() << std::endl;

    // ========================================================================
    // 2. Store Texts with E5 Embeddings
    // ========================================================================

    // Note: E5 models require text prefixes:
    //   - "passage: " for documents to be searched
    //   - "query: " for search queries

    std::vector<std::string> texts = {
        "passage: Working on GitHub PR #123 for feature implementation",
        "passage: Reviewing code changes in main.py",
        "passage: Attending team meeting about project roadmap",
        "passage: Reading documentation about API design",
        "passage: Debugging issue with database connection"};

    std::vector<Payload> payloads;
    for (size_t i = 0; i < texts.size(); ++i)
    {
        Payload payload;
        // Store original text without prefix for display
        std::string originalText = texts[i];
        if (originalText.find("passage: ") == 0)
        {
            originalText = originalText.substr(9); // Remove "passage: " prefix
        }
        payload["text"] = originalText;
        payload["index"] = static_cast<int64_t>(i);
        payload["content_type"] = std::string("code");
        payload["timestamp"] = static_cast<int64_t>(std::time(nullptr));
        payloads.push_back(payload);
    }

    std::cout << "\nStoring " << texts.size() << " texts with E5 embeddings..." << std::endl;
    if (!store.storeTexts(texts, payloads))
    {
        std::cerr << "Failed to store texts" << std::endl;
        return 1;
    }
    std::cout << "✓ Texts stored successfully" << std::endl;

    // ========================================================================
    // 3. Search with E5 Embeddings
    // ========================================================================

    // For search queries, use "query: " prefix
    std::string queryText = "query: GitHub pull request review";
    std::cout << "\nSearching for: \"" << queryText << "\"" << std::endl;

    auto results = store.search(queryText, 5);

    std::cout << "Found " << results.size() << " results:" << std::endl;
    for (size_t i = 0; i < results.size(); ++i)
    {
        const auto &result = results[i];
        std::cout << "  " << (i + 1) << ". Score: " << std::fixed << std::setprecision(4)
                  << result.score;

        if (result.payload.has_value())
        {
            const auto &payload = result.payload.value();
            auto it = payload.find("text");
            if (it != payload.end())
            {
                if (std::holds_alternative<std::string>(it->second))
                {
                    std::cout << " - " << std::get<std::string>(it->second);
                }
            }
        }
        std::cout << std::endl;
    }

    // ========================================================================
    // 4. Search with Metadata Filter
    // ========================================================================

    std::cout << "\nSearching with filter (content_type = 'code')..." << std::endl;

    auto filter = Filter::createMust({FilterCondition::createMatch("content_type", std::string("code"))});

    auto filteredResults = store.search(queryText, 5, {}, filter);
    std::cout << "Found " << filteredResults.size() << " filtered results" << std::endl;

    // ========================================================================
    // 5. Collection Information
    // ========================================================================

    auto collectionInfo = store.getClient().getCollectionInfo(collectionName);
    if (collectionInfo.has_value())
    {
        std::cout << "\nCollection Info:" << std::endl;
        std::cout << "  Name: " << collectionInfo->name << std::endl;
        std::cout << "  Points: " << collectionInfo->pointsCount << std::endl;
        std::cout << "  Vector Size: " << collectionInfo->vectorSize << std::endl;
        std::cout << "  Status: " << collectionInfo->status << std::endl;
    }

    std::cout << "\n=== Example completed ===" << std::endl;

    return 0;
}
