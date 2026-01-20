/**
 * @file LinguaCoreMain.cpp
 * @brief Main entry point for LinguaCore service
 */

#include "pe_base/logger.h"  // Add logger first
#include "pe_base/config_manager.h"  // Add ConfigManager
#include "linguacore/LinguaCore.h"
#include <iostream>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for Ctrl+C
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        PE_INFO("Shutdown signal received...");
        g_shutdown_requested = true;
    }
}

void printUsage(const char* program_name) {
    PE_INFO("LinguaCore - Automatic Content Summarization Service");
    PE_INFO("Usage: " << program_name << " [options]");
    PE_INFO("Options:");
    PE_INFO("  -c, --config <path>    Path to config.ini file (default: config.ini)");
    PE_INFO("  -h, --help             Show this help message");
    PE_INFO("  -v, --version          Show version information");
}

void printVersion() {
    PE_INFO("LinguaCore version 1.0.0");
    PE_INFO("Part of PerceptionEngine v2.0.0");
}

int main(int argc, char* argv[]) {
    // =========================================
    // Initialize Logger FIRST (before anything)
    // =========================================
    std::filesystem::path log_path = "";
    if (auto* p_appdata = getenv("APPDATA")) {
        log_path =
            std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
    }
    pe_base::LogWriter::SetLogFilePrefix(
        (log_path / "LinguaCore").generic_string());

    PE_INFO("========================================");
    PE_INFO("    LinguaCore Service");
    PE_INFO("    Automatic Content Summarization");
    PE_INFO("========================================");
    
    // Parse command line arguments
    std::string config_path = "config.ini";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                PE_ERROR("Error: --config requires a path argument");
                return 1;
            }
        } else {
            PE_ERROR("Error: Unknown option: " << arg);
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // =========================================
    // Load Configuration using ConfigManager
    // =========================================
    PE_INFO("Loading configuration from: " << config_path);
    
    auto& configManager = pe_base::ConfigManager::GetInstance();
    
    if (!configManager.LoadConfig(config_path)) {
        PE_WARN("Failed to load config.ini, using default values");
        PE_WARN("Error: " << configManager.GetLastError());
    } else {
        PE_INFO("Configuration loaded successfully");
    }
    
    // Validate configuration
    if (!configManager.ValidateConfiguration()) {
        PE_ERROR("Configuration validation failed:");
        PE_ERROR(configManager.GetLastError());
        PE_WARN("Continuing with best-effort configuration...");
    } else {
        PE_INFO("Configuration validated successfully");
    }
    
    try {
        // =========================================
        // Build LinguaCore Configuration from ConfigManager
        // =========================================
        PE_INFO("Building LinguaCore configuration from ConfigManager");
        
        linguacore::LinguaCoreConfig config;
        
        // Get model paths (already resolved to absolute paths)
        config.embedding_model_path = configManager.GetEmbeddingModelPathUtf8();
        config.llm_model_path = configManager.GetLLMModelPath();
        
        // Get LinguaCore settings
        config.check_interval_seconds = configManager.GetCheckIntervalSeconds();
        config.batch_size = configManager.GetLinguaCoreBatchSize();
        config.verbose = configManager.IsLinguaCoreVerbose();
        
        // Get PostgreSQL settings
        config.pg_host = configManager.GetPostgreSQLHost();
        config.pg_port = configManager.GetPostgreSQLPort();
        config.pg_dbname = configManager.GetPostgreSQLDatabase();
        config.pg_user = configManager.GetPostgreSQLUser();
        config.pg_password = configManager.GetPostgreSQLPassword();
        config.pg_table = configManager.GetPostgreSQLTable();
        
        // Get LLM settings
        config.llm_max_tokens = configManager.GetLLMMaxTokens();
        config.llm_temperature = configManager.GetLLMTemperature();
        
        // Get Qdrant settings
        config.qdrant_host = configManager.GetQdrantHost();
        config.qdrant_port = configManager.GetQdrantPort();
        config.qdrant_collection = configManager.GetQdrantCollection();
		config.pg_max_undelete_length = configManager.GetPostgreSQLMaxundeletelength();
        
        PE_INFO("Configuration summary:");
        PE_INFO("  Embedding model: " << config.embedding_model_path);
        PE_INFO("  LLM model: " << config.llm_model_path);
        PE_INFO("  Check interval: " << config.check_interval_seconds << " seconds");
        PE_INFO("  Batch size: " << config.batch_size);
        PE_INFO("  PostgreSQL: " << config.pg_host << ":" << config.pg_port << "/" << config.pg_dbname);
        PE_INFO("  Qdrant: " << config.qdrant_host << ":" << config.qdrant_port);
        
        // Verify model files exist
        if (!std::filesystem::exists(config.embedding_model_path)) {
            PE_ERROR("Embedding model file not found: " << config.embedding_model_path);
            return 1;
        }
        
        if (!std::filesystem::exists(config.llm_model_path)) {
            PE_ERROR("LLM model file not found: " << config.llm_model_path);
            return 1;
        }
        
        PE_INFO("All configurations validated successfully");
        
        // Install signal handler
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Create LinguaCore service
        linguacore::LinguaCore service(config);
        
        // Initialize all components
        PE_INFO("Initializing LinguaCore components...");
        if (!service.initialize()) {
            PE_ERROR("Failed to initialize LinguaCore service");
            return 1;
        }
        PE_INFO("LinguaCore components initialized successfully");
        
        // Start the service
        if (!service.start()) {
            PE_ERROR("Failed to start LinguaCore service");
            return 1;
        }
        
        PE_INFO("========================================");
        PE_INFO("Service is running. Press Ctrl+C to stop.");
        PE_INFO("========================================");
        
        // Main loop - wait for shutdown signal
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Print statistics periodically (every 60 seconds)
            static auto last_stats_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_stats_time).count();
            
            if (elapsed >= 60) {
                PE_INFO("--- Service Statistics ---");
                PE_INFO(service.getStatistics());
                PE_INFO("-------------------------");
                last_stats_time = now;
            }
        }
        
        // Graceful shutdown
        PE_INFO("Stopping LinguaCore service...");
        service.stop();
        
        PE_INFO("--- Final Statistics ---");
        PE_INFO(service.getStatistics());
        PE_INFO("------------------------");
        
        PE_INFO("LinguaCore service stopped successfully.");
        return 0;
        
    } catch (const std::exception& e) {
        PE_ERROR("FATAL ERROR: " << e.what());
        return 1;
    }
}
