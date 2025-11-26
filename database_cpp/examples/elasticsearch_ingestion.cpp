#include "layer0/DataIngestion.h"
#ifdef ELASTICSEARCH_ENABLED
#include "layer0/ElasticsearchClient.h"
#endif
#include "common/Logger.h"
#include "common/Utils.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace perception;
using json = nlohmann::json;

int main() {
    // Initialize logger with DEBUG level to see detailed logs
    Logger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    std::cout << "=============================================" << std::endl;
    std::cout << " Perception Engine - Elasticsearch Example" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << std::endl;
    
#ifdef ELASTICSEARCH_ENABLED
    try {
        // Elasticsearch configuration
        std::string esUrl = "http://localhost:9200";
        std::string indexName = "perception_raw_events";
        std::string deviceId = "example_device_es_001";
        
        LOG_INFO("Connecting to Elasticsearch at: " + esUrl);
        
        // Create Elasticsearch client
        layer0::ElasticsearchClient esClient(esUrl);
        
        // Initialize index
        LOG_INFO("Initializing Elasticsearch index: " + indexName);
        if (!esClient.initializeIndex(indexName)) {
            LOG_ERROR("Failed to initialize Elasticsearch index!");
            return 1;
        }
        
        std::cout << "? Connected to Elasticsearch" << std::endl;
        std::cout << "? Index initialized: " + indexName << std::endl;
        std::cout << std::endl;
        
        // Example 1: Index a single event
        LOG_INFO("Example 1: Indexing single event to Elasticsearch");
        
        layer0::RawEvent event1;
        event1.eventId = utils::generateUUID();  // Generate unique ID
        event1.timestamp = utils::now();
        event1.deviceId = deviceId;
        event1.appName = "chrome.exe";
        event1.windowTitle = "GitHub - perception_engine - Elasticsearch";
        event1.url = "https://github.com/Howard-Cheng/perception_engine";
        event1.screenContent = "# Elasticsearch Integration\n\nNow using Elasticsearch for Layer 0 storage...";
        event1.interactionCount = 5;
        event1.dwellTimeSeconds = 45;
        event1.compressed = false;
        event1.createdAt = utils::now();
        
        // Add mouse event
        MouseEvent mouseEvent1;
        mouseEvent1.timestamp = utils::now();
        mouseEvent1.eventType = "Copy";
        mouseEvent1.content = "Elasticsearch is awesome";
        mouseEvent1.posX = 100;
        mouseEvent1.posY = 200;
        mouseEvent1.elementType = "Text";
        event1.mouseEvents.push_back(mouseEvent1);
        
        // System info
        event1.systemInfo.batteryPercent = 85;
        event1.systemInfo.isCharging = false;
        event1.systemInfo.networkType = "WiFi";
        event1.systemInfo.cpuUsage = 15.3;
        event1.systemInfo.memoryUsage = 45.2;
        
        std::string docId1 = esClient.indexDocument(indexName, event1);
        if (!docId1.empty()) {
            std::cout << "? Indexed event to Elasticsearch: " << docId1 << std::endl;
        } else {
            std::cout << "? Failed to index event!" << std::endl;
            LOG_ERROR("indexDocument returned empty ID");
        }
        
        // Debug: Verify the event was indexed
        std::cout << "  Debug: Checking document count after first index..." << std::endl;
        int countAfterFirst = esClient.getDocumentCount(indexName);
        std::cout << "  Documents in index: " << countAfterFirst << std::endl;
        std::cout << std::endl;
        
        // Example 2: Bulk index multiple events
        LOG_INFO("Example 2: Bulk indexing multiple events");
        
        std::vector<layer0::RawEvent> events;
        for (int i = 0; i < 10; ++i) {
            layer0::RawEvent event;
            event.eventId = utils::generateUUID();
            event.timestamp = utils::now();
            event.deviceId = deviceId;
            
            // Add variety - some with Elasticsearch, some without
            if (i % 3 == 0) {
                event.appName = "chrome.exe";
                event.windowTitle = "Elasticsearch Documentation - Chrome";
                event.screenContent = "Learning about Elasticsearch features and API. "
                                     "Elasticsearch is a distributed search engine.";
                event.interactionCount = 5 + i;
            } else {
                event.appName = "code.exe";
                event.windowTitle = "main.cpp - Visual Studio Code";
                event.screenContent = "int main() { /* Test code */ }";
                event.interactionCount = 3;
            }
            
            event.dwellTimeSeconds = 30;
            event.compressed = false;
            event.createdAt = utils::now();
            
            event.systemInfo.batteryPercent = 80 - i;
            event.systemInfo.isCharging = false;
            event.systemInfo.cpuUsage = 20.0 + i;
            event.systemInfo.memoryUsage = 50.0 + i * 0.5;
            
            events.push_back(event);
            
            // Small delay to simulate different timestamps
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        bool bulkSuccess = esClient.bulkIndexDocuments(indexName, events);
        if (bulkSuccess) {
            std::cout << "? Bulk indexed " << events.size() << " events to Elasticsearch" << std::endl;
        } else {
            std::cout << "? Bulk indexing failed" << std::endl;
        }
        std::cout << std::endl;
        
        // Refresh index to make documents immediately searchable
        LOG_INFO("Refreshing index to make documents searchable...");
        esClient.refreshIndex(indexName);
        
        // Debug: Check if documents are actually indexed
        LOG_INFO("Debug: Checking indexed documents...");
        json allDocsQuery = json::parse(R"({
            "query": {
                "match_all": {}
            },
            "size": 20
        })");
        auto allDocs = esClient.search(indexName, allDocsQuery.dump(), 0, 20);
        std::cout << "  Total documents in index: " << allDocs.size() << std::endl;
        if (!allDocs.empty()) {
            std::cout << "  First document app: " << allDocs[0].appName << std::endl;
            std::cout << "  First document content: " 
                      << allDocs[0].screenContent.value_or("N/A").substr(0, 100) << std::endl;
        }
        std::cout << std::endl;
        
        // Example 3: Query statistics from Elasticsearch
        LOG_INFO("Example 3: Elasticsearch statistics");
        
        int totalDocs = esClient.getDocumentCount(indexName);
        int uncompressedDocs = esClient.getUncompressedCount();
        
        std::cout << "Elasticsearch Statistics:" << std::endl;
        std::cout << "  Total documents: " << totalDocs << std::endl;
        std::cout << "  Uncompressed: " << uncompressedDocs << std::endl;
        std::cout << std::endl;
        
        // Example 4: Query uncompressed events
        LOG_INFO("Example 4: Querying uncompressed events");
        
        auto uncompressedEvents = esClient.getUncompressedEvents(24);  // Last 24 hours
        
        std::cout << "Retrieved uncompressed events: " << uncompressedEvents.size() << std::endl;
        if (!uncompressedEvents.empty()) {
            std::cout << "Sample event:" << std::endl;
            const auto& sampleEvent = uncompressedEvents[0];
            std::cout << "  Event ID: " << sampleEvent.eventId << std::endl;
            std::cout << "  App: " << sampleEvent.appName << std::endl;
            std::cout << "  Device: " << sampleEvent.deviceId << std::endl;
            std::cout << "  Interactions: " << sampleEvent.interactionCount << std::endl;
        }
        std::cout << std::endl;
        
        // Example 5: Mark events as compressed (simulation)
        LOG_INFO("Example 5: Marking events as compressed");
        
        if (uncompressedEvents.size() >= 2) {
            std::vector<std::string> eventIds = {
                uncompressedEvents[0].eventId,
                uncompressedEvents[1].eventId
            };
            
            std::string sessionId = "test_session_001";
            bool markSuccess = esClient.markEventsAsCompressed(eventIds, sessionId);
            
            if (markSuccess) {
                std::cout << "? Marked " << eventIds.size() << " events as compressed" << std::endl;
                std::cout << "  Session ID: " << sessionId << std::endl;
            } else {
                std::cout << "? Failed to mark events as compressed" << std::endl;
            }
        } else {
            std::cout << "  Not enough events to demonstrate marking as compressed" << std::endl;
        }
        std::cout << std::endl;
        
        // Example 6: Search functionality (demonstration)
        LOG_INFO("Example 6: Full-text search");
        
        json searchQuery = json::parse(R"({
            "query": {
                "match": {
                    "screen_content": "Elasticsearch"
                }
            },
            "size": 5
        })");
        
        std::cout << "Searching for events containing 'Elasticsearch'..." << std::endl;
        std::cout << "  Search query: " << searchQuery.dump(2) << std::endl;
        
        // Execute the search
        auto searchResults = esClient.search(indexName, searchQuery.dump(), 0, 5);
        
        std::cout << "  Found " << searchResults.size() << " matching documents" << std::endl;
        
        if (!searchResults.empty()) {
            std::cout << "\n  Sample search result:" << std::endl;
            const auto& result = searchResults[0];
            std::cout << "    Event ID: " << result.eventId << std::endl;
            std::cout << "    App: " << result.appName << std::endl;
            std::cout << "    Window: " << result.windowTitle.value_or("N/A") << std::endl;
            std::cout << "    Content preview: " 
                      << result.screenContent.value_or("N/A").substr(0, 50) 
                      << "..." << std::endl;
        }
        std::cout << std::endl;
        
        // Example 7: More advanced searches
        LOG_INFO("Example 7: Advanced search examples");
        
        // Search by app name
        json appSearchQuery = json::parse(R"({
            "query": {
                "term": {
                    "app_name": "chrome.exe"
                }
            },
            "size": 10
        })");
        
        auto chromeEvents = esClient.search(indexName, appSearchQuery.dump(), 0, 10);
        std::cout << "Events from chrome.exe: " << chromeEvents.size() << std::endl;
        
        // Range query - events with high interaction
        json interactionQuery = json::parse(R"({
            "query": {
                "range": {
                    "interaction_count": {
                        "gte": 3
                    }
                }
            },
            "sort": [
                {
                    "interaction_count": {
                        "order": "desc"
                    }
                }
            ],
            "size": 10
        })");
        
        auto highInteractionEvents = esClient.search(indexName, interactionQuery.dump(), 0, 10);
        std::cout << "High interaction events (¡Ý3): " << highInteractionEvents.size() << std::endl;
        std::cout << std::endl;
        
        // Final statistics
        totalDocs = esClient.getDocumentCount(indexName);
        uncompressedDocs = esClient.getUncompressedCount();
        int compressedDocs = totalDocs - uncompressedDocs;
        
        std::cout << "=============================================" << std::endl;
        std::cout << "Final Elasticsearch Statistics:" << std::endl;
        std::cout << "  Total documents: " << totalDocs << std::endl;
        std::cout << "  Uncompressed: " << uncompressedDocs << std::endl;
        std::cout << "  Compressed: " << compressedDocs << std::endl;
        std::cout << "  Index: " << indexName << std::endl;
        std::cout << "  Elasticsearch URL: " << esUrl << std::endl;
        std::cout << "=============================================" << std::endl;
        
        std::cout << std::endl;
        std::cout << "? Elasticsearch integration test completed successfully!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Comparison: Elasticsearch vs SQLite" << std::endl;
        std::cout << "  Elasticsearch advantages:" << std::endl;
        std::cout << "    - Full-text search capabilities" << std::endl;
        std::cout << "    - Distributed and scalable" << std::endl;
        std::cout << "    - Real-time analytics" << std::endl;
        std::cout << "    - Built-in clustering" << std::endl;
        std::cout << "  SQLite advantages:" << std::endl;
        std::cout << "    - No external dependencies" << std::endl;
        std::cout << "    - Zero configuration" << std::endl;
        std::cout << "    - Embedded database" << std::endl;
        std::cout << "    - Lower resource usage" << std::endl;
        std::cout << std::endl;
        
        LOG_INFO("Example completed successfully!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Elasticsearch example failed: " + std::string(e.what()));
        std::cerr << "\nTroubleshooting:" << std::endl;
        std::cerr << "  1. Make sure Elasticsearch is running on port 9200" << std::endl;
        std::cerr << "  2. Check connection: curl http://localhost:9200" << std::endl;
        std::cerr << "  3. Start Elasticsearch:" << std::endl;
        std::cerr << "     docker run -d -p 9200:9200 -p 9300:9300 \\" << std::endl;
        std::cerr << "       -e \"discovery.type=single-node\" \\" << std::endl;
        std::cerr << "       -e \"xpack.security.enabled=false\" \\" << std::endl;
        std::cerr << "       docker.elastic.co/elasticsearch/elasticsearch:8.11.0" << std::endl;
        return 1;
    }
#else
    std::cout << "? Elasticsearch support not enabled!" << std::endl;
    std::cout << std::endl;
    std::cout << "To enable Elasticsearch:" << std::endl;
    std::cout << "  1. Rebuild with -DENABLE_ELASTICSEARCH=ON" << std::endl;
    std::cout << "  2. Make sure CURL is installed (vcpkg install curl)" << std::endl;
    std::cout << "  3. Re-run CMake configuration" << std::endl;
    std::cout << std::endl;
    return 1;
#endif
    
    return 0;
}
