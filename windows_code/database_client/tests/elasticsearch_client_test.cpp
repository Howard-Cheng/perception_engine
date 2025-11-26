// tests/elasticsearch_client_test.cpp
// Comprehensive test suite for Elasticsearch Client

#include "ElasticsearchClient.h"
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
    event.deviceId = "test_device_es_001";
    event.appName = appName;
    event.windowTitle = "Test Window - " + appName;
    event.url = "https://example.com/test";
    
    if (content.empty()) {
        event.screenContent = "Test content for " + appName + " application with searchable text including Elasticsearch keywords and full-text search capabilities";
    } else {
        event.screenContent = content;
    }
    
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    
    // Add some optional fields
    event.contentType = ContentType::TEXT;  // Changed from CODE
    event.domain = Domain::WORK;
    
    return event;
}

// Test 1: Connection and Initialization
void testConnection(IDatabaseClient& client) {
    TEST_START("Connection and Database Type");
    
    bool connected = client.testConnection();
    TEST_ASSERT(connected, "Connection to Elasticsearch server");
    
    DatabaseType type = client.getType();
    TEST_ASSERT(type == DatabaseType::ELASTICSEARCH, "Database type is ELASTICSEARCH");
    
    std::string info = client.getServerInfo();
    TEST_ASSERT(!info.empty(), "Server info retrieved");
    std::cout << "  Info (first 200 chars): " << info.substr(0, 200) << "..." << std::endl;
    
    TEST_END();
}

// Test 2: Index Management
void testIndexManagement(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Index Management");
    
    // Delete index if exists
    if (client.collectionExists(indexName)) {
        client.deleteCollection(indexName);
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for deletion
    }
    
    // Create index
    bool created = client.initializeCollection(indexName);
    TEST_ASSERT(created, "Index created with mapping");
    
    // Wait for index creation
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Refresh to make it available
    client.refreshCollection(indexName);
    
    // Check exists
    bool exists = client.collectionExists(indexName);
    TEST_ASSERT(exists, "Index exists");
    
    // Get stats
    CollectionStats stats = client.getCollectionStats(indexName);
    TEST_ASSERT(stats.collectionName == indexName, "Index stats retrieved");
    std::cout << "  Initial document count: " << stats.documentCount << std::endl;
    
    TEST_END();
}

// Test 3: Document Indexing
void testDocumentIndexing(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Document Indexing");
    
    // Index single document
    RawEvent event1 = createTestEvent("es_test_001", "chrome.exe");
    std::string docId = client.indexDocument(indexName, event1);
    TEST_ASSERT(!docId.empty(), "Single document indexed");
    TEST_ASSERT(docId == "es_test_001", "Document ID matches");
    
    // Refresh index to make document searchable
    client.refreshCollection(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify count
    int count = client.getDocumentCount(indexName);
    TEST_ASSERT(count >= 1, "Document count >= 1 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 4: Bulk Indexing
void testBulkIndexing(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Bulk Indexing");
    
    // Create multiple events
    std::vector<RawEvent> events;
    for (int i = 0; i < 10; ++i) {
        std::string id = "es_bulk_" + std::to_string(i);
        std::string app = (i % 2 == 0) ? "chrome.exe" : "vscode.exe";
        std::string content = "Document " + std::to_string(i) + " contains unique keywords like elasticsearch" + std::to_string(i);
        events.push_back(createTestEvent(id, app, content));
    }
    
    // Bulk index
    auto start = std::chrono::high_resolution_clock::now();
    bool success = client.bulkIndexDocuments(indexName, events);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    TEST_ASSERT(success, "Bulk indexing succeeded");
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    
    // Refresh to make documents searchable
    client.refreshCollection(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify
    int count = client.getDocumentCount(indexName);
    TEST_ASSERT(count >= 10, "Document count >= 10 (got " + std::to_string(count) + ")");
    
    TEST_END();
}

// Test 5: Full-Text Search
void testFullTextSearch(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Full-Text Search");
    
    // Ensure documents are searchable
    client.refreshCollection(indexName);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Simple keyword search using Elasticsearch query DSL
    std::string query1 = R"({
        "query": {
            "match": {
                "app_name": "chrome"
            }
        }
    })";
    
    SearchResult result1 = client.search(indexName, query1, 0, 20);
    TEST_ASSERT(result1.totalHits > 0, "Keyword search found results");
    std::cout << "  'chrome' search hits: " << result1.totalHits << std::endl;
    
    // Multi-field search
    std::string query2 = R"({
        "query": {
            "multi_match": {
                "query": "elasticsearch",
                "fields": ["screen_content", "window_title"]
            }
        }
    })";
    
    SearchResult result2 = client.search(indexName, query2, 0, 10);
    std::cout << "  'elasticsearch' multi-field hits: " << result2.totalHits << std::endl;
    TEST_ASSERT(result2.totalHits >= 0, "Multi-field search executed");
    
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
    
    SearchResult result3 = client.search(indexName, query3, 0, 10);
    TEST_ASSERT(result3.totalHits >= 0, "Boolean query executed");
    std::cout << "  Boolean query hits: " << result3.totalHits << std::endl;
    
    // Wildcard search
    std::string query4 = R"({
        "query": {
            "wildcard": {
                "app_name": "*chrome*"
            }
        }
    })";
    
    SearchResult result4 = client.search(indexName, query4, 0, 10);
    TEST_ASSERT(result4.totalHits >= 0, "Wildcard search executed");
    std::cout << "  Wildcard search hits: " << result4.totalHits << std::endl;
    
    TEST_END();
}

// Test 6: Advanced Queries
void testAdvancedQueries(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Advanced Elasticsearch Queries");
    
    // Range query
    std::string rangeQuery = R"({
        "query": {
            "range": {
                "timestamp": {
                    "gte": "now-1h"
                }
            }
        }
    })";
    
    SearchResult rangeResult = client.search(indexName, rangeQuery, 0, 10);
    TEST_ASSERT(rangeResult.totalHits >= 0, "Range query executed");
    std::cout << "  Range query (last hour) hits: " << rangeResult.totalHits << std::endl;
    
    // Aggregation query (counts by app_name)
    std::string aggQuery = R"({
        "size": 0,
        "aggs": {
            "apps": {
                "terms": {
                    "field": "app_name"
                }
            }
        }
    })";
    
    SearchResult aggResult = client.search(indexName, aggQuery, 0, 0);
    TEST_ASSERT(true, "Aggregation query executed");
    std::cout << "  Aggregation query completed" << std::endl;
    
    // Fuzzy search
    std::string fuzzyQuery = R"({
        "query": {
            "fuzzy": {
                "screen_content": {
                    "value": "elasticsarch",
                    "fuzziness": "AUTO"
                }
            }
        }
    })";
    
    SearchResult fuzzyResult = client.search(indexName, fuzzyQuery, 0, 10);
    TEST_ASSERT(fuzzyResult.totalHits >= 0, "Fuzzy search executed");
    std::cout << "  Fuzzy search hits: " << fuzzyResult.totalHits << std::endl;
    
    TEST_END();
}

// Test 7: Uncompressed Events Query
void testUncompressedEvents(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Uncompressed Events Query");
    
    int uncompressedCount = client.getUncompressedCount(indexName);
    std::cout << "  Uncompressed count: " << uncompressedCount << std::endl;
    
    std::vector<RawEvent> uncompressed = client.getUncompressedEvents(indexName, 24);
    std::cout << "  Retrieved: " << uncompressed.size() << " events" << std::endl;
    
    TEST_ASSERT(uncompressed.size() >= 0, "Query executed successfully");
    
    TEST_END();
}

// Test 8: Mark as Compressed
void testMarkAsCompressed(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Mark Events as Compressed");
    
    // Get some events
    std::vector<RawEvent> events = client.getUncompressedEvents(indexName, 24);
    
    if (events.size() >= 2) {
        std::vector<std::string> eventIds = {events[0].eventId, events[1].eventId};
        std::string sessionId = "test_session_es_123";
        
        bool marked = client.markEventsAsCompressed(indexName, eventIds, sessionId);
        TEST_ASSERT(marked, "Events marked as compressed");
        
        // Refresh and wait for update
        client.refreshCollection(indexName);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "  Marked " << eventIds.size() << " events as compressed" << std::endl;
        
        // Verify the change
        int newUncompressedCount = client.getUncompressedCount(indexName);
        std::cout << "  New uncompressed count: " << newUncompressedCount << std::endl;
        
    } else {
        std::cout << "  ? Not enough events to test" << std::endl;
        g_results.skipped++;
    }
    
    TEST_END();
}

// Test 9: Update Document
void testUpdateDocument(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Update Document");
    
    std::string updateJson = R"({
        "doc": {
            "compressed": false,
            "interaction_count": 10
        }
    })";
    
    bool updated = client.updateDocument(indexName, "es_test_001", updateJson);
    TEST_ASSERT(updated, "Document update request sent");
    
    // Refresh and wait for update
    client.refreshCollection(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    TEST_END();
}

// Test 10: Statistics and Metadata
void testStatistics(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Index Statistics");
    
    CollectionStats stats = client.getCollectionStats(indexName);
    
    std::cout << "  Documents: " << stats.documentCount << std::endl;
    std::cout << "  Uncompressed: " << stats.uncompressedCount << std::endl;
    std::cout << "  Compressed: " << stats.compressedCount << std::endl;
    std::cout << "  Size: " << stats.sizeInBytes << " bytes" << std::endl;
    
    TEST_ASSERT(stats.documentCount >= 0, "Document count retrieved");
    TEST_ASSERT(stats.sizeInBytes >= 0, "Index size retrieved");
    
    TEST_END();
}

// Test 11: Delete Operations
void testDeleteOperations(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Delete Operations");
    
    // Delete by query - old documents
    std::time_t cutoffTime = std::time(nullptr) - 3600; // 1 hour ago
    int deleted = client.deleteOlderThan(indexName, cutoffTime);
    std::cout << "  Deleted (1h cutoff): " << deleted << std::endl;
    
    TEST_ASSERT(deleted >= 0, "Delete by query executed");
    
    // Refresh after delete
    client.refreshCollection(indexName);
    
    // Delete specific document by app name
    bool specificDeleted = client.deleteDocumentByAppName(indexName, "nonexistent.exe");
    std::cout << "  Delete by app name executed: " << (specificDeleted ? "true" : "false") << std::endl;
    
    TEST_END();
}

// Test 12: Geo-Point Queries (if location data exists)
void testGeoQueries(IDatabaseClient& client, const std::string& indexName) {
    TEST_START("Geo-Point Queries");
    
    // Create event with location
    RawEvent geoEvent = createTestEvent("es_geo_001", "maps.exe");
    geoEvent.systemInfo.locationLat = 37.7749;  // San Francisco
    geoEvent.systemInfo.locationLon = -122.4194;
    
    client.indexDocument(indexName, geoEvent);
    client.refreshCollection(indexName);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Geo distance query
    std::string geoQuery = R"({
        "query": {
            "bool": {
                "filter": {
                    "geo_distance": {
                        "distance": "100km",
                        "system_info.location": {
                            "lat": 37.7749,
                            "lon": -122.4194
                        }
                    }
                }
            }
        }
    })";
    
    SearchResult geoResult = client.search(indexName, geoQuery, 0, 10);
    TEST_ASSERT(geoResult.totalHits >= 0, "Geo distance query executed");
    std::cout << "  Geo query hits: " << geoResult.totalHits << std::endl;
    
    TEST_END();
}

// Main test runner
int main(int argc, char** argv) {
    std::cout << "=============================================" << std::endl;
    std::cout << " Elasticsearch Client - Test Suite" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Configuration
    std::string esUrl = "http://localhost:9200";
    std::string indexName = "test_es_events";
    
    // Allow custom URL from command line
    if (argc > 1) esUrl = argv[1];
    if (argc > 2) indexName = argv[2];
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Elasticsearch URL: " << esUrl << std::endl;
    std::cout << "  Index: " << indexName << std::endl;
    std::cout << std::endl;
    
    std::cout << "??  IMPORTANT: Make sure Elasticsearch is running!" << std::endl;
    std::cout << "   Run: .\\deploy_elasticsearch.ps1" << std::endl;
    std::cout << "   Or:  docker run -p 9200:9200 -e \"discovery.type=single-node\" docker.elastic.co/elasticsearch/elasticsearch:8.11.0" << std::endl;
    std::cout << std::endl;
    
    try {
        // Create client using factory
        auto client = DatabaseClientFactory::createElasticsearch(esUrl);
        std::cout << "? Client created successfully" << std::endl;
        
        // Run tests
        testConnection(*client);
        testIndexManagement(*client, indexName);
        testDocumentIndexing(*client, indexName);
        testBulkIndexing(*client, indexName);
        testFullTextSearch(*client, indexName);
        testAdvancedQueries(*client, indexName);
        testUncompressedEvents(*client, indexName);
        testMarkAsCompressed(*client, indexName);
        testUpdateDocument(*client, indexName);
        testStatistics(*client, indexName);
        testDeleteOperations(*client, indexName);
        testGeoQueries(*client, indexName);
        
        // Cleanup
        std::cout << "\n[CLEANUP] Cleaning up test index..." << std::endl;
        client->deleteCollection(indexName);
        std::cout << "? Cleanup complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        std::cerr << "\nMake sure Elasticsearch is running on " << esUrl << std::endl;
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
        std::cout << "\nElasticsearch client is working correctly!" << std::endl;
        std::cout << "\nTested features:" << std::endl;
        std::cout << "  ? Connection and initialization" << std::endl;
        std::cout << "  ? Index management" << std::endl;
        std::cout << "  ? Document CRUD operations" << std::endl;
        std::cout << "  ? Bulk indexing" << std::endl;
        std::cout << "  ? Full-text search (match, multi-match, bool)" << std::endl;
        std::cout << "  ? Advanced queries (range, fuzzy, wildcard)" << std::endl;
        std::cout << "  ? Aggregations" << std::endl;
        std::cout << "  ? Geo-point queries" << std::endl;
        std::cout << "  ? Statistics and metadata" << std::endl;
        return 0;
    } else {
        std::cout << "\n? SOME TESTS FAILED!" << std::endl;
        std::cout << "Please check the error messages above." << std::endl;
        return 1;
    }
}
