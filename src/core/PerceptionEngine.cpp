#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include "pe_base/logger.h"  // NEW: Add Logger first
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <sstream>  // NEW: For std::istringstream (ISO time parsing)
#include <iomanip>  // NEW: For std::get_time (ISO time parsing)
#include <nlohmann/json.hpp>  // NEW: Add nlohmann::json
#include "core/WindowsService.h"
#include "communication/HttpServer.h"
#include "context/ContextCollector.h"  // UPDATED: Use context folder
#include "pe_base/config_manager.h"      // NEW: Add pe_base::ConfigManager

// #include "CameraVisionEngine.h"  // Removed - using Python client instead

// ========================================
// NEW: URL Decode Helper Function
// ========================================
// Helper: URL decode (convert %XX to actual characters for UTF-8 support)
std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    char ch;
    size_t i = 0;

    while (i < encoded.length()) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            // Convert %XX to character
            int value;
            if (sscanf_s(encoded.substr(i + 1, 2).c_str(), "%x", &value) == 1) {
                ch = static_cast<char>(value);
                decoded += ch;
                i += 3;
            }
            else {
                decoded += encoded[i];
                i++;
            }
        }
        else if (encoded[i] == '+') {
            decoded += ' ';
            i++;
        }
        else {
            decoded += encoded[i];
            i++;
        }
    }

    return decoded;
}

class PerceptionEngineService : public WindowsService {
private:
    std::unique_ptr<HttpServer> httpServer;
    std::unique_ptr<ContextCollector> contextCollector;
    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> serviceRunning{ false };
    bool screenOnlyMode;

public:
    PerceptionEngineService(bool screenOnly = false)
        : WindowsService("PerceptionEngine", "Perception Engine Service"),
        screenOnlyMode(screenOnly) {
    }

    void OnStart() override {
        try {
            PE_INFO("Starting PerceptionEngineService...");
            if (screenOnlyMode) {
                PE_INFO("Mode: Screen-Only (audio/camera disabled)");
            }
            else {
                PE_INFO("Mode: Full (screen + audio + camera)");
            }

            // Initialize context collector
            contextCollector = std::make_unique<ContextCollector>();  // UPDATED
            PE_INFO("Context collector started");

            // NEW: Initialize PostgreSQL (optional feature)
            // If PostgreSQL is not available, system continues without it
            if (contextCollector->InitializeDatabase("host=127.0.0.1 port=5432 dbname=perception_engine user=postgres", "perception_context")) {  // UPDATED: Use PostgreSQL connection string
                PE_INFO("? PostgreSQL initialized - auto storage every 5 seconds");
            }
            else {
                PE_WARN("??  PostgreSQL not available - running without database storage");
                PE_INFO("   To enable PostgreSQL: Install and start PostgreSQL on 127.0.0.1:5432");
            }

            // Initialize HTTP server
            httpServer = std::make_unique<HttpServer>(8777);
            PE_INFO("HTTP server created on port 8777");

            // Set request handler
            httpServer->SetRequestHandler([this](const HttpRequest& request, HttpResponse& response) {
                HandleContextRequest(request, response);
                });
            PE_INFO("Request handler set");

            // Start HTTP server in a separate thread for service mode
            serviceRunning = true;
            serverThread = std::make_unique<std::thread>([this]() {
                RunHttpServer();
                });

            PE_INFO("HTTP server thread started successfully");
            PE_INFO("Server accessible at: http://localhost:8777/context");
        }
        catch (const std::exception& e) {
            PE_ERROR_THIS("Service start error:" << e.what())
                throw;
        }
    }

    void OnStop() override {
        try {
            PE_INFO("Stopping PerceptionEngineService...");

            // Signal service to stop
            serviceRunning = false;

            if (httpServer) {
                httpServer->Stop();
                PE_INFO("HTTP server stop signal sent");
            }

            // Wait for server thread to finish
            if (serverThread && serverThread->joinable()) {
                serverThread->join();
                PE_INFO("HTTP server thread joined");
            }

            if (contextCollector) {
                contextCollector->StopPeriodicUpdate();
                contextCollector.reset();
                PE_INFO("Context collector stopped");
            }

            httpServer.reset();
            serverThread.reset();

            PE_INFO("Service stopped successfully");
        }
        catch (...) {
            PE_WARN("Error during shutdown (ignored)");
        }
    }

    void OnRunning() override {
        // For service mode, just check if everything is still running
        if (serviceRunning && httpServer) {
            // Service is running properly, just sleep a bit
            Sleep(1000);
        }
        else {
            // Something went wrong, signal service to stop
            SetRunning(false);
        }
    }

private:
    void RunHttpServer() {
        try {
            PE_INFO("Starting HTTP server in service thread...");

            if (!httpServer->Start()) {
                PE_ERROR("Failed to start HTTP server in service mode!");
                serviceRunning = false;
                return;
            }

            PE_INFO("HTTP server started successfully on port 8777");
            PE_INFO("Server is now listening on: http://localhost:8777");
            PE_INFO("API endpoint: http://localhost:8777/context");

            // Run the server loop
            httpServer->Run();

            PE_INFO("HTTP server loop ended");
        }
        catch (const std::exception& e) {
            PE_ERROR_THIS("HTTP server thread exception:" << e.what())
                serviceRunning = false;
        }
    }

    std::string LoadDashboardHTML() {
        std::ifstream file("dashboard.html");
        if (!file.is_open()) {
            return "<html><body><h1>Error: dashboard.html not found</h1></body></html>";
        }
        std::string content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        return content;
    }

    void HandleContextRequest(const HttpRequest& request, HttpResponse& response) {
        try {
            PE_DEBUG_THIS("Handling request:" << request.method.c_str() << request.path.c_str());

            if (request.path == "/context" && request.method == "GET") {
                if (contextCollector) {
                    nlohmann::json context = contextCollector->CollectCurrentContext();
                    response.SetHeader("Content-Type", "application/json");
                    response.SetBody(context.dump());  // Changed from .toString() to .dump()
                    response.status = 200;
                    PE_DEBUG("Returned context data successfully");
                }
                else {
                    response.SetBody("{\"error\":\"Service not initialized\"}");
                    response.status = 500;
                    PE_ERROR("Context collector not initialized");
                }
            }
            // ? NEW: Elasticsearch/VectorDB query endpoint
            else if (request.path.find("/query") == 0 && request.method == "GET") {
                if (!contextCollector) {
                    response.SetBody("{\"error\":\"Service not initialized\"}");
                    response.status = 500;
                    return;
                }

                if (!contextCollector->IsElasticsearchAvailable()) {
                    response.SetBody("{\"error\":\"Database not available\"}");
                    response.status = 503;
                    return;
                }

                try {
                    // Parse query parameters - FIX: Support new time format (starttime/endtime ISO format)
                    std::string keyword;
                    std::time_t startTime = 0;
                    std::time_t endTime = std::time(nullptr);  // Default: now
                    int maxResults = 100;
                    int requestType = 2;  // NEW: Default to VectorDB (type 2)

                    // Simple query parameter parsing
                    size_t queryPos = request.path.find('?');
                    if (queryPos != std::string::npos) {
                        std::string queryString = request.path.substr(queryPos + 1);

                        // Parse keyword - Apply URL decoding for Chinese/UTF-8 support
                        size_t keywordPos = queryString.find("keyword=");
                        if (keywordPos != std::string::npos) {
                            size_t keywordEnd = queryString.find('&', keywordPos);
                            std::string encodedKeyword;
                            if (keywordEnd == std::string::npos) {
                                encodedKeyword = queryString.substr(keywordPos + 8);
                            }
                            else {
                                encodedKeyword = queryString.substr(keywordPos + 8, keywordEnd - keywordPos - 8);
                            }
                            // URL decode the keyword to support Chinese characters
                            keyword = urlDecode(encodedKeyword);
                            PE_INFO("Decoded keyword: '" + keyword + "'");
                        }

                        // Parse starttime (ISO 8601 format: 2025-12-02T15)
                        size_t startTimePos = queryString.find("starttime=");
                        if (startTimePos != std::string::npos) {
                            size_t startTimeEnd = queryString.find('&', startTimePos);
                            std::string startTimeStr;
                            if (startTimeEnd == std::string::npos) {
                                startTimeStr = queryString.substr(startTimePos + 10);
                            }
                            else {
                                startTimeStr = queryString.substr(startTimePos + 10, startTimeEnd - startTimePos - 10);
                            }
                            // URL decode
                            startTimeStr = urlDecode(startTimeStr);
                            
                            // Parse ISO 8601 format
                            std::tm tm_start = {};
                            std::istringstream ss_start(startTimeStr);
                            ss_start >> std::get_time(&tm_start, "%Y-%m-%dT%H");
                            if (!ss_start.fail()) {
                                startTime = std::mktime(&tm_start);
                            }
                            PE_INFO("Parsed starttime: '" + startTimeStr + "' -> " + std::to_string(startTime));
                        }

                        // Parse endtime (ISO 8601 format: 2025-12-03T15)
                        size_t endTimePos = queryString.find("endtime=");
                        if (endTimePos != std::string::npos) {
                            size_t endTimeEnd = queryString.find('&', endTimePos);
                            std::string endTimeStr;
                            if (endTimeEnd == std::string::npos) {
                                endTimeStr = queryString.substr(endTimePos + 8);
                            }
                            else {
                                endTimeStr = queryString.substr(endTimePos + 8, endTimeEnd - endTimePos - 8);
                            }
                            // URL decode
                            endTimeStr = urlDecode(endTimeStr);
                            
                            // Parse ISO 8601 format
                            std::tm tm_end = {};
                            std::istringstream ss_end(endTimeStr);
                            ss_end >> std::get_time(&tm_end, "%Y-%m-%dT%H");
                            if (!ss_end.fail()) {
                                endTime = std::mktime(&tm_end);
                            }
                            PE_INFO("Parsed endtime: '" + endTimeStr + "' -> " + std::to_string(endTime));
                        }

                        // Parse maxResults
                        size_t maxPos = queryString.find("max=");
                        if (maxPos != std::string::npos) {
                            std::string maxStr = queryString.substr(maxPos + 4);
                            size_t maxEnd = maxStr.find('&');
                            if (maxEnd != std::string::npos) {
                                maxStr = maxStr.substr(0, maxEnd);
                            }
                            maxResults = std::stoi(maxStr);
                        }
                        
                        // NEW: Parse requesttype parameter
                        size_t typePos = queryString.find("requesttype=");
                        if (typePos != std::string::npos) {
                            std::string typeStr = queryString.substr(typePos + 12);
                            size_t typeEnd = typeStr.find('&');
                            if (typeEnd != std::string::npos) {
                                typeStr = typeStr.substr(0, typeEnd);
                            }
                            requestType = std::stoi(typeStr);
                            PE_INFO("Request type: " + std::to_string(requestType));
                        }
                        
                        // Fallback: Support old 'hours' parameter for backward compatibility
                        if (startTime == 0) {
                            size_t hoursPos = queryString.find("hours=");
                            if (hoursPos != std::string::npos) {
                                size_t hoursEnd = queryString.find('&', hoursPos);
                                std::string hoursStr;
                                if (hoursEnd == std::string::npos) {
                                    hoursStr = queryString.substr(hoursPos + 6);
                                }
                                else {
                                    hoursStr = queryString.substr(hoursPos + 6, hoursEnd - hoursPos - 6);
                                }
                                int hours = std::stoi(hoursStr);
                                startTime = endTime - (hours * 3600);
                                PE_INFO("Using legacy 'hours' parameter: " + std::to_string(hours));
                            }
                        }
                    }

                    // NEW: Query based on requesttype
                    nlohmann::json results;  // Changed from pe_base::Json
                    if (requestType == 1) {
                        // Type 1: Query PostgreSQL/Elasticsearch raw events
                        PE_INFO("Querying PostgreSQL/ES raw events (requesttype=1)");
                        results = contextCollector->GetESDBData(keyword, startTime, endTime, maxResults);
                    }
                    else if (requestType == 2) {
                        // Type 2: Query VectorDB (Qdrant) session summaries
                        PE_INFO("Querying VectorDB session summaries (requesttype=2)");
                        results = contextCollector->GetVectorDBData(keyword, startTime, endTime, maxResults);
                    }
                    else {
                        // Invalid request type
                        response.SetBody("{\"error\":\"Invalid requesttype. Use 1 for raw events or 2 for session summaries.\"}");
                        response.status = 400;
                        PE_ERROR("Invalid requesttype: " + std::to_string(requestType));
                        return;
                    }

                    response.SetHeader("Content-Type", "application/json");
                    response.SetBody(results.dump());  // Changed from .toString() to .dump()
                    response.status = 200;

                    PE_INFO("Database Query: keyword='" + keyword + "' startTime=" + std::to_string(startTime) + " endTime=" + std::to_string(endTime) + " requestType=" + std::to_string(requestType));

                }
                catch (const std::exception& e) {
                    response.SetBody("{\"error\":\"Query failed: " + std::string(e.what()) + "\"}");
                    response.status = 500;
                    PE_ERROR("Database query failed: " + std::string(e.what()));
                }
            }
            else if (request.path == "/dashboard" && request.method == "GET") {
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                PE_DEBUG("Served dashboard HTML");
            }
            else if (request.path == "/" && request.method == "GET") {
                // Redirect root to dashboard
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                PE_DEBUG("Served dashboard HTML from root");
            }
            else {
                response.SetBody("{\"error\":\"Not found\"}");
                response.status = 404;
                PE_DEBUG_THIS("Path not found:" << request.path.c_str())
            }
        }
        catch (const std::exception& e) {
            response.SetBody("{\"error\":\"Internal server error\"}");
            response.status = 500;
            PE_ERROR_THIS("Exception in request handler:" << e.what())
        }
    }
};

std::string GetExePath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exePathStr(exePath);
    size_t lastSlash = exePathStr.find_last_of("\\/");
    std::string exe_dir = exePathStr.substr(0, lastSlash);
    return exe_dir;
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
        (log_path / "PerceptionEngine").generic_string());

    PE_INFO("=====================================");
    PE_INFO("Perception Engine v1.0");
    PE_INFO("=====================================");

    // =========================================
    // Load Configuration
    // =========================================
    PE_INFO("Loading configuration from config.ini...");
    std::string config_path = GetExePath() + "\\config.ini";
    if (!pe_base::ConfigManager::GetInstance().LoadConfig(config_path)) {
        PE_WARN("Failed to load config.ini, using default values");
        PE_WARN(std::string("Error: ") + pe_base::ConfigManager::GetInstance().GetLastError());
    }
    else {
        PE_INFO("Configuration loaded successfully");
    }

    // Validate configuration
    if (!pe_base::ConfigManager::GetInstance().ValidateConfiguration()) {
        PE_ERROR("Configuration validation failed:");
        PE_ERROR(pe_base::ConfigManager::GetInstance().GetLastError());
        PE_WARN("Continuing with best-effort configuration...");
    }
    else {
        PE_INFO("Configuration validated successfully");
    }

    // Parse command line arguments
    bool screenOnlyMode = false;
    std::string primaryCommand = "";

    // Check for --screen-only flag in any position
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--screen-only") {
            screenOnlyMode = true;
            PE_INFO("Screen-only mode enabled (audio and camera disabled)");
        }
        else if (primaryCommand.empty()) {
            primaryCommand = arg;
        }
    }

    if (!primaryCommand.empty()) {
        PerceptionEngineService service(screenOnlyMode);

        if (primaryCommand == "--install") {
            PE_INFO("Installing Windows service...");
            if (service.Install()) {
                PE_INFO("Service installed successfully.");
                return 0;
            }
            else {
                PE_ERROR("Failed to install service. Run as administrator.");
                return 1;
            }
        }
        else if (primaryCommand == "--uninstall") {
            PE_INFO("Uninstalling Windows service...");
            if (service.Uninstall()) {
                PE_INFO("Service uninstalled successfully.");
                return 0;
            }
            else {
                PE_ERROR("Failed to uninstall service. Run as administrator.");
                return 1;
            }
        }
        else if (primaryCommand == "--start") {
            PE_INFO("Starting Windows service...");
            if (service.Start()) {
                PE_INFO("Service started successfully.");
                return 0;
            }
            else {
                PE_ERROR("Failed to start service.");
                return 1;
            }
        }
        else if (primaryCommand == "--stop") {
            PE_INFO("Stopping Windows service...");
            if (service.Stop()) {
                PE_INFO("Service stopped successfully.");
                return 0;
            }
            else {
                PE_ERROR("Failed to stop service.");
                return 1;
            }
        }
        else if (primaryCommand == "--console") {
            // Run as console application for testing
            PE_INFO("Running Perception Engine as console application...");
            if (screenOnlyMode) {
                PE_INFO("Mode: Screen-Only (lightweight - audio/camera disabled)");
            }
            else {
                PE_INFO("Mode: Full (screen + audio + camera)");
            }
            PE_INFO("Press Ctrl+C to stop.");
            PE_INFO("-----------------------------------------------------");

            try {
                // Create separate instances for console mode
                HttpServer server(8777);

                PE_INFO("Starting context collector...");

                // UPDATED: Use ContextCollector directly
                ContextCollector collector;

                // NEW: Initialize PostgreSQL (optional feature)
                if (collector.InitializeDatabase("host=127.0.0.1 port=5432 dbname=perception_engine user=postgres", "perception_context")) {  // UPDATED: Use PostgreSQL connection string
                    PE_INFO("? PostgreSQL initialized - auto storage every 5 seconds");
                }
                else {
                    PE_WARN("??  PostgreSQL not available - running without database storage");
                }

                PE_INFO("Setting up request handler...");
                server.SetRequestHandler([&collector](const HttpRequest& request, HttpResponse& response) {
                    PE_DEBUG("Received request:" << request.method.c_str() << request.path.c_str())

                    if (request.path == "/context" && request.method == "GET") {
                        nlohmann::json context = collector.CollectCurrentContext();  // Changed from pe_base::Json
                        response.SetHeader("Content-Type", "application/json");
                        response.SetBody(context.dump());  // Changed from .toString() to .dump()
                        response.status = 200;
                        PE_DEBUG("Sent context response");
                    }
                    // ? NEW: Elasticsearch query endpoint
                    else if (request.path.find("/query") == 0 && request.method == "GET") {
                        if (!collector.IsElasticsearchAvailable()) {
                            response.SetBody("{\"error\":\"Database not available\"}");
                            response.status = 503;
                            return;
                        }

                        try {
                            // Parse query parameters - FIX: Support new time format (starttime/endtime ISO format)
                            std::string keyword;
                            std::time_t startTime = 0;
                            std::time_t endTime = std::time(nullptr);  // Default: now
                            int maxResults = 100;
                            int requestType = 2;  // NEW: Default to VectorDB (type 2)

                            size_t queryPos = request.path.find('?');
                            if (queryPos != std::string::npos) {
                                std::string queryString = request.path.substr(queryPos + 1);

                                // Parse keyword - FIX: Apply URL decoding for Chinese/UTF-8 support
                                size_t keywordPos = queryString.find("keyword=");
                                if (keywordPos != std::string::npos) {
                                    size_t keywordEnd = queryString.find('&', keywordPos);
                                    std::string encodedKeyword;
                                    if (keywordEnd == std::string::npos) {
                                        encodedKeyword = queryString.substr(keywordPos + 8);
                                    }
                                    else {
                                        encodedKeyword = queryString.substr(keywordPos + 8, keywordEnd - keywordPos - 8);
                                    }
                                    // FIX: URL decode the keyword to support Chinese characters
                                    keyword = urlDecode(encodedKeyword);
                                    PE_INFO("Decoded keyword: '" + keyword + "'");
                                }

                                // Parse starttime (ISO 8601 format: 2025-12-02T15)
                                size_t startTimePos = queryString.find("starttime=");
                                if (startTimePos != std::string::npos) {
                                    size_t startTimeEnd = queryString.find('&', startTimePos);
                                    std::string startTimeStr;
                                    if (startTimeEnd == std::string::npos) {
                                        startTimeStr = queryString.substr(startTimePos + 10);
                                    }
                                    else {
                                        startTimeStr = queryString.substr(startTimePos + 10, startTimeEnd - startTimePos - 10);
                                    }
                                    // URL decode
                                    startTimeStr = urlDecode(startTimeStr);
                                    
                                    // Parse ISO 8601 format
                                    std::tm tm_start = {};
                                    std::istringstream ss_start(startTimeStr);
                                    ss_start >> std::get_time(&tm_start, "%Y-%m-%dT%H");
                                    if (!ss_start.fail()) {
                                        startTime = std::mktime(&tm_start);
                                    }
                                    PE_INFO("Parsed starttime: '" + startTimeStr + "' -> " + std::to_string(startTime));
                                }

                                // Parse endtime (ISO 8601 format: 2025-12-03T15)
                                size_t endTimePos = queryString.find("endtime=");
                                if (endTimePos != std::string::npos) {
                                    size_t endTimeEnd = queryString.find('&', endTimePos);
                                    std::string endTimeStr;
                                    if (endTimeEnd == std::string::npos) {
                                        endTimeStr = queryString.substr(endTimePos + 8);
                                    }
                                    else {
                                        endTimeStr = queryString.substr(endTimePos + 8, endTimeEnd - endTimePos - 8);
                                    }
                                    // URL decode
                                    endTimeStr = urlDecode(endTimeStr);
                                    
                                    // Parse ISO 8601 format
                                    std::tm tm_end = {};
                                    std::istringstream ss_end(endTimeStr);
                                    ss_end >> std::get_time(&tm_end, "%Y-%m-%dT%H");
                                    if (!ss_end.fail()) {
                                        endTime = std::mktime(&tm_end);
                                    }
                                    PE_INFO("Parsed endtime: '" + endTimeStr + "' -> " + std::to_string(endTime));
                                }

                                // Parse maxResults
                                size_t maxPos = queryString.find("max=");
                                if (maxPos != std::string::npos) {
                                    std::string maxStr = queryString.substr(maxPos + 4);
                                    size_t maxEnd = maxStr.find('&');
                                    if (maxEnd != std::string::npos) {
                                        maxStr = maxStr.substr(0, maxEnd);
                                    }
                                    maxResults = std::stoi(maxStr);
                                }
                                
                                // NEW: Parse requesttype parameter
                                size_t typePos = queryString.find("requesttype=");
                                if (typePos != std::string::npos) {
                                    std::string typeStr = queryString.substr(typePos + 12);
                                    size_t typeEnd = typeStr.find('&');
                                    if (typeEnd != std::string::npos) {
                                        typeStr = typeStr.substr(0, typeEnd);
                                    }
                                    requestType = std::stoi(typeStr);
                                    PE_INFO("Request type: " + std::to_string(requestType));
                                }
                                
                                // Fallback: Support old 'hours' parameter for backward compatibility
                                if (startTime == 0) {
                                    size_t hoursPos = queryString.find("hours=");
                                    if (hoursPos != std::string::npos) {
                                        size_t hoursEnd = queryString.find('&', hoursPos);
                                        std::string hoursStr;
                                        if (hoursEnd == std::string::npos) {
                                            hoursStr = queryString.substr(hoursPos + 6);
                                        }
                                        else {
                                            hoursStr = queryString.substr(hoursPos + 6, hoursEnd - hoursPos - 6);
                                        }
                                        int hours = std::stoi(hoursStr);
                                        startTime = endTime - (hours * 3600);
                                        PE_INFO("Using legacy 'hours' parameter: " + std::to_string(hours));
                                    }
                                }
                            }

                            // NEW: Query based on requesttype
                            nlohmann::json results;  // Changed from pe_base::Json
                            if (requestType == 1) {
                                // Type 1: Query PostgreSQL/Elasticsearch raw events
                                PE_INFO("Querying PostgreSQL/ES raw events (requesttype=1)");
                                results = collector.GetESDBData(keyword, startTime, endTime, maxResults);
                            }
                            else if (requestType == 2) {
                                // Type 2: Query VectorDB (Qdrant) session summaries
                                PE_INFO("Querying VectorDB session summaries (requesttype=2)");
                                results = collector.GetVectorDBData(keyword, startTime, endTime, maxResults);
                            }
                            else {
                                // Invalid request type
                                response.SetBody("{\"error\":\"Invalid requesttype. Use 1 for raw events or 2 for session summaries.\"}");
                                response.status = 400;
                                PE_ERROR("Invalid requesttype: " + std::to_string(requestType));
                                return;
                            }

                            response.SetHeader("Content-Type", "application/json");
                            response.SetBody(results.dump());  // Changed from .toString() to .dump()
                            response.status = 200;

                            PE_INFO("Database Query: keyword='" + keyword + "' startTime=" + std::to_string(startTime) + " endTime=" + std::to_string(endTime) + " requestType=" + std::to_string(requestType));

                        }
                        catch (const std::exception& e) {
                            response.SetBody("{\"error\":\"Query failed: " + std::string(e.what()) + "\"}");
                            response.status = 500;
                            PE_ERROR("Database query failed: " + std::string(e.what()));
                        }
                    }
                    else if (request.path == "/dashboard" || request.path == "/" && request.method == "GET") {
                        std::ifstream file("dashboard.html");
                        if (file.is_open()) {
                            std::string html((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
                            response.SetHeader("Content-Type", "text/html; charset=utf-8");
                            response.SetBody(html);
                            response.status = 200;
                            PE_DEBUG("Served dashboard HTML");
                        }
                        else {
                            response.SetBody("<html><body><h1>Error: dashboard.html not found</h1></body></html>");
                            response.SetHeader("Content-Type", "text/html");
                            response.status = 500;
                            PE_ERROR("dashboard.html not found");
                        }
                    }
                    else if (request.path == "/update_context" && request.method == "POST") {
                        try {
                            std::string body = request.body;
                            PE_DEBUG("POST body:" << body.c_str())

                            // Extract device type
                            size_t devicePos = body.find("\"device\"");
                            if (devicePos == std::string::npos) {
                                PE_ERROR("Missing device field in body");
                                response.SetBody("{\"error\":\"Missing device field\"}");
                                response.status = 400;
                            }
                            else {
                                size_t deviceStart = body.find("\"", devicePos + 9);
                                size_t deviceEnd = body.find("\"", deviceStart + 1);
                                std::string device = body.substr(deviceStart + 1, deviceEnd - deviceStart - 1);

                                if (device == "Camera") {
                                    std::string caption;
                                    size_t objectsPos = body.find("\"objects\"");
                                    if (objectsPos != std::wstring::npos) {
                                        size_t captionStart = body.find("\"", objectsPos + 12);
                                        size_t captionEnd = body.find("\"", captionStart + 1);
                                        if (captionStart != std::string::npos && captionEnd != std::string::npos) {
                                            caption = body.substr(captionStart + 1, captionEnd - captionStart - 1);
                                        }
                                    }

                                    collector.UpdateCameraContext(caption, 0.0f);
                                    PE_INFO("Camera update:" << caption.c_str())

                                    response.SetHeader("Content-Type", "application/json");
                                    response.SetBody("{\"status\":\"ok\"}");
                                    response.status = 200;
                                }
                                else {
                                    response.SetBody("{\"error\":\"Unknown device type\"}");
                                    response.status = 400;
                                }
                            }
                        }
                        catch (const std::exception& e) {
                            PE_ERROR("Failed to parse update_context:" << e.what())
                            response.SetBody("{\"error\":\"Invalid JSON\"}");
                            response.status = 400;
                        }
                    }
                    else {
                        response.SetBody("{\"error\":\"Not found\"}");
                        response.status = 404;
                        PE_ERROR("Sent 404 response for:" << request.path.c_str());
                    }
                    });

                PE_INFO("Starting HTTP server on port 8777...");
                if (!server.Start()) {
                    PE_ERROR("Failed to start HTTP server!");
                    PE_ERROR("Possible causes:");
                    PE_ERROR("1. Port 8777 is already in use");
                    PE_ERROR("2. Insufficient permissions");
                    PE_ERROR("3. Firewall blocking the connection");
                    return 1;
                }

                PE_INFO("HTTP server started successfully!");
                PE_INFO("Server is now listening on: http://localhost:8777");
                PE_INFO("Dashboard: http://localhost:8777/dashboard");
                PE_INFO("API endpoint: http://localhost:8777/context");
                PE_INFO("-----------------------------------------------------");

                PE_INFO("Starting server loop (blocking)...");
                server.Run(); // Blocking call

                PE_INFO("Server loop ended, cleaning up...");

                collector.StopPeriodicUpdate();
            }
            catch (const std::exception& e) {
                PE_ERROR("Exception:" << e.what())
                return 1;
            }

            PE_INFO("Console mode shutting down normally");
            return 0;
        }
        else {
            PE_ERROR("Unknown argument: %s" << primaryCommand.c_str());
            PE_INFO("Usage: PerceptionEngine.exe [--install|--uninstall|--start|--stop|--console] [--screen-only]");
            PE_INFO("  --console              Run as console application");
            PE_INFO("  --screen-only          Enable screen-only mode (disable audio/camera)");
            PE_INFO("  Example: PerceptionEngine.exe --console --screen-only");
            return 1;
        }
    }

    // If no arguments, run as Windows service
    PE_INFO("Starting as Windows service...");
    if (screenOnlyMode) {
        PE_INFO("Screen-only mode enabled for service");
    }
    try {
        PerceptionEngineService service(screenOnlyMode);
        WindowsService::RunAsService(&service);
    }
    catch (...) {
        PE_ERROR("Failed to start as Windows service");
        return 1;
    }

    PE_INFO("Application exiting normally");
    return 0;
}
