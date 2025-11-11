// tests/es_client_test.cpp
// Comprehensive test for Elasticsearch Client DLL

#include "ElasticsearchClient.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cassert>

using namespace elasticsearch;

// Test result tracking
struct TestResults {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

TestResults g_results;

// Helper macros
#define TEST_START(name) \
    std::cout << "\n[TEST] " << name << std::endl; \
    bool testPassed = true;

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        std::cout << "  ? FAIL: " << message << std::endl; \
        testPassed = false; \
    } else { \
        std::cout << "  ? " << message << std::endl; \
    }

#define TEST_END() \
    if (testPassed) { \
        std::cout << "  ? PASSED" << std::endl; \
        g_results.passed++; \
    } else { \
        std::cout << "  ? FAILED" << std::endl; \
        g_results.failed++; \
    }

// Helper: Create test event
RawEvent createTestEvent(const std::string& id, const std::string& appName) {
    RawEvent event;
    event.eventId = id;
    event.timestamp = std::time(nullptr);
    event.createdAt = std::time(nullptr);
    event.deviceId = "test_device_001";
    event.appName = appName;
    event.windowTitle = "Test Window - " + appName;
    event.url = "https://example.com/test";
    event.screenContent = "Test content for " + appName + " application";
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    
    // System info
    event.systemInfo.batteryPercent = 85;
    event.systemInfo.isCharging = false;
    event.systemInfo.networkType = "WiFi";
    event.systemInfo.cpuUsage = 25.5;
    event.systemInfo.memoryUsage = 60.2;
    
    return event;
}

// Test 1: Connection
void testConnection(ElasticsearchClient& client) {
    TEST_START("Connection Test");
    
    bool connected = client.testConnection();
    TEST_ASSERT(connected, "Connection to Elasticsearch");
    
    if (connected) {
        std::string info = client.getClusterInfo();
        TEST_ASSERT(!info.empty(), "Cluster info retrieved");
        std::cout << "  Info: " << info.substr(0, 100) << "..." << std::endl;
    }
    
    TEST_END();
}

// Test 2: Index management
void testIndexManagement(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Index Management");
    
    // Delete if exists
    client.deleteIndex(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Create index
    bool created = client.initializeIndex(indexName);
    TEST_ASSERT(created, "Index created");
    
    // Check exists
    bool exists = client.indexExists(indexName);
    TEST_ASSERT(exists, "Index exists");
    
    // Get stats
    IndexStats stats = client.getIndexStats(indexName);
    TEST_ASSERT(stats.indexName == indexName, "Index stats retrieved");
    std::cout << "  Index size: " << stats.sizeInBytes << " bytes" << std::endl;
    
    TEST_END();
}

// Test 3: Document indexing
void testDocumentIndexing(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Document Indexing");
    
    // Index single document
    RawEvent event1 = createTestEvent("test_event_001", "chrome.exe");
    std::string docId = client.indexDocument(indexName, event1);
    TEST_ASSERT(!docId.empty(), "Single document indexed");
    TEST_ASSERT(docId == "test_event_001", "Document ID matches");
    
    // Refresh to make searchable
    client.refreshIndex(indexName);
    
    // Verify count
    int count = client.getDocumentCount(indexName);
    TEST_ASSERT(count == 1, "Document count is 1 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 4: Bulk indexing
void testBulkIndexing(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Bulk Indexing");
    
    // Create multiple events
    std::vector<RawEvent> events;
    for (int i = 0; i < 10; ++i) {
        std::string id = "bulk_event_" + std::to_string(i);
        std::string app = (i % 2 == 0) ? "chrome.exe" : "vscode.exe";
        events.push_back(createTestEvent(id, app));
    }
    
    // Bulk index
    auto start = std::chrono::high_resolution_clock::now();
    bool success = client.bulkIndexDocuments(indexName, events);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    TEST_ASSERT(success, "Bulk indexing succeeded");
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    
    // Refresh and verify
    client.refreshIndex(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    int count = client.getDocumentCount(indexName);
    TEST_ASSERT(count == 11, "Total documents is 11 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 5: Search functionality
void testSearch(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Search Functionality");
    
    // Match all query
    std::string matchAllQuery = R"({
        "query": {
            "match_all": {}
        },
        "size": 20
    })";
    
    SearchResult result = client.search(indexName, matchAllQuery, 0, 20);
    TEST_ASSERT(result.totalHits > 0, "Match all query returned results");
    std::cout << "  Total hits: " << result.totalHits << std::endl;
    std::cout << "  Returned: " << result.events.size() << std::endl;
    
    // Term query (app_name)
    std::string termQuery = R"({
        "query": {
            "term": {
                "app_name": "chrome.exe"
            }
        }
    })";
    
    SearchResult chromeResults = client.search(indexName, termQuery, 0, 10);
    TEST_ASSERT(chromeResults.totalHits > 0, "Term query found chrome.exe events");
    std::cout << "  Chrome events: " << chromeResults.totalHits << std::endl;
    
    // Full-text search
    std::string textQuery = R"({
        "query": {
            "match": {
                "screen_content": "application"
            }
        }
    })";
    
    SearchResult textResults = client.search(indexName, textQuery, 0, 10);
    TEST_ASSERT(textResults.totalHits > 0, "Full-text search found results");
    std::cout << "  Text search hits: " << textResults.totalHits << std::endl;
    
    TEST_END();
}

// Test 6: Uncompressed events
void testUncompressedEvents(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Uncompressed Events Query");
    
    int uncompressedCount = client.getUncompressedCount(indexName);
    TEST_ASSERT(uncompressedCount > 0, "Uncompressed count > 0");
    std::cout << "  Uncompressed: " << uncompressedCount << std::endl;
    
    std::vector<RawEvent> uncompressed = client.getUncompressedEvents(indexName, 24);
    TEST_ASSERT(uncompressed.size() > 0, "Retrieved uncompressed events");
    std::cout << "  Retrieved: " << uncompressed.size() << std::endl;
    
    TEST_END();
}

// Test 7: Mark as compressed
void testMarkAsCompressed(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Mark Events as Compressed");
    
    // Get some uncompressed events
    std::vector<RawEvent> events = client.getUncompressedEvents(indexName, 24);
    
    if (events.size() >= 2) {
        std::vector<std::string> eventIds = {events[0].eventId, events[1].eventId};
        std::string sessionId = "test_session_123";
        
        bool marked = client.markEventsAsCompressed(indexName, eventIds, sessionId);
        TEST_ASSERT(marked, "Events marked as compressed");
        
        // Refresh and verify
        client.refreshIndex(indexName);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        int newUncompressedCount = client.getUncompressedCount(indexName);
        TEST_ASSERT(newUncompressedCount == events.size() - 2, 
                   "Uncompressed count decreased by 2");
        std::cout << "  New uncompressed count: " << newUncompressedCount << std::endl;
    } else {
        std::cout << "  ??  Not enough events to test" << std::endl;
        g_results.skipped++;
    }
    
    TEST_END();
}

// Test 8: Update document
void testUpdateDocument(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Update Document");
    
    std::string updateJson = R"({
        "doc": {
            "interaction_count": 100
        }
    })";
    
    bool updated = client.updateDocument(indexName, "test_event_001", updateJson);
    TEST_ASSERT(updated, "Document updated");
    
    TEST_END();
}

// Test 9: Delete old documents
void testDeleteOld(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Delete Old Documents");
    
    // Delete documents older than 1 hour from now (should delete nothing)
    std::time_t futureTime = std::time(nullptr) + 3600;
    int deleted = client.deleteOlderThan(indexName, futureTime);
    TEST_ASSERT(deleted >= 0, "Delete operation executed");
    std::cout << "  Deleted: " << deleted << std::endl;
    
    TEST_END();
}

// Test 10: Statistics
void testStatistics(ElasticsearchClient& client, const std::string& indexName) {
    TEST_START("Index Statistics");
    
    IndexStats stats = client.getIndexStats(indexName);
    
    TEST_ASSERT(stats.documentCount > 0, "Document count > 0");
    TEST_ASSERT(stats.sizeInBytes > 0, "Index size > 0");
    
    std::cout << "  Documents: " << stats.documentCount << std::endl;
    std::cout << "  Uncompressed: " << stats.uncompressedCount << std::endl;
    std::cout << "  Compressed: " << stats.compressedCount << std::endl;
    std::cout << "  Size: " << stats.sizeInBytes << " bytes ("
              << (stats.sizeInBytes / 1024.0) << " KB)" << std::endl;
    
    TEST_END();
}

// Main test runner
int main() {
    std::cout << "=============================================" << std::endl;
    std::cout << " Elasticsearch Client DLL - Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    std::string esUrl = "http://localhost:9200";
    std::string indexName = "test_events";
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  ES URL: " << esUrl << std::endl;
    std::cout << "  Index: " << indexName << std::endl;
    
    try {
        // Create client
        ElasticsearchClient client(esUrl);
        std::cout << "\n? Client created successfully" << std::endl;
        
        // Run tests
        testConnection(client);
        testIndexManagement(client, indexName);
        testDocumentIndexing(client, indexName);
        testBulkIndexing(client, indexName);
        testSearch(client, indexName);
        testUncompressedEvents(client, indexName);
        testMarkAsCompressed(client, indexName);
        testUpdateDocument(client, indexName);
        testDeleteOld(client, indexName);
        testStatistics(client, indexName);
        
        // Cleanup
        std::cout << "\n[CLEANUP] Deleting test index..." << std::endl;
        client.deleteIndex(indexName);
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        return 1;
    }
    
    // Print summary
    std::cout << "\n=============================================" << std::endl;
    std::cout << " Test Summary" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "  ? Passed:  " << g_results.passed << std::endl;
    std::cout << "  ? Failed:  " << g_results.failed << std::endl;
    std::cout << "  ??  Skipped: " << g_results.skipped << std::endl;
    std::cout << "  Total:    " << (g_results.passed + g_results.failed + g_results.skipped) << std::endl;
    std::cout << "=============================================" << std::endl;
    
    if (g_results.failed == 0) {
        std::cout << "\n?? ALL TESTS PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\n? SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}
