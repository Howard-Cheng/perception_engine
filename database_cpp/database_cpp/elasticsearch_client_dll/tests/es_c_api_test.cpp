// tests/es_c_api_test.cpp
// Test C API wrapper

#include "ElasticsearchClientAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

void printSeparator() {
    printf("=============================================\n");
}

int main() {
    printSeparator();
    printf(" Elasticsearch C API Test\n");
    printSeparator();
    printf("\n");
    
    const char* esUrl = "http://localhost:9200";
    const char* indexName = "test_c_api_index";
    
    printf("Creating client...\n");
    ESClient* client = ES_CreateClient(esUrl);
    
    if (!client) {
        printf("? Failed to create client!\n");
        return 1;
    }
    printf("? Client created\n\n");
    
    // Test connection
    printf("[Test 1] Connection\n");
    if (ES_TestConnection(client)) {
        printf("  ? Connected to Elasticsearch\n");
        
        char* info = ES_GetClusterInfo(client);
        if (info) {
            printf("  Cluster info: %.100s...\n", info);
            ES_FreeString(info);
        }
    } else {
        printf("  ? Connection failed\n");
    }
    printf("\n");
    
    // Test index management
    printf("[Test 2] Index Management\n");
    
    // Delete if exists
    ES_DeleteIndex(client, indexName);
    SLEEP_MS(500);
    
    // Create index
    if (ES_InitializeIndex(client, indexName)) {
        printf("  ? Index created: %s\n", indexName);
    } else {
        printf("  ? Failed to create index\n");
    }
    
    // Check exists
    if (ES_IndexExists(client, indexName)) {
        printf("  ? Index exists\n");
    } else {
        printf("  ? Index does not exist\n");
    }
    printf("\n");
    
    // Test document count
    printf("[Test 3] Document Count\n");
    int count = ES_GetDocumentCount(client, indexName);
    printf("  Document count: %d\n", count);
    printf("\n");
    
    // Test update
    printf("[Test 4] Update Document\n");
    const char* updateJson = "{\"doc\":{\"test\":\"value\"}}";
    if (ES_UpdateDocument(client, indexName, "test_doc_001", updateJson)) {
        printf("  ? Document updated\n");
    } else {
        printf("  ??  Update operation executed\n");
    }
    printf("\n");
    
    // Test refresh
    printf("[Test 5] Refresh Index\n");
    if (ES_RefreshIndex(client, indexName)) {
        printf("  ? Index refreshed\n");
    } else {
        printf("  ? Refresh failed\n");
    }
    printf("\n");
    
    // Cleanup
    printf("[Cleanup] Deleting test index...\n");
    ES_DeleteIndex(client, indexName);
    printf("  ? Done\n\n");
    
    // Destroy client
    printf("Destroying client...\n");
    ES_DestroyClient(client);
    printf("? Client destroyed\n\n");
    
    printSeparator();
    printf(" C API Test Complete\n");
    printSeparator();
    printf("\n");
    
    return 0;
}
