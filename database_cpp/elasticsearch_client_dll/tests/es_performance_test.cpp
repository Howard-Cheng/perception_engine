// tests/es_performance_test.cpp
// Performance benchmark for Elasticsearch Client

#include "ElasticsearchClient.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <thread>

using namespace elasticsearch;

// Helper: Create test events
std::vector<RawEvent> createTestEvents(int count, const std::string& prefix) {
    std::vector<RawEvent> events;
    events.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        RawEvent event;
        event.eventId = prefix + std::to_string(i);
        event.timestamp = std::time(nullptr);
        event.createdAt = std::time(nullptr);
        event.deviceId = "perf_test_device";
        event.appName = (i % 2 == 0) ? "chrome.exe" : "vscode.exe";
        event.windowTitle = "Performance Test Window " + std::to_string(i);
        event.screenContent = "Test content for performance testing. Event number " + std::to_string(i);
        event.interactionCount = i;
        event.dwellTimeSeconds = 10 + (i % 50);
        event.compressed = false;
        
        event.systemInfo.batteryPercent = 80 + (i % 20);
        event.systemInfo.isCharging = (i % 2 == 0);
        event.systemInfo.networkType = "WiFi";
        event.systemInfo.cpuUsage = 10.0 + (i % 50);
        event.systemInfo.memoryUsage = 40.0 + (i % 40);
        
        events.push_back(event);
    }
    
    return events;
}

// Benchmark: Single document indexing
void benchmarkSingleIndexing(ElasticsearchClient& client, const std::string& indexName) {
    std::cout << "\n[Benchmark] Single Document Indexing" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    
    const int count = 100;
    auto events = createTestEvents(count, "single_");
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& event : events) {
        client.indexDocument(indexName, event);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double avgTime = duration.count() / (double)count;
    double throughput = count / (duration.count() / 1000.0);
    
    std::cout << "  Documents: " << count << std::endl;
    std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms/doc" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " docs/sec" << std::endl;
}

// Benchmark: Bulk indexing
void benchmarkBulkIndexing(ElasticsearchClient& client, const std::string& indexName) {
    std::cout << "\n[Benchmark] Bulk Document Indexing" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    
    const int count = 1000;
    const int batchSize = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < count; i += batchSize) {
        auto events = createTestEvents(batchSize, "bulk_" + std::to_string(i) + "_");
        client.bulkIndexDocuments(indexName, events);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double avgTime = duration.count() / (double)count;
    double throughput = count / (duration.count() / 1000.0);
    
    std::cout << "  Documents: " << count << std::endl;
    std::cout << "  Batch size: " << batchSize << std::endl;
    std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms/doc" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " docs/sec" << std::endl;
}

// Benchmark: Search queries
void benchmarkSearch(ElasticsearchClient& client, const std::string& indexName) {
    std::cout << "\n[Benchmark] Search Queries" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    
    // Refresh index first
    client.refreshIndex(indexName);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    const int iterations = 50;
    
    // Match all query
    {
        std::string query = R"({"query":{"match_all":{}},"size":100})";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            client.search(indexName, query, 0, 100);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTime = duration.count() / (double)iterations;
        double qps = iterations / (duration.count() / 1000.0);
        
        std::cout << "\nMatch All Query:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
        std::cout << "  QPS: " << std::fixed << std::setprecision(2) << qps << std::endl;
    }
    
    // Term query
    {
        std::string query = R"({"query":{"term":{"app_name":"chrome.exe"}}})";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            client.search(indexName, query, 0, 100);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTime = duration.count() / (double)iterations;
        double qps = iterations / (duration.count() / 1000.0);
        
        std::cout << "\nTerm Query:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
        std::cout << "  QPS: " << std::fixed << std::setprecision(2) << qps << std::endl;
    }
    
    // Full-text search
    {
        std::string query = R"({"query":{"match":{"screen_content":"performance"}}})";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            client.search(indexName, query, 0, 100);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTime = duration.count() / (double)iterations;
        double qps = iterations / (duration.count() / 1000.0);
        
        std::cout << "\nFull-Text Search:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
        std::cout << "  QPS: " << std::fixed << std::setprecision(2) << qps << std::endl;
    }
}

// Benchmark: Aggregations and statistics
void benchmarkStatistics(ElasticsearchClient& client, const std::string& indexName) {
    std::cout << "\n[Benchmark] Statistics Operations" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    
    const int iterations = 100;
    
    // Document count
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            client.getDocumentCount(indexName);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTime = duration.count() / (double)iterations;
        
        std::cout << "\nDocument Count:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
    }
    
    // Index stats
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            client.getIndexStats(indexName);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        double avgTime = duration.count() / (double)iterations;
        
        std::cout << "\nIndex Stats:" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << avgTime << " ms" << std::endl;
    }
}

int main() {
    std::cout << "=============================================" << std::endl;
    std::cout << " Elasticsearch Client - Performance Test" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    std::string esUrl = "http://localhost:9200";
    std::string indexName = "perf_test_index";
    
    try {
        ElasticsearchClient client(esUrl);
        
        // Test connection
        std::cout << "\nTesting connection..." << std::endl;
        if (!client.testConnection()) {
            std::cerr << "? Cannot connect to Elasticsearch!" << std::endl;
            return 1;
        }
        std::cout << "? Connected" << std::endl;
        
        // Setup
        std::cout << "\nSetting up test index..." << std::endl;
        client.deleteIndex(indexName);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        client.initializeIndex(indexName);
        std::cout << "? Index ready" << std::endl;
        
        // Run benchmarks
        benchmarkSingleIndexing(client, indexName);
        benchmarkBulkIndexing(client, indexName);
        benchmarkSearch(client, indexName);
        benchmarkStatistics(client, indexName);
        
        // Final stats
        std::cout << "\n[Final Statistics]" << std::endl;
        std::cout << "-------------------------------------" << std::endl;
        IndexStats stats = client.getIndexStats(indexName);
        std::cout << "  Total documents: " << stats.documentCount << std::endl;
        std::cout << "  Index size: " << (stats.sizeInBytes / 1024.0 / 1024.0) << " MB" << std::endl;
        
        // Cleanup
        std::cout << "\nCleaning up..." << std::endl;
        client.deleteIndex(indexName);
        std::cout << "? Done" << std::endl;
        
        std::cout << "\n=============================================" << std::endl;
        std::cout << " Performance Test Complete" << std::endl;
        std::cout << "=============================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
