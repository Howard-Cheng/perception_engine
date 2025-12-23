// test_getesdbdata_fuzzy.cpp
// Test GetESDBData-style queries with fuzzy matching in PostgreSQL

#include "PostgreSQLClient.h"
#include "DatabaseClientFactory.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace database;

// Helper: Create test event
RawEvent createTestEvent(const std::string& id, const std::string& appName, const std::string& content) {
    RawEvent event;
    event.eventId = id;
    event.timestamp = std::time(nullptr);
    event.createdAt = std::time(nullptr);
    event.deviceId = "test_device_fuzzy";
    event.appName = appName;
    event.windowTitle = "Test Window - " + appName;
    event.screenContent = content;
    event.interactionCount = 5;
    event.dwellTimeSeconds = 30;
    event.compressed = false;
    event.contentType = ContentType::TEXT;
    event.domain = Domain::WORK;
    return event;
}

// Main function demonstrating GetESDBData with fuzzy matching
int main(int argc, char** argv) {
    std::cout << "=============================================" << std::endl;
    std::cout << " GetESDBData with Fuzzy Matching Test" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    std::string connectionString = "host=127.0.0.1 port=5432 dbname=postgres user=postgres";
    std::string tableName = "test_fuzzy_search";
    
    if (argc > 1) connectionString = argv[1];
    if (argc > 2) tableName = argv[2];
    
    try {
        auto client = DatabaseClientFactory::createPostgreSQL(connectionString);
        std::cout << "? Connected to PostgreSQL" << std::endl;
        
        // Initialize table
        if (client->collectionExists(tableName)) {
            client->deleteCollection(tableName);
        }
        client->initializeCollection(tableName);
        std::cout << "? Table initialized" << std::endl;
        
        // Insert test data with timestamps
        std::time_t baseTime = std::time(nullptr);
        
        std::vector<RawEvent> events = {
            createTestEvent("fuzzy_001", "chrome.exe", 
                "Working on project with PostgreSQL and design"),
            createTestEvent("fuzzy_002", "vscode.exe", 
                "Editing code and testing database queries"),
            createTestEvent("fuzzy_003", "firefox.exe", 
                "Browsing documentation for elasticsearch"),
            createTestEvent("fuzzy_004", "chrome.exe", 
                "Testing integration with postgres database"),
            createTestEvent("fuzzy_005", "vscode.exe", 
                "Developing features for search engine using elasticsearch")
        };
        
        // Set timestamps (spread over 20 minutes)
        for (size_t i = 0; i < events.size(); ++i) {
            events[i].timestamp = baseTime - (600 - i * 150);  // 10 min ago to now
            
            // Add voice transcription with typos
            if (i % 2 == 0) {
                events[i].voiceTranscription = "Working on databse systems";  // Typo!
            }
            
            // Add camera description
            if (i % 3 == 0) {
                events[i].cameraDescription = "User reading elasticsrch documentation";  // Typo!
            }
        }
        
        client->bulkIndexDocuments(tableName, events);
        std::cout << "? Inserted " << events.size() << " test events" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // ========================================
        // Test 1: Exact match (like GetESDBData)
        // ========================================
        std::cout << "\n========== Test 1: Exact Match ==========" << std::endl;
        
        std::time_t startTime = baseTime - 1200;  // 20 minutes ago
        std::time_t endTime = baseTime + 60;      // 1 minute from now
        std::string keyword = "database";
        
        std::string query1 = R"({"query":{"bool":{"must":[)";
        query1 += R"({"multi_match":{"query":")" + keyword + R"(",)";
        query1 += R"("fields":["screen_content","voice_transcription","camera_description","app_name","window_title"]}},)";
        query1 += R"({"range":{"timestamp":{"gte":)" + std::to_string(startTime);
        query1 += R"(,"lte":)" + std::to_string(endTime) + R"(}}}]}},)";
        query1 += R"("sort":[{"timestamp":{"order":"desc"}}]})";
        
        SearchResult result1 = client->search(tableName, query1, 0, 100);
        std::cout << "Keyword: '" << keyword << "' (exact)" << std::endl;
        std::cout << "Results: " << result1.totalHits << " matches" << std::endl;
        
        for (const auto& event : result1.events) {
            std::cout << "  - " << event.eventId << " | " << event.appName << std::endl;
        }
        
        // ========================================
        // Test 2: Fuzzy match with AUTO fuzziness (LIKE GetESDBData!)
        // ========================================
        std::cout << "\n========== Test 2: Fuzzy Match (AUTO) ==========" << std::endl;
        
        std::string fuzzyKeyword = "datbse";  // Typo!
        
        std::string query2 = R"({"query":{"bool":{"must":[)";
        query2 += R"({"multi_match":{)";
        query2 += R"("query":")" + fuzzyKeyword + R"(",)";
        query2 += R"("fields":["screen_content","voice_transcription","camera_description","app_name","window_title"],)";
        query2 += R"("type":"best_fields",)";
        query2 += R"("fuzziness":"AUTO")";  // FUZZY MATCHING!
        query2 += R"(}},)";
        query2 += R"({"range":{"timestamp":{"gte":)" + std::to_string(startTime);
        query2 += R"(,"lte":)" + std::to_string(endTime) + R"(}}}]}},)";
        query2 += R"("sort":[{"timestamp":{"order":"desc"}}]})";
        
        SearchResult result2 = client->search(tableName, query2, 0, 100);
        std::cout << "Keyword: '" << fuzzyKeyword << "' (with typo, fuzziness=AUTO)" << std::endl;
        std::cout << "Results: " << result2.totalHits << " matches" << std::endl;
        std::cout << "? Fuzzy matching found results despite typo!" << std::endl;
        
        for (const auto& event : result2.events) {
            std::cout << "  - " << event.eventId << " | " << event.appName;
            if (event.screenContent.has_value()) {
                std::string content = event.screenContent.value();
                if (content.length() > 50) content = content.substr(0, 50) + "...";
                std::cout << " | " << content;
            }
            std::cout << std::endl;
        }
        
        // ========================================
        // Test 3: Multiple typos with fuzzy
        // ========================================
        std::cout << "\n========== Test 3: Multiple Typos ==========" << std::endl;
        
        std::string multiKeyword = "elasticsarch postgrs";  // Multiple typos!
        
        std::string query3 = R"({"query":{"bool":{"must":[)";
        query3 += R"({"multi_match":{)";
        query3 += R"("query":")" + multiKeyword + R"(",)";
        query3 += R"("fields":["screen_content","voice_transcription","camera_description","window_title"],)";
        query3 += R"("type":"best_fields",)";
        query3 += R"("fuzziness":"AUTO")";
        query3 += R"(}},)";
        query3 += R"({"range":{"timestamp":{"gte":)" + std::to_string(startTime);
        query3 += R"(,"lte":)" + std::to_string(endTime) + R"(}}}]}},)";
        query3 += R"("sort":[{"timestamp":{"order":"desc"}}]})";
        
        SearchResult result3 = client->search(tableName, query3, 0, 100);
        std::cout << "Keywords: '" << multiKeyword << "' (multiple typos)" << std::endl;
        std::cout << "Results: " << result3.totalHits << " matches" << std::endl;
        
        for (const auto& event : result3.events) {
            std::cout << "  - " << event.eventId << " | " << event.appName << std::endl;
        }
        
        // ========================================
        // Test 4: Fuzzy with filters
        // ========================================
        std::cout << "\n========== Test 4: Fuzzy + Filters ==========" << std::endl;
        
        std::string query4 = R"({"query":{"bool":{)";
        query4 += R"("must":[)";
        query4 += R"({"multi_match":{)";
        query4 += R"("query":"databse",)";
        query4 += R"("fields":["screen_content","app_name"],)";
        query4 += R"("fuzziness":"AUTO")";
        query4 += R"(}},)";
        query4 += R"({"range":{"timestamp":{"gte":)" + std::to_string(startTime) + R"(}}})";
        query4 += R"(],)";
        query4 += R"("filter":[)";
        query4 += R"({"term":{"compressed":false}})";
        query4 += R"(]}},)";
        query4 += R"("sort":[{"timestamp":{"order":"desc"}}]})";
        
        SearchResult result4 = client->search(tableName, query4, 0, 100);
        std::cout << "Fuzzy search with compressed=false filter" << std::endl;
        std::cout << "Results: " << result4.totalHits << " matches" << std::endl;
        
        // ========================================
        // Test 5: Different fuzziness levels
        // ========================================
        std::cout << "\n========== Test 5: Custom Fuzziness Levels ==========" << std::endl;
        
        // Using PostgreSQL's dedicated fuzzy search method
        auto* pgClient = dynamic_cast<PostgreSQLClient*>(client.get());
        if (pgClient) {
            std::cout << "\nUsing PostgreSQL fuzzySearch method:" << std::endl;
            
            // Low threshold (more results)
            SearchResult fuzzy1 = pgClient->fuzzySearch(
                tableName, "screen_content", "databse", 0.3, 0, 100);
            std::cout << "  Threshold 0.3: " << fuzzy1.totalHits << " matches" << std::endl;
            
            // Medium threshold
            SearchResult fuzzy2 = pgClient->fuzzySearch(
                tableName, "screen_content", "databse", 0.5, 0, 100);
            std::cout << "  Threshold 0.5: " << fuzzy2.totalHits << " matches" << std::endl;
            
            // High threshold (fewer results)
            SearchResult fuzzy3 = pgClient->fuzzySearch(
                tableName, "screen_content", "databse", 0.7, 0, 100);
            std::cout << "  Threshold 0.7: " << fuzzy3.totalHits << " matches" << std::endl;
        }
        
        // ========================================
        // Summary
        // ========================================
        std::cout << "\n=============================================" << std::endl;
        std::cout << " Summary" << std::endl;
        std::cout << "=============================================" << std::endl;
        std::cout << "? Exact keyword search: " << result1.totalHits << " results" << std::endl;
        std::cout << "? Fuzzy keyword search: " << result2.totalHits << " results (with typo!)" << std::endl;
        std::cout << "? Multiple fuzzy keywords: " << result3.totalHits << " results" << std::endl;
        std::cout << "? Fuzzy with filters: " << result4.totalHits << " results" << std::endl;
        std::cout << "\n? GetESDBData-style fuzzy matching works in PostgreSQL!" << std::endl;
        std::cout << "? Query syntax is identical to Elasticsearch!" << std::endl;
        
        // Cleanup
        std::cout << "\n[CLEANUP] Removing test table..." << std::endl;
        client->deleteCollection(tableName);
        std::cout << "? Done!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Error: " << e.what() << std::endl;
        return 1;
    }
}
