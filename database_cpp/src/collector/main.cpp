#include "collector/DataCollector.h"
#include "common/Logger.h"
#include <iostream>
#include <csignal>
#include <string>
#include <atomic>
#include <thread>
#include <filesystem>

using namespace perception;
using namespace perception::collector;

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << std::endl;
    LOG_INFO("Received shutdown signal, stopping collector...");
    g_running = false;
}

void printBanner() {
    std::cout << "=========================================" << std::endl;
    std::cout << " Perception Engine Data Collector" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
}

void printHelp() {
    std::cout << "Usage: perception_data_collector [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --api-url URL          Perception Engine API URL (default: http://localhost:8777/context)" << std::endl;
    std::cout << "  --storage BACKEND      Storage backend: sqlite or elasticsearch (default: sqlite)" << std::endl;
    std::cout << "  --db-path PATH         SQLite database file path (default: ./perception_data/raw_events.db)" << std::endl;
    std::cout << "  --es-url URL           Elasticsearch URL (default: http://localhost:9200)" << std::endl;
    std::cout << "  --es-index NAME        Elasticsearch index name (default: perception_raw_events)" << std::endl;
    std::cout << "  --device-id ID         Device identifier (default: pc_001)" << std::endl;
    std::cout << "  --poll-interval SEC    Polling interval in seconds (default: 5)" << std::endl;
    std::cout << "  --timeout SEC          Connection timeout in seconds (default: 10)" << std::endl;
    std::cout << "  --max-retries NUM      Maximum retry attempts (default: 3)" << std::endl;
    std::cout << "  --log-level LEVEL      Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)" << std::endl;
    std::cout << "  --log-file PATH        Log file path (optional)" << std::endl;
    std::cout << "  --help, -h             Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  # Use SQLite (default)" << std::endl;
    std::cout << "  perception_data_collector --storage sqlite" << std::endl;
    std::cout << std::endl;
    std::cout << "  # Use Elasticsearch" << std::endl;
    std::cout << "  perception_data_collector --storage elasticsearch --es-url http://localhost:9200" << std::endl;
    std::cout << std::endl;
}

CollectorConfig parseArguments(int argc, char* argv[]) {
    CollectorConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printHelp();
            exit(0);
        }
        else if (arg == "--api-url" && i + 1 < argc) {
            config.apiUrl = argv[++i];
        }
        else if (arg == "--storage" && i + 1 < argc) {
            std::string backend = argv[++i];
            if (backend == "sqlite") {
                config.storageBackend = StorageBackend::SQLITE;
            } else if (backend == "elasticsearch" || backend == "es") {
                config.storageBackend = StorageBackend::ELASTICSEARCH;
            } else {
                std::cerr << "Unknown storage backend: " << backend << std::endl;
                std::cerr << "Valid options: sqlite, elasticsearch" << std::endl;
                exit(1);
            }
        }
        else if (arg == "--db-path" && i + 1 < argc) {
            config.dbPath = argv[++i];
        }
        else if (arg == "--es-url" && i + 1 < argc) {
            config.elasticsearchUrl = argv[++i];
        }
        else if (arg == "--es-index" && i + 1 < argc) {
            config.elasticsearchIndex = argv[++i];
        }
        else if (arg == "--device-id" && i + 1 < argc) {
            config.deviceId = argv[++i];
        }
        else if (arg == "--poll-interval" && i + 1 < argc) {
            config.pollIntervalSeconds = std::stoi(argv[++i]);
        }
        else if (arg == "--timeout" && i + 1 < argc) {
            config.connectionTimeoutSeconds = std::stoi(argv[++i]);
        }
        else if (arg == "--max-retries" && i + 1 < argc) {
            config.maxRetries = std::stoi(argv[++i]);
        }
        else if (arg == "--log-level" && i + 1 < argc) {
            std::string level = argv[++i];
            if (level == "DEBUG") Logger::getInstance().setLogLevel(LogLevel::DEBUG);
            else if (level == "INFO") Logger::getInstance().setLogLevel(LogLevel::INFO);
            else if (level == "WARNING") Logger::getInstance().setLogLevel(LogLevel::WARNING);
            else if (level == "ERROR") Logger::getInstance().setLogLevel(LogLevel::ERROR);
        }
        else if (arg == "--log-file" && i + 1 < argc) {
            Logger::getInstance().enableFileLogging(argv[++i]);
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printHelp();
            exit(1);
        }
    }

    return config;
}

int main(int argc, char* argv[]) {
    try {
        // Setup signal handlers for graceful shutdown
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);

        printBanner();

        // Parse command line arguments
        CollectorConfig config = parseArguments(argc, argv);

        // Only set dbPath if using SQLite backend
        if (config.storageBackend == StorageBackend::SQLITE) {
            std::filesystem::path exePath = std::filesystem::current_path();
            std::filesystem::path dataDir = exePath / "perception_data";

            // Ensure the data directory exists
            if (!std::filesystem::exists(dataDir)) {
                std::filesystem::create_directories(dataDir);
                LOG_INFO("Created data directory: " + dataDir.string());
            }
            config.dbPath = (dataDir / "raw_events.db").string();
        }

        // Initialize logger
        Logger::getInstance().setLogLevel(LogLevel::INFO);

        // Create data collector
        DataCollector collector(config);

        // Start collection in a separate thread
        std::thread collectorThread([&collector]() {
            collector.start();
            });

        // Wait for shutdown signal
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop collector
        collector.stop();

        // Wait for collector thread to finish
        if (collectorThread.joinable()) {
            collectorThread.join();
        }

        // Print statistics
        std::cout << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << " Collection Statistics" << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << "  Total events collected: " << collector.getTotalEventsCollected() << std::endl;
        std::cout << "  Failed requests: " << collector.getFailedRequests() << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << std::endl;

        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
