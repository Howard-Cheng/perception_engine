#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "WindowsService.h"
#include "HttpServer.h"
#include "ContextCollector.h"

class PerceptionEngineService : public WindowsService {
private:
    std::unique_ptr<HttpServer> httpServer;
    std::unique_ptr<ContextCollector> contextCollector;
    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> serviceRunning{false};
    
public:
    PerceptionEngineService() 
        : WindowsService("PerceptionEngine", "Perception Engine Service") {
    }
    
    void OnStart() override {
        try {
            LogMessage("[DEBUG] Starting PerceptionEngineService...");
            
            // Initialize context collector
            contextCollector = std::make_unique<ContextCollector>();
            contextCollector->StartPeriodicUpdate();
            LogMessage("[DEBUG] Context collector started");
            
            // Initialize HTTP server
            httpServer = std::make_unique<HttpServer>(8777);
            LogMessage("[DEBUG] HTTP server created on port 8777");
            
            // Set request handler
            httpServer->SetRequestHandler([this](const HttpRequest& request, HttpResponse& response) {
                HandleContextRequest(request, response);
            });
            LogMessage("[DEBUG] Request handler set");
            
            // Start HTTP server in a separate thread for service mode
            serviceRunning = true;
            serverThread = std::make_unique<std::thread>([this]() {
                RunHttpServer();
            });
            
            LogMessage("[SUCCESS] HTTP server thread started successfully");
            LogMessage("[INFO] Server accessible at: http://localhost:8777/context");
        }
        catch (const std::exception& e) {
            LogMessage("[ERROR] Service start error: " + std::string(e.what()));
            // Log error to Windows Event Log
            OutputDebugStringA(("Service start error: " + std::string(e.what())).c_str());
            throw;
        }
    }
    
    void OnStop() override {
        try {
            LogMessage("[DEBUG] Stopping PerceptionEngineService...");
            
            // Signal service to stop
            serviceRunning = false;
            
            if (httpServer) {
                httpServer->Stop();
                LogMessage("[DEBUG] HTTP server stop signal sent");
            }
            
            // Wait for server thread to finish
            if (serverThread && serverThread->joinable()) {
                serverThread->join();
                LogMessage("[DEBUG] HTTP server thread joined");
            }
            
            if (contextCollector) {
                contextCollector->StopPeriodicUpdate();
                contextCollector.reset();
                LogMessage("[DEBUG] Context collector stopped");
            }
            
            httpServer.reset();
            serverThread.reset();
            
            LogMessage("[SUCCESS] Service stopped successfully");
        }
        catch (...) {
            LogMessage("[WARNING] Error during shutdown (ignored)");
            // Ignore errors during shutdown
        }
    }
    
    void OnRunning() override {
        // For service mode, just check if everything is still running
        if (serviceRunning && httpServer) {
            // Service is running properly, just sleep a bit
            Sleep(1000);
        } else {
            // Something went wrong, signal service to stop
            SetRunning(false);
        }
    }
    
private:
    void RunHttpServer() {
        try {
            LogMessage("[DEBUG] Starting HTTP server in service thread...");
            
            if (!httpServer->Start()) {
                LogMessage("[ERROR] Failed to start HTTP server in service mode!");
                serviceRunning = false;
                return;
            }
            
            LogMessage("[SUCCESS] HTTP server started successfully on port 8777");
            LogMessage("[INFO] Server is now listening on: http://localhost:8777");
            LogMessage("[INFO] API endpoint: http://localhost:8777/context");
            
            // Run the server loop
            httpServer->Run();
            
            LogMessage("[DEBUG] HTTP server loop ended");
        }
        catch (const std::exception& e) {
            LogMessage("[ERROR] HTTP server thread exception: " + std::string(e.what()));
            serviceRunning = false;
        }
    }
    
    void HandleContextRequest(const HttpRequest& request, HttpResponse& response) {
        try {
            LogMessage("[DEBUG] Handling request: " + request.method + " " + request.path);
            
            if (request.path == "/context" && request.method == "GET") {
                if (contextCollector) {
                    Json context = contextCollector->CollectCurrentContext();
                    response.SetHeader("Content-Type", "application/json");
                    response.SetBody(context.toString());
                    response.status = 200;
                    LogMessage("[DEBUG] Returned context data successfully");
                } else {
                    response.SetBody("{\"error\":\"Service not initialized\"}");
                    response.status = 500;
                    LogMessage("[ERROR] Context collector not initialized");
                }
            } else {
                response.SetBody("{\"error\":\"Not found\"}");
                response.status = 404;
                LogMessage("[DEBUG] Path not found: " + request.path);
            }
        }
        catch (const std::exception& e) {
            response.SetBody("{\"error\":\"Internal server error\"}");
            response.status = 500;
            LogMessage("[ERROR] Exception in request handler: " + std::string(e.what()));
        }
    }
    
    void LogMessage(const std::string& message) {
        // For console mode, print to stdout
        std::cout << message << std::endl;
        
        // For service mode, log to Windows Event Log and Debug Output
        OutputDebugStringA((message + "\n").c_str());
        
        // Could also add Windows Event Log here if needed
        // WriteToEventLog(message);
    }
};

int main(int argc, char* argv[]) {
    std::cout << "Perception Engine v1.0" << std::endl;
    std::cout << "======================" << std::endl;
    
    // Parse command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        
        PerceptionEngineService service;
        
        if (arg == "--install") {
            std::cout << "Installing Windows service..." << std::endl;
            if (service.Install()) {
                std::cout << "Service installed successfully." << std::endl;
                return 0;
            } else {
                std::cout << "Failed to install service. Run as administrator." << std::endl;
                return 1;
            }
        }
        else if (arg == "--uninstall") {
            std::cout << "Uninstalling Windows service..." << std::endl;
            if (service.Uninstall()) {
                std::cout << "Service uninstalled successfully." << std::endl;
                return 0;
            } else {
                std::cout << "Failed to uninstall service. Run as administrator." << std::endl;
                return 1;
            }
        }
        else if (arg == "--start") {
            std::cout << "Starting Windows service..." << std::endl;
            if (service.Start()) {
                std::cout << "Service started successfully." << std::endl;
                return 0;
            } else {
                std::cout << "Failed to start service." << std::endl;
                return 1;
            }
        }
        else if (arg == "--stop") {
            std::cout << "Stopping Windows service..." << std::endl;
            if (service.Stop()) {
                std::cout << "Service stopped successfully." << std::endl;
                return 0;
            } else {
                std::cout << "Failed to stop service." << std::endl;
                return 1;
            }
        }
        else if (arg == "--console") {
            // Run as console application for testing
            std::cout << "Running Perception Engine as console application..." << std::endl;
            std::cout << "Press Ctrl+C to stop." << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            try {
                // Create separate instances for console mode
                HttpServer server(8777);
                ContextCollector collector;
                
                std::cout << "[DEBUG] Starting context collector..." << std::endl;
                collector.StartPeriodicUpdate();
                
                std::cout << "[DEBUG] Setting up request handler..." << std::endl;
                server.SetRequestHandler([&collector](const HttpRequest& request, HttpResponse& response) {
                    std::cout << "[DEBUG] Received request: " << request.method << " " << request.path << std::endl;
                    
                    if (request.path == "/context" && request.method == "GET") {
                        Json context = collector.CollectCurrentContext();
                        response.SetHeader("Content-Type", "application/json");
                        response.SetBody(context.toString());
                        response.status = 200;
                        std::cout << "[DEBUG] Sent context response" << std::endl;
                    } else {
                        response.SetBody("{\"error\":\"Not found\"}");
                        response.status = 404;
                        std::cout << "[DEBUG] Sent 404 response for: " << request.path << std::endl;
                    }
                });
                
                std::cout << "[DEBUG] Starting HTTP server on port 8777..." << std::endl;
                if (!server.Start()) {
                    std::cout << "[ERROR] Failed to start HTTP server!" << std::endl;
                    std::cout << "Possible causes:" << std::endl;
                    std::cout << "1. Port 8777 is already in use" << std::endl;
                    std::cout << "2. Insufficient permissions" << std::endl;
                    std::cout << "3. Firewall blocking the connection" << std::endl;
                    return 1;
                }
                
                std::cout << "[SUCCESS] HTTP server started successfully!" << std::endl;
                std::cout << "[INFO] Server is now listening on: http://localhost:8777" << std::endl;
                std::cout << "[INFO] API endpoint: http://localhost:8777/context" << std::endl;
                std::cout << "[INFO] Test with: curl http://localhost:8777/context" << std::endl;
                std::cout << std::string(50, '-') << std::endl;
                
                std::cout << "Starting server loop (blocking)..." << std::endl;
                server.Run(); // Blocking call
                
                std::cout << "[DEBUG] Server loop ended, cleaning up..." << std::endl;
                collector.StopPeriodicUpdate();
            }
            catch (const std::exception& e) {
                std::cout << "[ERROR] Exception: " << e.what() << std::endl;
                return 1;
            }
            
            return 0;
        }
        else {
            std::cout << "Usage: " << argv[0] << " [--install|--uninstall|--start|--stop|--console]" << std::endl;
            return 1;
        }
    }
    
    // If no arguments, run as Windows service
    std::cout << "Starting as Windows service..." << std::endl;
    try {
        PerceptionEngineService service;
        WindowsService::RunAsService(&service);
    }
    catch (...) {
        std::cout << "[ERROR] Failed to start as Windows service" << std::endl;
        return 1;
    }
    
    return 0;
}
