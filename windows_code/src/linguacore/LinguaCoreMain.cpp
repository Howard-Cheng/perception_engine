/**
 * @file LinguaCoreMain.cpp
 * @brief Main entry point for LinguaCore service
 */

#include "pe_base/logger.h"  // Add logger first
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
    
    // Install signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Load configuration
        PE_INFO("Loading configuration from: " << config_path);
        auto config = linguacore::loadConfiguration(config_path);
        
        // Create and start LinguaCore service
        linguacore::LinguaCore service(config);
        
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
