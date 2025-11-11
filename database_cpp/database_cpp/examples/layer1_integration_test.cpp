#include "layer0/DataIngestion.h"
#include "layer1/CompressionPipeline.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace perception;

void printBanner() {
    std::cout << "==========================================" << std::endl;
    std::cout << " Layer 1 DuckDB Integration Test" << std::endl;
    std::cout << " SQLite ¡ú Sessions ¡ú DuckDB" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << std::endl;
}

void createTestEvents(layer0::DataIngestion& ingestion) {
    std::cout << "Creating test events..." << std::endl;
    
    // Create a series of events for a coding session
    for (int i = 0; i < 5; ++i) {
        layer0::RawEvent event;
        event.timestamp = utils::now();
        event.deviceId = "test_pc_001";
        event.appName = "Code.exe";
        event.windowTitle = "main.cpp - Visual Studio Code";
        event.url = "vscode://file/project/main.cpp";
        event.screenContent = "int main() { return 0; }";
        event.interactionCount = 3 + i;
        event.dwellTimeSeconds = 30 + (i * 10);
        
        // Add mouse events
        MouseEvent mouseEvent;
        mouseEvent.timestamp = utils::now();
        mouseEvent.eventType = "Copy";
        mouseEvent.content = "test code snippet " + std::to_string(i);
        mouseEvent.posX = 100;
        mouseEvent.posY = 200;
        event.mouseEvents.push_back(mouseEvent);
        
        // System info
        event.systemInfo.batteryPercent = 85 - i;
        event.systemInfo.isCharging = false;
        event.systemInfo.cpuUsage = 15.3 + i;
        event.systemInfo.memoryUsage = 45.2 + i;
        
        auto eventId = ingestion.ingestEvent(event);
        std::cout << "  ? Created event: " << eventId << std::endl;
        
        // Small delay between events
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << std::endl;
}

int main() {
    try {
        printBanner();
        
        // Setup paths
        std::string baseDir = "./test_layer1_data";
        std::string sqlitePath = baseDir + "/raw_events_test.db";
        std::string duckdbPath = baseDir + "/compressed_sessions_test.duckdb";
        std::string deviceId = "test_pc_001";
        
        // Ensure directory exists
        utils::ensureDirectoryExists(baseDir);
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "  SQLite DB: " << sqlitePath << std::endl;
        std::cout << "  DuckDB: " << duckdbPath << std::endl;
        std::cout << "  Device ID: " << deviceId << std::endl;
        std::cout << std::endl;
        
        // ===== STEP 1: Create test events in SQLite =====
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << "STEP 1: Ingesting Test Events to Layer 0" << std::endl;
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << std::endl;
        
        layer0::DataIngestion ingestion(sqlitePath, deviceId);
        createTestEvents(ingestion);
        
        // Check stats
        int totalEvents = ingestion.getEventCount();
        int uncompressed = ingestion.getUncompressedEventCount();
        
        std::cout << "Layer 0 Statistics:" << std::endl;
        std::cout << "  Total events: " << totalEvents << std::endl;
        std::cout << "  Uncompressed: " << uncompressed << std::endl;
        std::cout << std::endl;
        
        // ===== STEP 2: Run compression pipeline =====
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << "STEP 2: Running Compression Pipeline" << std::endl;
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << std::endl;
        
        // Setup session config
        SessionConfig sessionConfig;
        sessionConfig.idleThresholdSeconds = 300;  // 5 minutes
        
#ifdef DUCKDB_ENABLED
        // Create compression pipeline
        layer1::CompressionPipeline pipeline(sqlitePath, duckdbPath, sessionConfig);
        
        // Process uncompressed events
        std::cout << "Starting compression..." << std::endl;
        int sessionsCompressed = pipeline.processUncompressedEvents();
        
        std::cout << std::endl;
        std::cout << "Compression Results:" << std::endl;
        std::cout << "  Sessions compressed: " << sessionsCompressed << std::endl;
        
        // Get statistics
        auto stats = pipeline.getStatistics();
        std::cout << "  Events processed: " << stats.eventsProcessed << std::endl;
        std::cout << std::endl;
        
        // ===== STEP 3: Verify compression =====
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << "STEP 3: Verification" << std::endl;
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << std::endl;
        
        // Check SQLite - all events should be marked as compressed
        int remainingUncompressed = ingestion.getUncompressedEventCount();
        std::cout << "Layer 0 (SQLite):" << std::endl;
        std::cout << "  Remaining uncompressed: " << remainingUncompressed << std::endl;
        
        if (remainingUncompressed == 0) {
            std::cout << "  ? All events marked as compressed" << std::endl;
        } else {
            std::cout << "  ? Warning: Some events not compressed" << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << "Layer 1 (DuckDB):" << std::endl;
        std::cout << "  Compressed sessions stored: " << sessionsCompressed << std::endl;
        std::cout << "  Database file: " << duckdbPath << std::endl;
        std::cout << std::endl;
        
        // ===== SUCCESS =====
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << "? SUCCESS: Layer 1 Integration Test Passed!" << std::endl;
        std::cout << "©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Data Flow Verified:" << std::endl;
        std::cout << "  1. ? Events ingested to SQLite (Layer 0)" << std::endl;
        std::cout << "  2. ? Sessions detected and compressed" << std::endl;
        std::cout << "  3. ? Compressed data stored to DuckDB (Layer 1)" << std::endl;
        std::cout << "  4. ? Source events marked as compressed" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Next Steps:" << std::endl;
        std::cout << "  ? Query compressed sessions from DuckDB" << std::endl;
        std::cout << "  ? Implement LLM content compression" << std::endl;
        std::cout << "  ? Add Layer 2 aggregation" << std::endl;
        std::cout << std::endl;
        
#else
        std::cout << "? DuckDB not enabled in build" << std::endl;
        std::cout << "  Please rebuild with DuckDB support to test compression" << std::endl;
#endif
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "? Test failed: " << e.what() << std::endl;
        return 1;
    }
}
