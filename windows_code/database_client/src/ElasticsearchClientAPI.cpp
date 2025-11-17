// src/ElasticsearchClientAPI.cpp
#include "ElasticsearchClientAPI.h"
#include "ElasticsearchClient.h"
#include <cstring>
#include <cstdlib>

using namespace database;

// Helper: Allocate C string
static char* allocateString(const std::string& str) {
    char* result = (char*)malloc(str.length() + 1);
    if (result) {
        strcpy(result, str.c_str());
    }
    return result;
}

ESClient* ES_CreateClient(const char* esUrl) {
    try {
        return reinterpret_cast<ESClient*>(new ElasticsearchClient(esUrl));
    } catch (...) {
        return nullptr;
    }
}

void ES_DestroyClient(ESClient* client) {
    if (client) {
        delete reinterpret_cast<ElasticsearchClient*>(client);
    }
}

int ES_InitializeIndex(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->initializeCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int ES_DeleteIndex(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->deleteCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int ES_IndexExists(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->collectionExists(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int ES_RefreshIndex(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->refreshCollection(indexName) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

char* ES_IndexDocument(ESClient* client, const char* indexName, 
                       const char* eventJson) {
    if (!client || !indexName || !eventJson) return nullptr;
    
    try {
        // This is a simplified version - in real implementation,
        // you would parse JSON to RawEvent and call indexDocument
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

int ES_UpdateDocument(ESClient* client, const char* indexName,
                     const char* docId, const char* updateJson) {
    if (!client || !indexName || !docId || !updateJson) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->updateDocument(indexName, docId, updateJson) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int ES_GetDocumentCount(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->getDocumentCount(indexName);
    } catch (...) {
        return 0;
    }
}

int ES_GetUncompressedCount(ESClient* client, const char* indexName) {
    if (!client || !indexName) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->getUncompressedCount(indexName);
    } catch (...) {
        return 0;
    }
}

int ES_TestConnection(ESClient* client) {
    if (!client) return 0;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        return esClient->testConnection() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

char* ES_GetClusterInfo(ESClient* client) {
    if (!client) return nullptr;
    
    try {
        auto* esClient = reinterpret_cast<ElasticsearchClient*>(client);
        std::string info = esClient->getServerInfo();
        return allocateString(info);
    } catch (...) {
        return nullptr;
    }
}

void ES_FreeString(char* str) {
    free(str);
}
