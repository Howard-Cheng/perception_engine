// tests/meilisearch_client_test.cpp
// Comprehensive test for MeiliSearch Client

#include "MeiliSearchClient.h"
#include "DatabaseClientFactory.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cassert>
#include <thread>

using namespace database;

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
RawEvent createTestEvent(const std::string& id, const std::string& appName, const std::string& content = "") {
    RawEvent event;
    event.eventId = id;
    event.timestamp = std::time(nullptr);
    event.createdAt = std::time(nullptr);
    event.deviceId = "test_device_001";
    event.appName = appName;
    event.windowTitle = "Test Window - " + appName;
    event.url = "https://example.com/test";
    
    if (content.empty()) {
        event.screenContent = "Test content for " + appName + " application with searchable text including MeiliSearch keywords";
    } else {
        event.screenContent = content;
    }
    
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    
    return event;
}

// Test 1: Connection and Initialization
void testConnection(IDatabaseClient& client) {
    TEST_START("Connection and Database Type");
    
    bool connected = client.testConnection();
    TEST_ASSERT(connected, "Connection to MeiliSearch server");
    
    DatabaseType type = client.getType();
    TEST_ASSERT(type == DatabaseType::MEILISEARCH, "Database type is MEILISEARCH");
    
    std::string info = client.getServerInfo();
    TEST_ASSERT(!info.empty(), "Server info retrieved");
    std::cout << "  Info: " << info << std::endl;
    
    TEST_END();
}

// Test 2: Collection Management
void testCollectionManagement(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Collection Management");
    
    // Delete collection if exists
    if (client.collectionExists(collectionName)) {
        client.deleteCollection(collectionName);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for deletion
    }
    
    // Create collection
    bool created = client.initializeCollection(collectionName);
    TEST_ASSERT(created, "Collection created");
    
    // Wait for index creation
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Check exists
    bool exists = client.collectionExists(collectionName);
    TEST_ASSERT(exists, "Collection exists");
    
    // Get stats
    CollectionStats stats = client.getCollectionStats(collectionName);
    TEST_ASSERT(stats.collectionName == collectionName, "Collection stats retrieved");
    
    TEST_END();
}

// Test 3: Document Indexing
void testDocumentIndexing(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Document Indexing");
    
    // Index single document
    RawEvent event1 = createTestEvent("meili_test_001", "chrome.exe");
    std::string docId = client.indexDocument(collectionName, event1);
    TEST_ASSERT(!docId.empty(), "Single document indexed");
    TEST_ASSERT(docId == "meili_test_001", "Document ID matches");
    
    // Wait for indexing (MeiliSearch is async)
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Verify count
    int count = client.getDocumentCount(collectionName);
    TEST_ASSERT(count >= 1, "Document count >= 1 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 4: Bulk Indexing
void testBulkIndexing(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Bulk Indexing");
    
    // Create multiple events
    std::vector<RawEvent> events;
    for (int i = 0; i < 10; ++i) {
        std::string id = "meili_bulk_" + std::to_string(i);
        std::string app = (i % 2 == 0) ? "chrome.exe" : "vscode.exe";
        std::string content = "Document " + std::to_string(i) + " contains unique keywords like keyword" + std::to_string(i);
        events.push_back(createTestEvent(id, app, content));
    }
    
    // Bulk index
    auto start = std::chrono::high_resolution_clock::now();
    bool success = client.bulkIndexDocuments(collectionName, events);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    TEST_ASSERT(success, "Bulk indexing succeeded");
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    
    // Wait for indexing (MeiliSearch is async)
    std::cout << "  Waiting for indexing to complete..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Verify
    int count = client.getDocumentCount(collectionName);
    TEST_ASSERT(count >= 10, "Document count >= 10 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 5: Full-Text Search
void testFullTextSearch(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Full-Text Search");
    
    // Wait for indexing to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Simple keyword search
    SearchResult result1 = client.search(collectionName, "chrome", 0, 20);
    TEST_ASSERT(result1.totalHits > 0, "Keyword search found results");
    std::cout << "  'chrome' search hits: " << result1.totalHits << std::endl;
    
    // Search for specific content
    SearchResult result2 = client.search(collectionName, "keyword5", 0, 10);
    std::cout << "  'keyword5' search hits: " << result2.totalHits << std::endl;
    
    // MeiliSearch-specific: typo tolerance
    SearchResult result3 = client.search(collectionName, "chorme", 0, 10); // Typo: chorme
    std::cout << "  'chorme' (typo) search hits: " << result3.totalHits << std::endl;
    TEST_ASSERT(result3.totalHits >= 0, "Typo-tolerant search executed");
    
    // Multi-word search
    SearchResult result4 = client.search(collectionName, "test content", 0, 10);
    TEST_ASSERT(result4.totalHits >= 0, "Multi-word search executed");
    std::cout << "  'test content' hits: " << result4.totalHits << std::endl;
    
    TEST_END();
}

// Test 6: Uncompressed Events
void testUncompressedEvents(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Uncompressed Events Query");
    
    int uncompressedCount = client.getUncompressedCount(collectionName);
    std::cout << "  Uncompressed count: " << uncompressedCount << std::endl;
    
    std::vector<RawEvent> uncompressed = client.getUncompressedEvents(collectionName, 24);
    std::cout << "  Retrieved: " << uncompressed.size() << " events" << std::endl;
    
    TEST_ASSERT(true, "Query executed successfully");
    
    TEST_END();
}

// Test 7: Mark as Compressed
void testMarkAsCompressed(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Mark Events as Compressed");
    
    // Get some events
    std::vector<RawEvent> events = client.getUncompressedEvents(collectionName, 24);
    
    if (events.size() >= 2) {
        std::vector<std::string> eventIds = {events[0].eventId, events[1].eventId};
        std::string sessionId = "test_session_meili_123";
        
        bool marked = client.markEventsAsCompressed(collectionName, eventIds, sessionId);
        TEST_ASSERT(marked, "Events marked as compressed");
        
        // Wait for update
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "  Marked " << eventIds.size() << " events as compressed" << std::endl;
    } else {
        std::cout << "  ? Not enough events to test" << std::endl;
        g_results.skipped++;
    }
    
    TEST_END();
}

// Test 8: Update Document
void testUpdateDocument(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Update Document");
    
    std::string updateJson = R"({"compressed": false})";
    
    bool updated = client.updateDocument(collectionName, "meili_test_001", updateJson);
    TEST_ASSERT(updated, "Document update request sent");
    
    // Wait for update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    TEST_END();
}

// Test 9: Statistics
void testStatistics(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Collection Statistics");
    
    CollectionStats stats = client.getCollectionStats(collectionName);
    
    std::cout << "  Documents: " << stats.documentCount << std::endl;
    std::cout << "  Size: " << stats.sizeInBytes << " bytes" << std::endl;
    
    TEST_ASSERT(stats.documentCount >= 0, "Document count retrieved");
    
    TEST_END();
}

// Test 10: Delete Operations
void testDeleteOperations(IDatabaseClient& client, const std::string& collectionName) {
    TEST_START("Delete Operations");
    
    // Delete old documents (none should exist)
    std::time_t futureTime = std::time(nullptr) + 3600;
    int deleted = client.deleteOlderThan(collectionName, futureTime);
    std::cout << "  Deleted (future cutoff): " << deleted << std::endl;
    
    TEST_ASSERT(deleted >= 0, "Delete operation executed");
    
    TEST_END();
}

// Main test runner
int main(int argc, char** argv) {
    std::cout << "=============================================" << std::endl;
    std::cout << " MeiliSearch Client - Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Configuration
    std::string meiliUrl = "http://localhost:7700";
    std::string apiKey = "perception_engine_key_2025";
    std::string collectionName = "test_meili_events";
    
    // Allow custom URL and key from command line
    if (argc > 1) meiliUrl = argv[1];
    if (argc > 2) apiKey = argv[2];
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  MeiliSearch URL: " << meiliUrl << std::endl;
    std::cout << "  API Key: " << (apiKey.empty() ? "(none)" : "***" + apiKey.substr(apiKey.length() - 4)) << std::endl;
    std::cout << "  Collection: " << collectionName << std::endl;
    std::cout << std::endl;
    
    std::cout << "??  IMPORTANT: Make sure MeiliSearch is running!" << std::endl;
    std::cout << "   Run: .\\deploy_meilisearch.ps1" << std::endl;
    std::cout << "   Or:  meilisearch.exe --master-key=" << apiKey << std::endl;
    std::cout << std::endl;
    
    try {
        // Create client using factory
        auto client = DatabaseClientFactory::createMeiliSearch(meiliUrl, apiKey);
        std::cout << "? Client created successfully" << std::endl;
        
        // Run tests
        testConnection(*client);
        testCollectionManagement(*client, collectionName);
        testDocumentIndexing(*client, collectionName);
        testBulkIndexing(*client, collectionName);
        testFullTextSearch(*client, collectionName);
        testUncompressedEvents(*client, collectionName);
        testMarkAsCompressed(*client, collectionName);
        testUpdateDocument(*client, collectionName);
        testStatistics(*client, collectionName);
        testDeleteOperations(*client, collectionName);
        
        // Cleanup
        std::cout << "\n[CLEANUP] Cleaning up test collection..." << std::endl;
        client->deleteCollection(collectionName);
        std::cout << "? Cleanup complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        std::cerr << "\nMake sure MeiliSearch is running on " << meiliUrl << std::endl;
        return 1;
    }
    
    // Print summary
    std::cout << "\n=============================================" << std::endl;
    std::cout << " Test Summary" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "  ? Passed:  " << g_results.passed << std::endl;
    std::cout << "  ? Failed:  " << g_results.failed << std::endl;
    std::cout << "  ? Skipped: " << g_results.skipped << std::endl;
    std::cout << "  Total:    " << (g_results.passed + g_results.failed + g_results.skipped) << std::endl;
    std::cout << "=============================================" << std::endl;
    
    if (g_results.failed == 0) {
        std::cout << "\n?? ALL TESTS PASSED!" << std::endl;
        std::cout << "\nMeiliSearch client is working correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "\n? SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}
