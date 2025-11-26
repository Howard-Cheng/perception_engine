// include/ElasticsearchClientAPI.h
// C API wrapper for Elasticsearch Client
#pragma once

#include "ElasticsearchTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct ESClient ESClient;

// Client management
ES_CLIENT_API ESClient* ES_CreateClient(const char* esUrl);
ES_CLIENT_API void ES_DestroyClient(ESClient* client);

// Index operations
ES_CLIENT_API int ES_InitializeIndex(ESClient* client, const char* indexName);
ES_CLIENT_API int ES_DeleteIndex(ESClient* client, const char* indexName);
ES_CLIENT_API int ES_IndexExists(ESClient* client, const char* indexName);
ES_CLIENT_API int ES_RefreshIndex(ESClient* client, const char* indexName);

// Document operations
ES_CLIENT_API char* ES_IndexDocument(ESClient* client, const char* indexName, 
                                     const char* eventJson);
ES_CLIENT_API int ES_UpdateDocument(ESClient* client, const char* indexName,
                                   const char* docId, const char* updateJson);

// Statistics
ES_CLIENT_API int ES_GetDocumentCount(ESClient* client, const char* indexName);
ES_CLIENT_API int ES_GetUncompressedCount(ESClient* client, const char* indexName);

// Utility
ES_CLIENT_API int ES_TestConnection(ESClient* client);
ES_CLIENT_API char* ES_GetClusterInfo(ESClient* client);

// Memory management for returned strings
ES_CLIENT_API void ES_FreeString(char* str);

#ifdef __cplusplus
}
#endif
