// examples/postgresql_example.cpp
// Example usage of PostgreSQL Client

#include "PostgreSQLClient.h"
#include "DatabaseClientFactory.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace database;

// Helper: Print event details
void printEvent(const RawEvent& event, int index = -1) {
    if (index >= 0) {
        std::cout << "\n[" << index << "] Event: " << event.eventId << std::endl;
    } else {
        std::cout << "\nEvent: " << event.eventId << std::endl;
    }
    std::cout << "  App: " << event.appName << std::endl;
    std::cout << "  Window: " << (event.windowTitle ? *event.windowTitle : "N/A") << std::endl;
    std::cout << "  Timestamp: " << std::put_time(std::localtime(&event.timestamp), "%Y-%m-%d %H:%M:%S") << std::endl;
    std::cout << "  Content: " << (event.screenContent ? event.screenContent->substr(0, 80) + "..." : "N/A") << std::endl;
    std::cout << "  Compressed: " << (event.compressed ? "Yes" : "No") << std::endl;
    std::cout << "  Session ID: " << (event.sessionId ? *event.sessionId : "N/A") << std::endl;
}

// Helper: Create sample event
RawEvent createSampleEvent(const std::string& id, const std::string& appName) {
    RawEvent event;
    event.eventId = id;
    event.timestamp = std::time(nullptr);
    event.createdAt = std::time(nullptr);
    event.deviceId = "device_001";
    event.appName = appName;
    event.windowTitle = "Example Window - " + appName;
    event.url = "https://example.com";
    event.screenContent = "This is sample content for " + appName + 
                         " with searchable keywords like PostgreSQL, database, SQL, and full-text search.";
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    event.summarized = false;
    event.contentType = ContentType::TEXT;
    event.domain = Domain::WORK;
    
    return event;
}

int main() {
    std::cout << "================================================" << std::endl;
    std::cout << " PostgreSQL Database Client - Example Usage" << std::endl;
    std::cout << "================================================\n" << std::endl;
    
    // Configuration
    std::string connectionString = "host=localhost port=5432 dbname=perception user=postgres password=postgres";
    std::string tableName = "example_events";
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Connection: " << connectionString << std::endl;
    std::cout << "  Table: " << tableName << std::endl;
    std::cout << "\n??  Make sure PostgreSQL is running!" << std::endl;
    std::cout << "   Docker: docker run -d -p 5432:5432 -e POSTGRES_PASSWORD=postgres postgres:15\n" << std::endl;
    
    try {
        // Step 1: Create client
        std::cout << "[1] Creating PostgreSQL client..." << std::endl;
        auto client = DatabaseClientFactory::createPostgreSQL(connectionString);
        std::cout << "? Client created\n" << std::endl;
        
        // Step 2: Test connection
        std::cout << "[2] Testing connection..." << std::endl;
        if (!client->testConnection()) {
            std::cerr << "? Failed to connect to PostgreSQL" << std::endl;
            return 1;
        }
        std::cout << "? Connected to PostgreSQL\n" << std::endl;
        
        // Get server info
        std::string serverInfo = client->getServerInfo();
        std::cout << "Server Info:\n" << serverInfo << "\n" << std::endl;
        
        // Step 3: Initialize table (create if not exists)
        std::cout << "[3] Initializing table: " << tableName << std::endl;
        
        // Drop existing table for clean start
        if (client->collectionExists(tableName)) {
            std::cout << "  Dropping existing table..." << std::endl;
            client->deleteCollection(tableName);
        }
        
        bool initialized = client->initializeCollection(tableName);
        if (!initialized) {
            std::cerr << "? Failed to initialize table" << std::endl;
            return 1;
        }
        std::cout << "? Table initialized\n" << std::endl;
        
        // Step 4: Insert single document
        std::cout << "[4] Inserting single event..." << std::endl;
        RawEvent event1 = createSampleEvent("evt_001", "chrome.exe");
        std::string docId1 = client->indexDocument(tableName, event1);
        std::cout << "? Inserted event: " << docId1 << "\n" << std::endl;
        
        // Step 5: Bulk insert
        std::cout << "[5] Bulk inserting events..." << std::endl;
        std::vector<RawEvent> events;
        events.push_back(createSampleEvent("evt_002", "vscode.exe"));
        events.push_back(createSampleEvent("evt_003", "chrome.exe"));
        events.push_back(createSampleEvent("evt_004", "slack.exe"));
        events.push_back(createSampleEvent("evt_005", "outlook.exe"));
        
        auto startBulk = std::chrono::high_resolution_clock::now();
        bool bulkSuccess = client->bulkIndexDocuments(tableName, events);
        auto endBulk = std::chrono::high_resolution_clock::now();
        auto durationBulk = std::chrono::duration_cast<std::chrono::milliseconds>(endBulk - startBulk);
        
        std::cout << "? Bulk inserted " << events.size() << " events in " 
                  << durationBulk.count() << " ms\n" << std::endl;
        
        // Step 6: Get statistics
        std::cout << "[6] Getting table statistics..." << std::endl;
        CollectionStats stats = client->getCollectionStats(tableName);
        std::cout << "  Total documents: " << stats.documentCount << std::endl;
        std::cout << "  Uncompressed: " << stats.uncompressedCount << std::endl;
        std::cout << "  Compressed: " << stats.compressedCount << std::endl;
        std::cout << "  Table size: " << stats.sizeInBytes << " bytes\n" << std::endl;
        
        // Step 7: Search for events
        std::cout << "[7] Searching for events..." << std::endl;
        
        // Simple match query
        std::string query1 = R"({
            "query": {
                "match": {
                    "app_name": "chrome"
                }
            }
        })";
        
        SearchResult result1 = client->search(tableName, query1, 0, 10);
        std::cout << "  Query: app_name contains 'chrome'" << std::endl;
        std::cout << "  Results: " << result1.totalHits << " hits" << std::endl;
        
        for (size_t i = 0; i < result1.events.size(); ++i) {
            printEvent(result1.events[i], i + 1);
        }
        std::cout << std::endl;
        
        // Boolean query
        std::string query2 = R"({
            "query": {
                "bool": {
                    "must": [
                        {"match": {"screen_content": "PostgreSQL"}}
                    ],
                    "filter": [
                        {"term": {"compressed": false}}
                    ]
                }
            }
        })";
        
        SearchResult result2 = client->search(tableName, query2, 0, 10);
        std::cout << "  Query: screen_content contains 'PostgreSQL' AND compressed=false" << std::endl;
        std::cout << "  Results: " << result2.totalHits << " hits\n" << std::endl;
        
        // Step 8: Get uncompressed events
        std::cout << "[8] Getting uncompressed events..." << std::endl;
        std::vector<RawEvent> uncompressed = client->getUncompressedEvents(tableName, 24);
        std::cout << "  Found " << uncompressed.size() << " uncompressed events\n" << std::endl;
        
        // Step 9: Mark events as compressed
        if (uncompressed.size() >= 2) {
            std::cout << "[9] Marking events as compressed..." << std::endl;
            std::vector<std::string> eventIds = {
                uncompressed[0].eventId,
                uncompressed[1].eventId
            };
            
            std::string sessionId = "session_001";
            bool marked = client->markEventsAsCompressed(tableName, eventIds, sessionId);
            
            if (marked) {
                std::cout << "? Marked " << eventIds.size() << " events as compressed" << std::endl;
                std::cout << "  Session ID: " << sessionId << "\n" << std::endl;
            }
        }
        
        // Step 10: Update a document
        std::cout << "[10] Updating a document..." << std::endl;
        std::string updateJson = R"({
            "doc": {
                "interaction_count": 15,
                "dwell_time_seconds": 60
            }
        })";
        
        bool updated = client->updateDocument(tableName, "evt_001", updateJson);
        std::cout << "? Document updated: evt_001\n" << std::endl;
        
        // Step 11: Get document by app name
        std::cout << "[11] Getting document by app name..." << std::endl;
        RawEvent slackEvent = client->getDocumentByAppName(tableName, "slack.exe");
        if (!slackEvent.eventId.empty()) {
            printEvent(slackEvent);
            std::cout << std::endl;
        }
        
        // Step 12: Final statistics
        std::cout << "[12] Final statistics..." << std::endl;
        CollectionStats finalStats = client->getCollectionStats(tableName);
        std::cout << "  Total documents: " << finalStats.documentCount << std::endl;
        std::cout << "  Uncompressed: " << finalStats.uncompressedCount << std::endl;
        std::cout << "  Compressed: " << finalStats.compressedCount << std::endl;
        std::cout << "  Table size: " << finalStats.sizeInBytes << " bytes\n" << std::endl;
        
        // Cleanup
        std::cout << "[CLEANUP] Dropping test table..." << std::endl;
        client->deleteCollection(tableName);
        std::cout << "? Table dropped\n" << std::endl;
        
        std::cout << "================================================" << std::endl;
        std::cout << "? Example completed successfully!" << std::endl;
        std::cout << "================================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Error: " << e.what() << std::endl;
        std::cerr << "\nMake sure PostgreSQL is running and accessible." << std::endl;
        std::cerr << "Connection string: " << connectionString << std::endl;
        return 1;
    }
}
