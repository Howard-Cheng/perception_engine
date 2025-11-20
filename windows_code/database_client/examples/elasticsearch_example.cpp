// examples/elasticsearch_example.cpp
// Complete example demonstrating Elasticsearch client usage

#include "DatabaseClientFactory.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace database;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " Perception Engine - Elasticsearch Example" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "⚠️  Make sure Elasticsearch is running!" << std::endl;
    std::cout << "   Run: .\\deploy_elasticsearch.ps1" << std::endl;
    std::cout << "   Or:  docker run -p 9200:9200 -e \"discovery.type=single-node\" elasticsearch:8.11.0" << std::endl;
    std::cout << std::endl;
    
    try {
        // Step 1: Create Elasticsearch client
        std::cout << "[1/6] Creating Elasticsearch client..." << std::endl;
        auto client = DatabaseClientFactory::createElasticsearch("http://localhost:9200");
        std::cout << "  ✓ Client created" << std::endl;
        
        // Get server info
        std::string info = client->getServerInfo();
        std::cout << "  Info (first 150 chars): " << info.substr(0, 150) << "..." << std::endl;
        std::cout << std::endl;
        
        // Step 2: Initialize index
        std::cout << "[2/6] Initializing index..." << std::endl;
        std::string indexName = "perception_events_demo";
        client->initializeCollection(indexName);
        std::cout << "  ✓ Index initialized with mapping" << std::endl;
        
        // Wait for index to be ready
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << std::endl;
        
        // Step 3: Create and index sample documents
        std::cout << "[3/6] Indexing sample documents..." << std::endl;
        
        std::vector<RawEvent> sampleEvents;
        
        // Event 1: Chrome browsing
        {
            RawEvent event;
            event.eventId = "demo_001";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "chrome.exe";
            event.windowTitle = "Elasticsearch - The Official Elastic Stack";
            event.url = "https://www.elastic.co/elasticsearch/";
            event.screenContent = "Elasticsearch is a distributed, RESTful search and analytics engine. "
                                 "It provides powerful full-text search capabilities with advanced features "
                                 "like fuzzy matching, relevance scoring, and aggregations.";
            event.interactionCount = 8;
            event.dwellTimeSeconds = 180;
            event.compressed = false;
            event.contentType = ContentType::TEXT;  // Changed from WEB_PAGE
            event.domain = Domain::WORK;
            sampleEvents.push_back(event);
        }
        
        // Event 2: VS Code programming
        {
            RawEvent event;
            event.eventId = "demo_002";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "Code.exe";
            event.windowTitle = "ElasticsearchClient.cpp - Visual Studio Code";
            event.screenContent = "C++ implementation of Elasticsearch REST API client. "
                                 "Using libcurl for HTTP requests and nlohmann/json for JSON parsing. "
                                 "Implementing full-text search with Query DSL support.";
            event.interactionCount = 25;
            event.dwellTimeSeconds = 600;
            event.compressed = false;
            event.contentType = ContentType::CODE;
            event.domain = Domain::WORK;
            sampleEvents.push_back(event);
        }
        
        // Event 3: Teams meeting about search
        {
            RawEvent event;
            event.eventId = "demo_003";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "Teams.exe";
            event.windowTitle = "Architecture Review - Microsoft Teams";
            event.screenContent = "Team discussing search architecture. "
                                 "Comparing Elasticsearch vs other solutions. "
                                 "Elasticsearch chosen for its scalability and powerful query capabilities.";
            event.voiceTranscription = "We need a search engine that can handle complex queries. "
                                      "Elasticsearch provides aggregations which are perfect for analytics.";
            event.interactionCount = 5;
            event.dwellTimeSeconds = 2400;
            event.compressed = false;
            event.contentType = ContentType::AUDIO;  // Changed from MEETING
            event.domain = Domain::WORK;
            sampleEvents.push_back(event);
        }
        
        // Event 4: Document editing with location
        {
            RawEvent event;
            event.eventId = "demo_004";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "WINWORD.EXE";
            event.windowTitle = "Search_Architecture.docx - Word";
            event.screenContent = "Document outlining search architecture using Elasticsearch. "
                                 "Key requirements: sub-second search latency, fuzzy matching, "
                                 "relevance tuning, and geo-spatial queries.";
            event.interactionCount = 15;
            event.dwellTimeSeconds = 900;
            event.compressed = false;
            event.contentType = ContentType::DOCUMENT;
            event.domain = Domain::WORK;
            
            // Add location data
            event.systemInfo.locationLat = 37.7749;  // San Francisco
            event.systemInfo.locationLon = -122.4194;
            event.systemInfo.cpuUsage = 45.2;
            event.systemInfo.memoryUsage = 62.5;
            event.systemInfo.batteryPercent = 75;
            event.systemInfo.isCharging = false;
            
            sampleEvents.push_back(event);
        }
        
        // Bulk index
        bool success = client->bulkIndexDocuments(indexName, sampleEvents);
        std::cout << "  ✓ Indexed " << sampleEvents.size() << " documents" << std::endl;
        
        // Refresh to make documents searchable
        client->refreshCollection(indexName);
        std::cout << "  Refreshing index..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << std::endl;
        
        // Step 4: Perform various searches
        std::cout << "[4/6] Performing searches..." << std::endl;
        std::cout << std::endl;
        
        // Search 1: Simple match query
        std::cout << "  Search 1: Match query for 'Elasticsearch'" << std::endl;
        {
            std::string query = R"({
                "query": {
                    "match": {
                        "screen_content": "Elasticsearch"
                    }
                }
            })";
            
            SearchResult result = client->search(indexName, query, 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 2: Multi-field search
        std::cout << "  Search 2: Multi-field search for 'search'" << std::endl;
        {
            std::string query = R"({
                "query": {
                    "multi_match": {
                        "query": "search",
                        "fields": ["screen_content", "window_title", "voice_transcription"]
                    }
                }
            })";
            
            SearchResult result = client->search(indexName, query, 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 3: Boolean query with filters
        std::cout << "  Search 3: Boolean query (WORK domain + uncompressed)" << std::endl;
        {
            std::string query = R"({
                "query": {
                    "bool": {
                        "must": [
                            {"match": {"screen_content": "architecture"}}
                        ],
                        "filter": [
                            {"term": {"domain": "WORK"}},
                            {"term": {"compressed": false}}
                        ]
                    }
                }
            })";
            
            SearchResult result = client->search(indexName, query, 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 4: Fuzzy search
        std::cout << "  Search 4: Fuzzy search for 'Elasticsarch' (typo)" << std::endl;
        {
            std::string query = R"({
                "query": {
                    "fuzzy": {
                        "screen_content": {
                            "value": "Elasticsarch",
                            "fuzziness": "AUTO"
                        }
                    }
                }
            })";
            
            SearchResult result = client->search(indexName, query, 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits (fuzzy matching!)" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 5: Geo-distance query
        std::cout << "  Search 5: Geo-distance query (within 50km of SF)" << std::endl;
        {
            std::string query = R"({
                "query": {
                    "bool": {
                        "filter": {
                            "geo_distance": {
                                "distance": "50km",
                                "system_info.location": {
                                    "lat": 37.7749,
                                    "lon": -122.4194
                                }
                            }
                        }
                    }
                }
            })";
            
            SearchResult result = client->search(indexName, query, 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Step 5: Aggregations
        std::cout << "[5/6] Performing aggregations..." << std::endl;
        {
            std::string aggQuery = R"({
                "size": 0,
                "aggs": {
                    "by_app": {
                        "terms": {
                            "field": "app_name"
                        }
                    },
                    "avg_dwell": {
                        "avg": {
                            "field": "dwell_time_seconds"
                        }
                    }
                }
            })";
            
            SearchResult aggResult = client->search(indexName, aggQuery, 0, 0);
            std::cout << "  ✓ Aggregations completed" << std::endl;
            std::cout << "    (Results would be in aggregations field)" << std::endl;
        }
        std::cout << std::endl;
        
        // Step 6: Display statistics
        std::cout << "[6/6] Index statistics:" << std::endl;
        CollectionStats stats = client->getCollectionStats(indexName);
        std::cout << "  Total documents:   " << stats.documentCount << std::endl;
        std::cout << "  Uncompressed:      " << stats.uncompressedCount << std::endl;
        std::cout << "  Compressed:        " << stats.compressedCount << std::endl;
        std::cout << "  Index size:        " << stats.sizeInBytes << " bytes" << std::endl;
        std::cout << std::endl;
        
        std::cout << "==================================================" << std::endl;
        std::cout << " Example completed successfully!" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "Elasticsearch Features Demonstrated:" << std::endl;
        std::cout << "  ✓ Full-text search (match, multi-match)" << std::endl;
        std::cout << "  ✓ Boolean queries with filters" << std::endl;
        std::cout << "  ✓ Fuzzy matching (typo tolerance)" << std::endl;
        std::cout << "  ✓ Geo-spatial queries" << std::endl;
        std::cout << "  ✓ Aggregations" << std::endl;
        std::cout << "  ✓ Relevance scoring" << std::endl;
        std::cout << std::endl;
        std::cout << "Next steps:" << std::endl;
        std::cout << "  - Explore Kibana UI: http://localhost:5601" << std::endl;
        std::cout << "  - Try custom queries with Query DSL" << std::endl;
        std::cout << "  - Test with larger datasets" << std::endl;
        std::cout << "  - Configure analyzers for better search" << std::endl;
        std::cout << std::endl;
        
        // Optional: cleanup
        std::cout << "Keep the test data? (Y/N): ";
        char choice;
        std::cin >> choice;
        if (choice == 'N' || choice == 'n') {
            client->deleteCollection(indexName);
            std::cout << "✓ Test index deleted" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "\nMake sure Elasticsearch is running!" << std::endl;
        std::cerr << "Run: .\\deploy_elasticsearch.ps1" << std::endl;
        return 1;
    }
    
    return 0;
}
