#include "layer0/DataIngestion.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace perception;

int main() {
    // Initialize logger
    Logger::getInstance().setLogLevel(LogLevel::INFO);
    
    std::cout << "=========================================" << std::endl;
    std::cout << " Perception Engine Database Example" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        // Get the absolute path of the executable directory
        std::filesystem::path exePath = std::filesystem::current_path();
        std::filesystem::path dataDir = exePath / "perception_data";
        
        // Ensure the data directory exists
        if (!std::filesystem::exists(dataDir)) {
            std::filesystem::create_directories(dataDir);
            LOG_INFO("Created data directory: " + dataDir.string());
        }
        
        // Initialize data ingestion with absolute path
        std::filesystem::path dbPath = dataDir / "example_raw_events.db";
        std::string deviceId = "example_device_001";
        
        LOG_INFO("Creating database at: " + dbPath.string());
        layer0::DataIngestion ingestion(dbPath.string(), deviceId);
        
        // Example 1: Ingest a single event
        LOG_INFO("Example 1: Ingesting single event");
        
        layer0::RawEvent event1;
        event1.timestamp = utils::now();
        event1.appName = "chrome.exe";
        event1.windowTitle = "GitHub - perception_engine";
        event1.url = "https://github.com/Howard-Cheng/perception_engine";
        event1.screenContent = "# Perception Engine\n\nA cross-device context collection system...";
        event1.interactionCount = 5;
        event1.dwellTimeSeconds = 45;
        
        // Add mouse event
        MouseEvent mouseEvent1;
        mouseEvent1.timestamp = utils::now();
        mouseEvent1.eventType = "Copy";
        mouseEvent1.content = "perception_engine";
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
        
        EventId eventId1 = ingestion.ingestEvent(event1);
        std::cout << "✓ Ingested event: " << eventId1 << std::endl;
        std::cout << std::endl;
        
        // Example 2: Ingest multiple events
        LOG_INFO("Example 2: Batch ingesting multiple events");
        
        std::vector<layer0::RawEvent> events;
        for (int i = 0; i < 10; ++i) {
            layer0::RawEvent event;
            event.timestamp = utils::now();
            event.appName = "code.exe";
            event.windowTitle = "main.cpp - perception_engine";
            event.screenContent = "int main() { /* code */ }";
            event.interactionCount = 3;
            event.dwellTimeSeconds = 30;
            
            event.systemInfo.batteryPercent = 80 - i;
            event.systemInfo.isCharging = false;
            event.systemInfo.cpuUsage = 20.0 + i;
            event.systemInfo.memoryUsage = 50.0 + i * 0.5;
            
            events.push_back(event);
            
            // Small delay to simulate different timestamps
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto eventIds = ingestion.ingestEvents(events);
        std::cout << "✓ Batch ingested " << eventIds.size() << " events" << std::endl;
        std::cout << std::endl;
        
        // Example 3: Get statistics
        LOG_INFO("Example 3: Database statistics");
        
        int totalEvents = ingestion.getEventCount();
        int uncompressedEvents = ingestion.getUncompressedEventCount();
        int todayEvents = ingestion.getTodayEventCount();
        
        std::cout << "Database Statistics:" << std::endl;
        std::cout << "  Total events: " << totalEvents << std::endl;
        std::cout << "  Uncompressed: " << uncompressedEvents << std::endl;
        std::cout << "  Today: " << todayEvents << std::endl;
        std::cout << std::endl;
        
        // Example 4: High-engagement event with multiple interactions
        LOG_INFO("Example 4: High-engagement event");
        
        layer0::RawEvent highEngagementEvent;
        highEngagementEvent.timestamp = utils::now();
        highEngagementEvent.appName = "WINWORD.EXE";
        highEngagementEvent.windowTitle = "Q4_Report.docx - Word";
        highEngagementEvent.screenContent = "Q4 Financial Report\n\nRevenue: $2.3M\nGrowth: 15% YoY\n...";
        highEngagementEvent.interactionCount = 25;
        highEngagementEvent.dwellTimeSeconds = 180;  // 3 minutes
        
        // Add multiple mouse events
        MouseEvent copyEvent;
        copyEvent.timestamp = utils::now();
        copyEvent.eventType = "Copy";
        copyEvent.content = "Revenue: $2.3M";
        copyEvent.posX = 150;
        copyEvent.posY = 250;
        copyEvent.elementType = "Text";
        highEngagementEvent.mouseEvents.push_back(copyEvent);
        
        MouseEvent selectEvent;
        selectEvent.timestamp = utils::now();
        selectEvent.eventType = "TextSelection";
        selectEvent.content = "Growth: 15% YoY";
        selectEvent.posX = 150;
        selectEvent.posY = 280;
        selectEvent.elementType = "Text";
        highEngagementEvent.mouseEvents.push_back(selectEvent);
        
        highEngagementEvent.systemInfo.batteryPercent = 75;
        highEngagementEvent.systemInfo.isCharging = true;
        highEngagementEvent.systemInfo.cpuUsage = 25.5;
        highEngagementEvent.systemInfo.memoryUsage = 60.2;
        
        EventId highEngagementId = ingestion.ingestEvent(highEngagementEvent);
        std::cout << "✓ Ingested high-engagement event: " << highEngagementId << std::endl;
        std::cout << "  Interaction count: " << highEngagementEvent.interactionCount << std::endl;
        std::cout << "  Dwell time: " << highEngagementEvent.dwellTimeSeconds << "s" << std::endl;
        std::cout << "  Mouse events: " << highEngagementEvent.mouseEvents.size() << std::endl;
        std::cout << std::endl;
        
        // Final statistics
        totalEvents = ingestion.getEventCount();
        std::cout << "=========================================" << std::endl;
        std::cout << "Final Statistics:" << std::endl;
        std::cout << "  Total events in database: " << totalEvents << std::endl;
        std::cout << "  Database path: " << dbPath.string() << std::endl;
        std::cout << "=========================================" << std::endl;
        
        LOG_INFO("Example completed successfully!");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Example failed: " + std::string(e.what()));
        std::cerr << "\nTroubleshooting:" << std::endl;
        std::cerr << "  1. Make sure you have write permissions in the current directory" << std::endl;
        std::cerr << "  2. Try running from the build directory: cd build && ./bin/basic_ingestion_example" << std::endl;
        return 1;
    }
    
    return 0;
}
