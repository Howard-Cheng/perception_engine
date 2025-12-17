/**
 * @file LinguaCoreMain.cpp
 * @brief Main entry point for LinguaCore service
 */

#include "linguacore/LinguaCore.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for Ctrl+C
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown signal received..." << std::endl;
        g_shutdown_requested = true;
    }
}

void printUsage(const char* program_name) {
    std::cout << "LinguaCore - Automatic Content Summarization Service\n\n";
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <path>    Path to config.ini file (default: config.ini)\n";
    std::cout << "  -h, --help             Show this help message\n";
    std::cout << "  -v, --version          Show version information\n";
    std::cout << "\n";
}

void printVersion() {
    std::cout << "LinguaCore version 1.0.0\n";
    std::cout << "Part of PerceptionEngine v2.0.0\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "    LinguaCore Service\n";
    std::cout << "    Automatic Content Summarization\n";
    std::cout << "========================================\n\n";
    
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
                std::cerr << "Error: --config requires a path argument\n";
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Install signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Load configuration
        std::cout << "Loading configuration from: " << config_path << "\n\n";
        auto config = linguacore::loadConfiguration(config_path);
        
        // Create and start LinguaCore service
        linguacore::LinguaCore service(config);
        
        if (!service.start()) {
            std::cerr << "Failed to start LinguaCore service\n";
            return 1;
        }
        
        std::cout << "\n========================================\n";
        std::cout << "Service is running. Press Ctrl+C to stop.\n";
        std::cout << "========================================\n\n";
        
        // Main loop - wait for shutdown signal
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Print statistics periodically (every 60 seconds)
            static auto last_stats_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - last_stats_time).count();
            
            if (elapsed >= 60) {
                std::cout << "\n--- Service Statistics ---\n";
                std::cout << service.getStatistics() << "\n";
                std::cout << "-------------------------\n\n";
                last_stats_time = now;
            }
        }
        
        // Graceful shutdown
        std::cout << "\nStopping LinguaCore service...\n";
        service.stop();
        
        std::cout << "\n--- Final Statistics ---\n";
        std::cout << service.getStatistics() << "\n";
        std::cout << "------------------------\n\n";
        
        std::cout << "LinguaCore service stopped successfully.\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}
