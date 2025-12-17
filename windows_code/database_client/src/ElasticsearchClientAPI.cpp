// src/ElasticsearchClientAPI.cpp
#include "ElasticsearchClientAPI.h"
#include "ElasticsearchClient.h"
#include <cstring>
#include <memory>
#include <iostream>

using namespace database;

// Internal structure to hold the C++ client
struct ESClient {
    std::unique_ptr<ElasticsearchClient> client;
};

// Client management
ES_CLIENT_API ESClient* ES_CreateClient(const char* esUrl) {
    try {
        auto esClient = new ESClient();
        esClient->client = std::make_unique<ElasticsearchClient>(esUrl);
        return esClient;
    } catch (...) {
        return nullptr;
    }
}

ES_CLIENT_API void ES_DestroyClient(ESClient* client) {
    delete client;
}

// Index operations
ES_CLIENT_API int ES_InitializeIndex(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->initializeCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

ES_CLIENT_API int ES_DeleteIndex(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->deleteCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

ES_CLIENT_API int ES_IndexExists(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->collectionExists(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

ES_CLIENT_API int ES_RefreshIndex(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->refreshCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

// Document operations
ES_CLIENT_API char* ES_IndexDocument(ESClient* client, const char* indexName, 
                                     const char* eventJson) {
    if (!client || !client->client || !indexName || !eventJson) {
        return nullptr;
    }
    // Note: This would need RawEvent parsing from JSON
    // For now, returning nullptr as placeholder
    return nullptr;
}

ES_CLIENT_API int ES_UpdateDocument(ESClient* client, const char* indexName,
                                   const char* docId, const char* updateJson) {
    if (!client || !client->client || !indexName || !docId || !updateJson) {
        return 0;
    }
    try {
        return client->client->updateDocument(indexName, docId, updateJson) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

// Statistics
ES_CLIENT_API int ES_GetDocumentCount(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->getDocumentCount(indexName);
    } catch (...) {
        return 0;
    }
}

ES_CLIENT_API int ES_GetUncompressedCount(ESClient* client, const char* indexName) {
    if (!client || !client->client || !indexName) {
        return 0;
    }
    try {
        return client->client->getUncompressedCount(indexName);
    } catch (...) {
        return 0;
    }
}

// Utility
ES_CLIENT_API int ES_TestConnection(ESClient* client) {
    if (!client || !client->client) {
        return 0;
    }
    try {
        return client->client->testConnection() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

ES_CLIENT_API char* ES_GetClusterInfo(ESClient* client) {
    if (!client || !client->client) {
        return nullptr;
    }
    try {
        std::string info = client->client->getServerInfo();
        char* result = new char[info.length() + 1];
        std::strcpy(result, info.c_str());
        return result;
    } catch (...) {
        return nullptr;
    }
}

// Memory management
ES_CLIENT_API void ES_FreeString(char* str) {
    delete[] str;
}
