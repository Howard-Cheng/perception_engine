#include "common/Logger.h"
#include "common/DatabaseConfig.h"
#include "layer0/DataIngestion.h"
#include "layer0/SchemaManager.h"
#include "layer1/CompressionPipeline.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

using namespace perception;

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    LOG_INFO("Received signal " + std::to_string(signal) + ", shutting down...");
    g_running = false;
}

void printBanner() {
    std::cout << "=========================================" << std::endl;
    std::cout << " Perception Engine Database Service" << std::endl;
    std::cout << " Layered Architecture for Context Collection" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
}

void printHelp() {
    std::cout << "Usage: perception_db_service [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --base-dir PATH        Base directory for databases (default: ./perception_data)" << std::endl;
    std::cout << "  --device-id ID         Device identifier (default: pc_001)" << std::endl;
    std::cout << "  --log-level LEVEL      Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)" << std::endl;
    std::cout << "  --log-file PATH        Log file path (optional)" << std::endl;
    std::cout << "  --compression-interval Compression interval in seconds (default: 300)" << std::endl;
    std::cout << "  --cleanup-interval     Cleanup interval in seconds (default: 3600)" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
    std::cout << std::endl;
}

struct ServiceConfig {
    std::string baseDir = "./perception_data";
    std::string deviceId = "pc_001";
    std::string logLevel = "INFO";
    std::string logFile = "";
    int compressionInterval = 20;  // 5 minutes
    int cleanupInterval = 3600;     // 1 hour
};

ServiceConfig parseArguments(int argc, char* argv[]) {
    ServiceConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printHelp();
            exit(0);
        } else if (arg == "--base-dir" && i + 1 < argc) {
            config.baseDir = argv[++i];
        } else if (arg == "--device-id" && i + 1 < argc) {
            config.deviceId = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.logLevel = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            config.logFile = argv[++i];
        } else if (arg == "--compression-interval" && i + 1 < argc) {
            config.compressionInterval = std::stoi(argv[++i]);
        } else if (arg == "--cleanup-interval" && i + 1 < argc) {
            config.cleanupInterval = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printHelp();
            exit(1);
        }
    }
    
    return config;
}

LogLevel parseLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARNING") return LogLevel::WARNING;
    if (level == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;
}

class DatabaseService {
public:
    DatabaseService(const ServiceConfig& config)
        : config_(config)
        , dbConfig_(config.baseDir) {
        
        // Initialize logger
        Logger::getInstance().setLogLevel(parseLogLevel(config.logLevel));
        if (!config.logFile.empty()) {
            Logger::getInstance().enableFileLogging(config.logFile);
        }
        
        LOG_INFO("Initializing Database Service");
        LOG_INFO("Base directory: " + config.baseDir);
        LOG_INFO("Device ID: " + config.deviceId);
        
        // Initialize data ingestion (Layer 0)
        ingestion_ = std::make_unique<layer0::DataIngestion>(
            dbConfig_.getSqlitePath().string(),
            config.deviceId
        );
        
#ifdef DUCKDB_ENABLED
        // Initialize compression pipeline (Layer 1)
        SessionConfig sessionConfig;
        sessionConfig.idleThresholdSeconds = dbConfig_.idleThresholdSeconds;
        
        pipeline_ = std::make_unique<layer1::CompressionPipeline>(
            dbConfig_.getSqlitePath().string(),
            dbConfig_.getDuckdbPath().string(),
            sessionConfig
        );
        
        LOG_INFO("Compression pipeline initialized with DuckDB support");
#else
        LOG_WARNING("DuckDB support not enabled - compression disabled");
#endif
        
        LOG_INFO("Database service initialized successfully");
    }
    
    void run() {
        LOG_INFO("Starting database service main loop");
        LOG_INFO("Compression interval: " + std::to_string(config_.compressionInterval) + "s");
        LOG_INFO("Cleanup interval: " + std::to_string(config_.cleanupInterval) + "s");
        LOG_INFO("Press Ctrl+C to stop");
        LOG_INFO("");
        
        auto lastCompression = std::chrono::steady_clock::now();
        auto lastCleanup = std::chrono::steady_clock::now();
        auto lastStats = std::chrono::steady_clock::now();
        
        while (g_running) {
            auto now = std::chrono::steady_clock::now();
            
            // Print statistics every minute
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStats).count() >= 10) {
                printStatistics();
                lastStats = now;
            }
            
            // Run compression pipeline
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCompression).count() 
                >= config_.compressionInterval) {
                runCompression();
                lastCompression = now;
            }
            
            // Run cleanup
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCleanup).count() 
                >= config_.cleanupInterval) {
                runCleanup();
                lastCleanup = now;
            }
            
            // Sleep for 1 second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        LOG_INFO("Database service stopped");
    }
    
private:
    void runCompression() {
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        LOG_INFO("Running compression pipeline...");
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        try {
            // Check for uncompressed events in Layer 0 (SQLite)
            int uncompressed = ingestion_->getUncompressedEventCount();
            
            if (uncompressed == 0) {
                LOG_INFO("No uncompressed events to process");
                LOG_INFO("");
                return;
            }
            
            LOG_INFO("Found " + std::to_string(uncompressed) + " uncompressed events in Layer 0");
            
#ifdef DUCKDB_ENABLED
            // Execute compression pipeline:
            // 1. Fetch uncompressed events from SQLite (Layer 0)
            // 2. Detect interaction sessions
            // 3. Calculate engagement scores
            // 4. Extract and classify content
            // 5. Compress content (with LLM if available)
            // 6. Store compressed sessions to DuckDB (Layer 1)
            // 7. Mark source events as compressed in SQLite
            
            LOG_INFO("Starting Layer 0 → Layer 1 compression...");
            int sessionsCompressed = pipeline_->processUncompressedEvents();
            
            if (sessionsCompressed > 0) {
                LOG_INFO("✓ Successfully compressed " + std::to_string(sessionsCompressed) + " sessions");
                
                // Verify compression
                int remainingUncompressed = ingestion_->getUncompressedEventCount();
                LOG_INFO("Remaining uncompressed events: " + std::to_string(remainingUncompressed));
                
                // Get pipeline statistics
                auto stats = pipeline_->getStatistics();
                LOG_INFO("Pipeline stats:");
                LOG_INFO("  - Events processed: " + std::to_string(stats.eventsProcessed));
                LOG_INFO("  - Sessions created: " + std::to_string(stats.sessionsCompressed));
                
            } else {
                LOG_WARNING("No sessions were compressed (events may not meet session criteria)");
            }
#else
            LOG_WARNING("Compression skipped - DuckDB support not enabled");
            LOG_INFO("Please rebuild with DUCKDB_ENABLED flag to enable compression");
#endif
            
        } catch (const std::exception& e) {
            LOG_ERROR("Compression failed: " + std::string(e.what()));
        }
        
        LOG_INFO("");
    }
    
    void runCleanup() {
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        LOG_INFO("Running data cleanup...");
        LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        try {
#ifdef DUCKDB_ENABLED
            // Clean up old compressed sessions in DuckDB (Layer 1)
            int retentionDays = dbConfig_.compressedRetentionDays;
            LOG_INFO("Cleaning up compressed sessions older than " + 
                    std::to_string(retentionDays) + " days");
            
            int deletedSessions = pipeline_->cleanupOldCompressedSessions(retentionDays);
            
            if (deletedSessions > 0) {
                LOG_INFO("✓ Cleaned up " + std::to_string(deletedSessions) + 
                        " old compressed sessions from DuckDB");
            } else {
                LOG_INFO("No old sessions to clean up");
            }
#else
            LOG_INFO("Cleanup skipped - DuckDB support not enabled");
#endif
            
            // TODO: Clean up raw events from SQLite (Layer 0) based on retention policy
            // Keep raw events for rawRetentionHours, then delete after compression
            
        } catch (const std::exception& e) {
            LOG_ERROR("Cleanup failed: " + std::string(e.what()));
        }
        
        LOG_INFO("");
    }
    
    void printStatistics() {
        try {
            LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            LOG_INFO("Database Statistics");
            LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            
            // Layer 0 (SQLite) statistics
            int total = ingestion_->getEventCount();
            int uncompressed = ingestion_->getUncompressedEventCount();
            int today = ingestion_->getTodayEventCount();
            
            LOG_INFO("Layer 0 (SQLite - Raw Events):");
            LOG_INFO("  Total events: " + std::to_string(total));
            LOG_INFO("  Uncompressed: " + std::to_string(uncompressed));
            LOG_INFO("  Today: " + std::to_string(today));
            
#ifdef DUCKDB_ENABLED
            // Layer 1 (DuckDB) statistics
            auto stats = pipeline_->getStatistics();
            LOG_INFO("Layer 1 (DuckDB - Compressed Sessions):");
            LOG_INFO("  Total sessions compressed: " + std::to_string(stats.sessionsCompressed));
            LOG_INFO("  Total events processed: " + std::to_string(stats.eventsProcessed));
#endif
            
            LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            LOG_INFO("");
            
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to get statistics: " + std::string(e.what()));
        }
    }
    
    ServiceConfig config_;
    DatabaseConfig dbConfig_;
    std::unique_ptr<layer0::DataIngestion> ingestion_;
#ifdef DUCKDB_ENABLED
    std::unique_ptr<layer1::CompressionPipeline> pipeline_;
#endif
};

int main(int argc, char* argv[]) {
    try {
        // Setup signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        printBanner();
        
        // Parse command line arguments
        ServiceConfig config = parseArguments(argc, argv);
        
        // Create and run service
        DatabaseService service(config);
        service.run();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
