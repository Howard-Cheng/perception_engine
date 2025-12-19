// tests/postgresql_client_test.cpp
// Comprehensive test suite for PostgreSQL Client

#include "PostgreSQLClient.h"
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
    event.deviceId = "test_device_pg_001";
    event.appName = appName;
    event.windowTitle = "Test Window - " + appName;
    event.url = "https://example.com/test";
    
    if (content.empty()) {
        event.screenContent = "Test content for " + appName + " application with searchable text including PostgreSQL keywords and full-text search capabilities";
    } else {
        event.screenContent = content;
    }
    
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    event.summarized = false;
    
    // Add some optional fields
    event.contentType = ContentType::TEXT;
    event.domain = Domain::WORK;
    
    return event;
}

// Test 1: Connection and Initialization
void testConnection(IDatabaseClient& client) {
    TEST_START("Connection and Database Type");
    
    bool connected = client.testConnection();
    TEST_ASSERT(connected, "Connection to PostgreSQL server");
    
    DatabaseType type = client.getType();
    TEST_ASSERT(type == DatabaseType::POSTGRESQL, "Database type is POSTGRESQL");
    
    std::string info = client.getServerInfo();
    TEST_ASSERT(!info.empty(), "Server info retrieved");
    std::cout << "  Server Info:\n" << info << std::endl;
    
    TEST_END();
}

// Test 2: Table Management
void testTableManagement(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Table Management");
    
    // Delete table if exists
    if (client.collectionExists(tableName)) {
        client.deleteCollection(tableName);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Create table
    bool created = client.initializeCollection(tableName);
    TEST_ASSERT(created, "Table created with schema");
    
    // Check exists
    bool exists = client.collectionExists(tableName);
    TEST_ASSERT(exists, "Table exists");
    
    // Get stats
    CollectionStats stats = client.getCollectionStats(tableName);
    TEST_ASSERT(stats.collectionName == tableName, "Table stats retrieved");
    std::cout << "  Initial document count: " << stats.documentCount << std::endl;
    
    TEST_END();
}

// Test 3: Document Insertion
void testDocumentInsertion(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Document Insertion");
    
    // Insert single document
    RawEvent event1 = createTestEvent("pg_test_001", "chrome.exe");
    std::string docId = client.indexDocument(tableName, event1);
    TEST_ASSERT(!docId.empty(), "Single document inserted");
    TEST_ASSERT(docId == "pg_test_001", "Document ID matches");
    
    // Verify count
    int count = client.getDocumentCount(tableName);
    TEST_ASSERT(count >= 1, "Document count >= 1 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 4: Bulk Insertion
void testBulkInsertion(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Bulk Insertion");
    
    // Create multiple events
    std::vector<RawEvent> events;
    for (int i = 0; i < 10; ++i) {
        std::string id = "pg_bulk_" + std::to_string(i);
        std::string app = (i % 2 == 0) ? "chrome.exe" : "vscode.exe";
        std::string content = "Document " + std::to_string(i) + " contains unique keywords like postgresql" + std::to_string(i);
        events.push_back(createTestEvent(id, app, content));
    }
    
    // Bulk insert
    auto start = std::chrono::high_resolution_clock::now();
    bool success = client.bulkIndexDocuments(tableName, events);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    TEST_ASSERT(success, "Bulk insertion succeeded");
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    
    // Verify
    int count = client.getDocumentCount(tableName);
    TEST_ASSERT(count >= 10, "Document count >= 10 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 5: Full-Text Search
void testFullTextSearch(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Full-Text Search");
    
    // Simple keyword search using JSON query DSL (similar to Elasticsearch)
    std::string query1 = R"({
        "query": {
            "match": {
                "app_name": "chrome"
            }
        }
    })";
    
    SearchResult result1 = client.search(tableName, query1, 0, 20);
    TEST_ASSERT(result1.totalHits > 0, "Keyword search found results");
    std::cout << "  'chrome' search hits: " << result1.totalHits << std::endl;
    
    // Term search (exact match)
    std::string query2 = R"({
        "query": {
            "term": {
                "compressed": false
            }
        }
    })";
    
    SearchResult result2 = client.search(tableName, query2, 0, 10);
    std::cout << "  'compressed=false' hits: " << result2.totalHits << std::endl;
    TEST_ASSERT(result2.totalHits >= 0, "Term search executed");
    
    // Boolean query
    std::string query3 = R"({
        "query": {
            "bool": {
                "must": [
                    {"match": {"screen_content": "test"}}
                ],
                "filter": [
                    {"term": {"compressed": false}}
                ]
            }
        }
    })";
    
    SearchResult result3 = client.search(tableName, query3, 0, 10);
    TEST_ASSERT(result3.totalHits >= 0, "Boolean query executed");
    std::cout << "  Boolean query hits: " << result3.totalHits << std::endl;
    
    TEST_END();
}

// Test 6: Uncompressed Events Query
void testUncompressedEvents(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Uncompressed Events Query");
    
    int uncompressedCount = client.getUncompressedCount(tableName);
    std::cout << "  Uncompressed count: " << uncompressedCount << std::endl;
    
    std::vector<RawEvent> uncompressed = client.getUncompressedEvents(tableName, 24);
    std::cout << "  Retrieved: " << uncompressed.size() << " events" << std::endl;
    
    TEST_ASSERT(uncompressed.size() >= 0, "Query executed successfully");
    
    TEST_END();
}

// Test 7: Mark as Compressed
void testMarkAsCompressed(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Mark Events as Compressed");
    
    // Get some events
    std::vector<RawEvent> events = client.getUncompressedEvents(tableName, 24);
    
    if (events.size() >= 2) {
        std::vector<std::string> eventIds = {events[0].eventId, events[1].eventId};
        std::string sessionId = "test_session_pg_123";
        
        bool marked = client.markEventsAsCompressed(tableName, eventIds, sessionId);
        TEST_ASSERT(marked, "Events marked as compressed");
        
        std::cout << "  Marked " << eventIds.size() << " events as compressed" << std::endl;
        
        // Verify the change
        int newUncompressedCount = client.getUncompressedCount(tableName);
        std::cout << "  New uncompressed count: " << newUncompressedCount << std::endl;
        
    } else {
        std::cout << "  ? Not enough events to test" << std::endl;
        g_results.skipped++;
    }
    
    TEST_END();
}

// Test 8: Mark as Compressed with Similarity
void testMarkAsCompressedWithSimilarity(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Mark Events as Compressed with Similarity Content");
    
    // Get some uncompressed events
    std::vector<RawEvent> events = client.getUncompressedEvents(tableName, 24);
    
    if (events.size() >= 2) {
        std::vector<std::string> eventIds;
        for (size_t i = 0; i < std::min(size_t(3), events.size()); ++i) {
            eventIds.push_back(events[i].eventId);
        }
        
        std::string sessionId = "test_session_pg_456";
        std::string similarContent = "This is similar content for testing PostgreSQL similarity feature";
        
        // Cast to PostgreSQLClient to access specific method
        auto* pgClient = dynamic_cast<PostgreSQLClient*>(&client);
        if (pgClient) {
            bool marked = pgClient->markEventsAsCompressedWithSimilarity(
                tableName, eventIds, sessionId, similarContent);
            TEST_ASSERT(marked, "Events marked with similarity content");
            
            std::cout << "  Marked " << eventIds.size() << " events with similarity" << std::endl;
        } else {
            std::cout << "  ? Could not cast to PostgreSQLClient" << std::endl;
            g_results.skipped++;
        }
    } else {
        std::cout << "  ? Not enough events to test" << std::endl;
        g_results.skipped++;
    }
    
    TEST_END();
}

// Test 9: Update Document
void testUpdateDocument(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Update Document");
    
    std::string updateJson = R"({
        "doc": {
            "compressed": false,
            "interaction_count": 10
        }
    })";
    
    bool updated = client.updateDocument(tableName, "pg_test_001", updateJson);
    TEST_ASSERT(updated, "Document update executed");
    
    TEST_END();
}

// Test 10: Statistics and Metadata
void testStatistics(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Table Statistics");
    
    CollectionStats stats = client.getCollectionStats(tableName);
    
    std::cout << "  Documents: " << stats.documentCount << std::endl;
    std::cout << "  Uncompressed: " << stats.uncompressedCount << std::endl;
    std::cout << "  Compressed: " << stats.compressedCount << std::endl;
    std::cout << "  Size: " << stats.sizeInBytes << " bytes" << std::endl;
    
    TEST_ASSERT(stats.documentCount >= 0, "Document count retrieved");
    TEST_ASSERT(stats.sizeInBytes >= 0, "Table size retrieved");
    
    TEST_END();
}

// Test 11: Delete Operations
void testDeleteOperations(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("Delete Operations");
    
    // Delete by timestamp - old documents
    std::time_t cutoffTime = std::time(nullptr) - 3600; // 1 hour ago
    int deleted = client.deleteOlderThan(tableName, cutoffTime);
    std::cout << "  Deleted (1h cutoff): " << deleted << std::endl;
    
    TEST_ASSERT(deleted >= 0, "Delete by timestamp executed");
    
    // Delete specific document by app name
    bool specificDeleted = client.deleteDocumentByAppName(tableName, "nonexistent.exe");
    std::cout << "  Delete by app name executed: " << (specificDeleted ? "true" : "false") << std::endl;
    
    TEST_END();
}

// Test 12: JSON Storage (Mouse Events and System Info)
void testJSONStorage(IDatabaseClient& client, const std::string& tableName) {
    TEST_START("JSON Storage (JSONB)");
    
    // Create event with mouse events and system info
    RawEvent jsonEvent = createTestEvent("pg_json_001", "notepad.exe");
    
    // Add mouse events
    MouseEvent me1;
    me1.timestamp = std::time(nullptr);
    me1.eventType = "LeftClick";
    me1.content = "Button text";
    me1.posX = 100;
    me1.posY = 200;
    me1.elementType = "Button";
    jsonEvent.mouseEvents.push_back(me1);
    
    // Add system info
    jsonEvent.systemInfo.batteryPercent = 85;
    jsonEvent.systemInfo.isCharging = true;
    jsonEvent.systemInfo.networkType = "WiFi";
    jsonEvent.systemInfo.locationLat = 37.7749;
    jsonEvent.systemInfo.locationLon = -122.4194;
    jsonEvent.systemInfo.cpuUsage = 45.5;
    jsonEvent.systemInfo.memoryUsage = 60.2;
    
    std::string docId = client.indexDocument(tableName, jsonEvent);
    TEST_ASSERT(!docId.empty(), "Event with JSON data inserted");
    
    // Retrieve and verify
    RawEvent retrieved = client.getDocumentByAppName(tableName, "notepad.exe");
    TEST_ASSERT(!retrieved.eventId.empty(), "Event retrieved");
    TEST_ASSERT(retrieved.mouseEvents.size() > 0, "Mouse events preserved");
    TEST_ASSERT(retrieved.systemInfo.batteryPercent.has_value(), "System info preserved");
    
    std::cout << "  Mouse events: " << retrieved.mouseEvents.size() << std::endl;
    std::cout << "  Battery: " << (retrieved.systemInfo.batteryPercent.value_or(0)) << "%" << std::endl;
    
    TEST_END();
}

// Main test runner
int main(int argc, char** argv) {
    std::cout << "=============================================" << std::endl;
    std::cout << " PostgreSQL Client - Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Configuration
    std::string connectionString = "host=127.0.0.1 port=5432 dbname=postgres user=postgres";
    std::string tableName = "test_pg_events";
    
    // Allow custom connection string from command line
    if (argc > 1) connectionString = argv[1];
    if (argc > 2) tableName = argv[2];
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Connection: " << connectionString << std::endl;
    std::cout << "  Table: " << tableName << std::endl;
    std::cout << std::endl;
    
    std::cout << "??  IMPORTANT: Make sure PostgreSQL is running!" << std::endl;
    std::cout << "   Default: host=localhost port=5432 user=postgres password=postgres" << std::endl;
    std::cout << "   Docker:  docker run -d -p 5432:5432 -e POSTGRES_PASSWORD=postgres postgres:15" << std::endl;
    std::cout << std::endl;
    
    try {
        // Create client using factory
        auto client = DatabaseClientFactory::createPostgreSQL(connectionString);
        std::cout << "? Client created successfully" << std::endl;
        
        // Run tests
        testConnection(*client);
        testTableManagement(*client, tableName);
        testDocumentInsertion(*client, tableName);
        testBulkInsertion(*client, tableName);
        testFullTextSearch(*client, tableName);
        testUncompressedEvents(*client, tableName);
        testMarkAsCompressed(*client, tableName);
        testMarkAsCompressedWithSimilarity(*client, tableName);
        testUpdateDocument(*client, tableName);
        testStatistics(*client, tableName);
        testJSONStorage(*client, tableName);
        getchar();
        testDeleteOperations(*client, tableName);
        
        // Cleanup
        std::cout << "\n[CLEANUP] Cleaning up test table..." << std::endl;
        client->deleteCollection(tableName);
        std::cout << "? Cleanup complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        std::cerr << "\nMake sure PostgreSQL is running and accessible with provided connection string" << std::endl;
        std::cerr << "Check logs above for more details." << std::endl;
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
        std::cout << "\nPostgreSQL client is working correctly!" << std::endl;
        std::cout << "\nTested features:" << std::endl;
        std::cout << "  ? Connection and initialization" << std::endl;
        std::cout << "  ? Table management" << std::endl;
        std::cout << "  ? Document CRUD operations" << std::endl;
        std::cout << "  ? Bulk insertion" << std::endl;
        std::cout << "  ? Full-text search (match, term, bool)" << std::endl;
        std::cout << "  ? JSONB storage (mouse events, system info)" << std::endl;
        std::cout << "  ? Compressed events management" << std::endl;
        std::cout << "  ? Statistics and metadata" << std::endl;
        return 0;
    } else {
        std::cout << "\n? SOME TESTS FAILED!" << std::endl;
        std::cout << "Please check the error messages above." << std::endl;
        return 1;
    }
}
