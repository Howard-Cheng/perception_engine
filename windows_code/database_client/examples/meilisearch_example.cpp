// examples/meilisearch_example.cpp
// Simple example demonstrating MeiliSearch client usage

#include "DatabaseClientFactory.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace database;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << " Perception Engine - MeiliSearch Example" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "??  Make sure MeiliSearch is running!" << std::endl;
    std::cout << "   Run: .\\deploy_meilisearch.ps1" << std::endl;
    std::cout << "   Or:  meilisearch.exe --master-key=perception_engine_key_2025" << std::endl;
    std::cout << std::endl;
    
    try {
        // Step 1: Create MeiliSearch client
        std::cout << "[1/5] Creating MeiliSearch client..." << std::endl;
        auto client = DatabaseClientFactory::createMeiliSearch(
            "http://localhost:7700",
            "perception_engine_key_2025"
        );
        std::cout << "  ? Client created" << std::endl;
        std::cout << "  Info: " << client->getServerInfo() << std::endl;
        std::cout << std::endl;
        
        // Step 2: Initialize collection
        std::cout << "[2/5] Initializing collection..." << std::endl;
        std::string collectionName = "perception_events";
        client->initializeCollection(collectionName);
        std::cout << "  ? Collection initialized" << std::endl;
        
        // Wait for index creation
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << std::endl;
        
        // Step 3: Create and index sample documents
        std::cout << "[3/5] Indexing sample documents..." << std::endl;
        
        std::vector<RawEvent> sampleEvents;
        
        // Event 1: Chrome browsing
        {
            RawEvent event;
            event.eventId = "example_001";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "chrome.exe";
            event.windowTitle = "Google Search - Chrome";
            event.url = "https://www.google.com/search?q=MeiliSearch";
            event.screenContent = "Search results for MeiliSearch. "
                                 "MeiliSearch is a lightning-fast search engine. "
                                 "It offers typo tolerance and instant search capabilities.";
            event.interactionCount = 5;
            event.dwellTimeSeconds = 120;
            event.compressed = false;
            sampleEvents.push_back(event);
        }
        
        // Event 2: VS Code programming
        {
            RawEvent event;
            event.eventId = "example_002";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "Code.exe";
            event.windowTitle = "main.cpp - Visual Studio Code";
            event.screenContent = "C++ code for perception engine implementation. "
                                 "Integrating MeiliSearch for full-text search. "
                                 "Database client factory pattern for flexibility.";
            event.interactionCount = 15;
            event.dwellTimeSeconds = 300;
            event.compressed = false;
            sampleEvents.push_back(event);
        }
        
        // Event 3: Teams meeting
        {
            RawEvent event;
            event.eventId = "example_003";
            event.timestamp = std::time(nullptr);
            event.createdAt = std::time(nullptr);
            event.deviceId = "laptop_001";
            event.appName = "Teams.exe";
            event.windowTitle = "Project Discussion - Microsoft Teams";
            event.screenContent = "Team meeting discussing search engine options. "
                                 "Comparing Elasticsearch, SQLite FTS5, and MeiliSearch. "
                                 "MeiliSearch chosen for its speed and ease of use.";
            event.voiceTranscription = "Let's evaluate MeiliSearch for our search needs. "
                                      "It has built-in typo tolerance which is great for users.";
            event.interactionCount = 3;
            event.dwellTimeSeconds = 1800;
            event.compressed = false;
            sampleEvents.push_back(event);
        }
        
        // Bulk index
        bool success = client->bulkIndexDocuments(collectionName, sampleEvents);
        std::cout << "  ? Indexed " << sampleEvents.size() << " documents" << std::endl;
        
        // Wait for indexing (MeiliSearch is asynchronous)
        std::cout << "  Waiting for indexing to complete..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << std::endl;
        
        // Step 4: Perform full-text searches
        std::cout << "[4/5] Performing full-text searches..." << std::endl;
        std::cout << std::endl;
        
        // Search 1: Simple keyword
        std::cout << "  Search 1: 'MeiliSearch'" << std::endl;
        {
            SearchResult result = client->search(collectionName, "MeiliSearch", 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 2: Typo tolerance (MeiliSearch feature)
        std::cout << "  Search 2: 'MeiliSerch' (typo - missing 'a')" << std::endl;
        {
            SearchResult result = client->search(collectionName, "MeiliSerch", 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits (typo-tolerant!)" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 3: Multi-word search
        std::cout << "  Search 3: 'search engine'" << std::endl;
        {
            SearchResult result = client->search(collectionName, "search engine", 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Search 4: Content-specific
        std::cout << "  Search 4: 'C++ code'" << std::endl;
        {
            SearchResult result = client->search(collectionName, "C++ code", 0, 10);
            std::cout << "    Results: " << result.totalHits << " hits" << std::endl;
            for (const auto& event : result.events) {
                std::cout << "    - " << event.appName << ": " << event.windowTitle.value_or("(no title)") << std::endl;
            }
        }
        std::cout << std::endl;
        
        // Step 5: Display statistics
        std::cout << "[5/5] Collection statistics:" << std::endl;
        CollectionStats stats = client->getCollectionStats(collectionName);
        std::cout << "  Total documents:   " << stats.documentCount << std::endl;
        std::cout << "  Database size:     " << stats.sizeInBytes << " bytes" << std::endl;
        std::cout << std::endl;
        
        std::cout << "==================================================" << std::endl;
        std::cout << " Example completed successfully!" << std::endl;
        std::cout << "==================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "MeiliSearch Features Demonstrated:" << std::endl;
        std::cout << "  ? Simple keyword search" << std::endl;
        std::cout << "  ? Typo tolerance (automatic!)" << std::endl;
        std::cout << "  ? Multi-word search" << std::endl;
        std::cout << "  ? Fast results (< 50ms typical)" << std::endl;
        std::cout << std::endl;
        std::cout << "Next steps:" << std::endl;
        std::cout << "  - Open MeiliSearch dashboard: http://localhost:7700" << std::endl;
        std::cout << "  - Try custom searches in the web UI" << std::endl;
        std::cout << "  - See MEILISEARCH_DEPLOYMENT.md for more examples" << std::endl;
        std::cout << std::endl;
        
        // Optional: cleanup
        std::cout << "Keep the test data? (Y/N): ";
        char choice;
        std::cin >> choice;
        if (choice == 'N' || choice == 'n') {
            client->deleteCollection(collectionName);
            std::cout << "? Test collection deleted" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "\nMake sure MeiliSearch is running!" << std::endl;
        std::cerr << "Run: .\\deploy_meilisearch.ps1" << std::endl;
        return 1;
    }
    
    return 0;
}
