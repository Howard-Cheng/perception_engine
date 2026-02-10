/**
 * @file LinguaServerMain.cpp
 * @brief Main entry point for LinguaCore subprocess
 *
 * This executable runs as a child process spawned by the parent application.
 * It communicates via anonymous pipes (stdin/stdout) using a custom binary protocol.
 *
 * Architecture:
 * - Parent Process (PerceptionEngine) spawns LinguaCoreServer.exe
 * - Communication via anonymous pipes (stdin for receiving, stdout for sending)
 * - Message framing: [size_t size][payload]
 * - Payload format: [uint32_t command][data...]
 *
 * Lifecycle:
 * 1. Receive stdin/stdout handles from parent
 * 2. Initialize LinguaCore
 * 3. Enter message loop reading from stdin
 * 4. Process commands and send responses via stdout
 * 5. Exit when receiving SIZE_MAX or pipe breaks
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "pe_base/logger.h"
#include "pe_base/config_manager.h"
#include "linguacore/LinguaCore.h"
#include "linguacore/LinguaServer.h"
#include "pe_base/task_queue/task_queue.h"
#include <filesystem>
#include <iostream>
#include <vector>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

 // Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested{ false };

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        PE_INFO("Shutdown signal received...");
        g_shutdown_requested = true;
    }
}

void printUsage(const char* program_name) {
    PE_INFO("LinguaCoreServer - IPC Subprocess for LinguaCore");
    PE_INFO("Usage: " << program_name << " [options]");
    PE_INFO("Options:");
    PE_INFO("  -c, --config <path>    Path to config.ini file (default: config.ini)");
    PE_INFO("  --background           Run in background mode (hide console window)");
    PE_INFO("  -h, --help             Show this help message");
    PE_INFO("  -v, --version          Show version information");
    PE_INFO("");
    PE_INFO("Note: This program is typically launched by parent process via pipes.");
}

void printVersion() {
    PE_INFO("LinguaCoreServer version 1.0.0");
    PE_INFO("Part of PerceptionEngine v2.0.0");
    PE_INFO("IPC subprocess for real-time NLP requests");
}

namespace {

    /**
     * @brief Read exact number of bytes from pipe
     * @return true if successful, false on error or EOF
     */
    bool ReadFromPipe(HANDLE hPipe, uint8_t* buffer, size_t size) {
        uint8_t* ptr = buffer;
        size_t remaining = size;

        while (remaining > 0) {
            DWORD chunk = 0;
            BOOL ok = ReadFile(hPipe, ptr, static_cast<DWORD>(remaining), &chunk, nullptr);

            if (!ok) {
                DWORD err = ::GetLastError();
                if (err == ERROR_BROKEN_PIPE) {
                    PE_INFO("Pipe broken, parent process disconnected");
                    return false;
                }
                PE_ERROR("ReadFile failed, error: " << err);
                return false;
            }

            if (chunk == 0) {
                PE_INFO("Pipe closed (0 bytes read)");
                return false;
            }

            remaining -= chunk;
            ptr += chunk;
        }

        return true;
    }

    /**
     * @brief Load configuration using ConfigManager
     */
    linguacore::LinguaCoreConfig LoadConfiguration(const std::string& config_path) {
        linguacore::LinguaCoreConfig config;

        PE_INFO("Loading configuration from: " << config_path);

        auto& configManager = pe_base::ConfigManager::GetInstance();

        if (!configManager.LoadConfig(config_path)) {
            PE_WARN("Failed to load config.ini, using default values");
            PE_WARN("Error: " << configManager.GetLastError());
        }
        else {
            PE_INFO("Configuration loaded successfully");
        }

        // Validate configuration
        if (!configManager.ValidateConfiguration()) {
            PE_ERROR("Configuration validation failed:");
            PE_ERROR(configManager.GetLastError());
            PE_WARN("Continuing with best-effort configuration...");
        }
        else {
            PE_INFO("Configuration validated successfully");
        }

        // Build LinguaCore Configuration from ConfigManager
        PE_INFO("Building LinguaCore configuration from ConfigManager");

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
        config.pg_max_undelete_length = configManager.GetPostgreSQLMaxundeletelength();
        config.pg_out_of_date_hour = configManager.GetPostgreSQLOutofdatehour();

        // Get LLM settings
        config.llm_max_tokens = configManager.GetLLMMaxTokens();
        config.llm_temperature = configManager.GetLLMTemperature();

        // Get Qdrant settings
        config.qdrant_host = configManager.GetQdrantHost();
        config.qdrant_port = configManager.GetQdrantPort();
        config.qdrant_collection = configManager.GetQdrantCollection();

        // QtCore settings
        config.qtcore_enabled = true;
        config.qtcore_dll_path = configManager.ResolvePath("quantum-sdk-1.0.10.dll");
        config.qtcore_model = "default";

        // Check if QtCore should be enabled (via environment variable)
        if (auto* qtcore_env = std::getenv("QTCORE_ENABLED")) {
            if (std::string(qtcore_env) == "1" || std::string(qtcore_env) == "true") {
                config.qtcore_enabled = true;
            }
        }

        // Check if custom DLL path is set
        if (auto* qtcore_dll = std::getenv("QTCORE_DLL_PATH")) {
            config.qtcore_dll_path = qtcore_dll;
        }

        // Check if custom model is set
        if (auto* qtcore_model = std::getenv("QTCORE_MODEL")) {
            config.qtcore_model = qtcore_model;
        }

        PE_INFO("Configuration summary:");
        PE_INFO("  Embedding model: " << config.embedding_model_path);
        PE_INFO("  LLM model: " << config.llm_model_path);
        PE_INFO("  Check interval: " << config.check_interval_seconds << " seconds");
        PE_INFO("  Batch size: " << config.batch_size);
        PE_INFO("  PostgreSQL: " << config.pg_host << ":" << config.pg_port << "/" << config.pg_dbname);
        PE_INFO("  Qdrant: " << config.qdrant_host << ":" << config.qdrant_port);
        PE_INFO("  QtCore enabled: " << (config.qtcore_enabled ? "Yes" : "No"));
        PE_INFO("  QtCore DLL path: " << config.qtcore_dll_path);
        PE_INFO("  QtCore model: " << config.qtcore_model);

        return config;
    }

} // anonymous namespace

void setLogStatus() {
    bool enablelocallog = false;

    // =========================================
    // Read Log setting from Registry
    // =========================================
    {
        HKEY hKey;
        LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Lenovo\\Perception",
            0, KEY_READ, &hKey);

        if (result == ERROR_SUCCESS) {
            DWORD logValue = 0;
            DWORD dataSize = sizeof(DWORD);
            DWORD dataType = 0;

            result = RegQueryValueExA(hKey, "Log", nullptr, &dataType,
                reinterpret_cast<LPBYTE>(&logValue), &dataSize);

            if (result == ERROR_SUCCESS && dataType == REG_DWORD) {
                enablelocallog = (logValue == 1);
            }

            RegCloseKey(hKey);
        }
    }

    // =========================================
    // Initialize Logger FIRST (before anything)
    // =========================================
    if (enablelocallog) {
        std::filesystem::path log_path = "";
        if (auto* p_appdata = getenv("APPDATA")) {
            log_path =
                std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
        }
        pe_base::LogWriter::SetLogFilePrefix(
            (log_path / "LinguaCoreServer").generic_string());
    }

}
/**
 * @brief Main entry point for LinguaCoreServer subprocess
 *
 * Command line: LinguaCoreServer.exe [options]
 * Parent process must:
 * 1. Create anonymous pipes
 * 2. Set STARTF_USESTDHANDLES in STARTUPINFO
 * 3. Pass pipe handles as stdin/stdout to child process
 */
int main(int argc, char* argv[]) {
    setLogStatus();
    PE_INFO("========================================");
    PE_INFO("    LinguaCoreServer Subprocess");
    PE_INFO("    IPC Handler for NLP Requests");
    PE_INFO("========================================");

    // Parse command line arguments
    std::string config_path = "config.ini";
    bool background_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }
        else if (arg == "--background") {
            background_mode = true;
        }
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            }
            else {
                PE_ERROR("Error: --config requires a path argument");
                return 1;
            }
        }
        else {
            PE_ERROR("Error: Unknown option: " << arg);
            printUsage(argv[0]);
            return 1;
        }
    }

    // Hide console window if running in background mode
    if (background_mode) {
        PE_INFO("Running in background mode (hiding console window)...");
        HWND consoleWindow = GetConsoleWindow();
        if (consoleWindow) {
            ShowWindow(consoleWindow, SW_HIDE);
        }
    }

    PE_INFO("PID: " << GetCurrentProcessId());

    // Install signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Get stdin/stdout handles from parent process
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
            PE_ERROR("Invalid stdin/stdout handles");
            PE_ERROR("stdin: " << hStdin << ", stdout: " << hStdout);
            PE_ERROR("This process must be launched with pipe handles!");
            return 1;
        }

        PE_INFO("Handles acquired - stdin: " << hStdin << ", stdout: " << hStdout);

        // Load configuration
        auto config = LoadConfiguration(config_path);

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

        // Create LinguaCore service
        auto core = std::make_shared<linguacore::LinguaCore>(config);

        // Initialize all components
        PE_INFO("Initializing LinguaCore components...");
        if (!core->initialize()) {
            PE_ERROR("Failed to initialize LinguaCore service");
            return 1;
        }
        PE_INFO("LinguaCore components initialized successfully");

        // Start the service (background thread for periodic tasks)
        if (!core->start()) {
            PE_ERROR("Failed to start LinguaCore service");
            return 1;
        }

        // Create server wrapper (reuse the initialized core)
        auto server = std::make_unique<linguacore::LinguaServer>(hStdout, core);

        PE_INFO("========================================");
        PE_INFO("LinguaServer ready, entering message loop...");
        PE_INFO("Press Ctrl+C to stop or send SIZE_MAX via pipe.");
        PE_INFO("========================================");

        // Message loop - read commands from parent process
        std::vector<uint8_t> receiveBuffer;

        while (!g_shutdown_requested) {
            size_t size;
            if (!ReadFromPipe(hStdin, reinterpret_cast<uint8_t*>(&size),
                sizeof(size))) {
                break;
            }
            if (size == MAXSIZE_T) {
                //PS_INFO("finish")
                break;
            }
            receiveBuffer.resize(size);
            if (!ReadFromPipe(hStdin, receiveBuffer.data(), size)) {
                break;
            }
            // Process command
            server->NotifyReceiveData(receiveBuffer);
        }

        PE_INFO("Message loop ended, cleaning up...");

        // Graceful shutdown
        PE_INFO("Stopping LinguaCore service...");
        core->stop();

        PE_INFO("--- Final Statistics ---");
        PE_INFO(core->getStatistics());
        PE_INFO("------------------------");

        // Cleanup
        server.reset();

        // Wait for all task queues to finish
        pe_base::WaitAllTaskQueueExit();

        PE_INFO("========================================");
        PE_INFO("LinguaCoreServer subprocess exiting normally");
        PE_INFO("========================================");

        return 0;

    }
    catch (const std::exception& e) {
        PE_ERROR("FATAL ERROR: " << e.what());
        return 1;
    }
    catch (...) {
        PE_ERROR("FATAL ERROR: Unknown exception");
        return 99;
    }
}
