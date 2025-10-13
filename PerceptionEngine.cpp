#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
#include "Logger.h"  // NEW: Add Logger first
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "WindowsService.h"
#include "HttpServer.h"
#include "ContextCollector.h"
#include "AudioCaptureEngine.h"
#include "CameraVisionEngine.h"

class PerceptionEngineService : public WindowsService {
private:
    std::unique_ptr<HttpServer> httpServer;
    std::unique_ptr<ContextCollector> contextCollector;
    std::unique_ptr<AudioCaptureEngine> audioEngine;
    std::unique_ptr<CameraVisionEngine> cameraEngine;
    std::unique_ptr<std::thread> serverThread;
    std::unique_ptr<std::thread> audioPollingThread;
    std::unique_ptr<std::thread> cameraThread;
    std::atomic<bool> serviceRunning{false};
    
public:
    PerceptionEngineService() 
        : WindowsService("PerceptionEngine", "Perception Engine Service") {
    }
    
    void OnStart() override {
        try {
            LOG_INFO("Starting PerceptionEngineService...");

            // Initialize context collector
            contextCollector = std::make_unique<ContextCollector>();
            contextCollector->StartPeriodicUpdate();
            LOG_INFO("Context collector started");

            // Initialize audio capture engine
            audioEngine = std::make_unique<AudioCaptureEngine>();
            if (!audioEngine->Initialize("models/whisper/ggml-tiny.en.bin")) {
                LOG_WARN("Failed to initialize audio engine");
                audioEngine.reset();
            } else {
                LOG_INFO("Audio engine initialized");

                // Set callback to update context when new transcription arrives
                audioEngine->SetTranscriptionCallback([this](const std::string& transcription) {
                    if (contextCollector) {
                        // Get latency from audio engine metrics
                        auto metrics = audioEngine->GetMetrics();
                        contextCollector->UpdateVoiceContext(transcription, metrics.whisperLatencyMs);
                        LOG_INFO_FMT("Voice transcription: %s", transcription.c_str());
                    }
                });

                // Start audio capture
                if (audioEngine->Start()) {
                    LOG_INFO("Audio capture started");

                    // Start polling thread to pull transcriptions
                    audioPollingThread = std::make_unique<std::thread>([this]() {
                        while (serviceRunning.load() && audioEngine) {
                            audioEngine->GetLatestUserSpeech(); // Triggers callback if new result
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    });
                } else {
                    LOG_WARN("Failed to start audio capture");
                }
            }

            // Initialize camera vision engine
            cameraEngine = std::make_unique<CameraVisionEngine>();
            if (!cameraEngine->Initialize("models/fastvlm", 0)) {
                LOG_WARN("Failed to initialize camera engine");
                cameraEngine.reset();
            } else {
                LOG_INFO("Camera vision engine initialized");

                // Start camera processing thread (every 10 seconds)
                cameraThread = std::make_unique<std::thread>([this]() {
                    while (serviceRunning.load() && cameraEngine) {
                        if (cameraEngine->IsReady()) {
                            std::string description = cameraEngine->DescribeScene();
                            if (!description.empty()) {
                                float latency = cameraEngine->GetLastLatencyMs();
                                if (contextCollector) {
                                    contextCollector->UpdateCameraContext(description, latency);
                                    LOG_INFO_FMT("Camera scene: %s (latency: %d ms)", description.c_str(), static_cast<int>(latency));
                                }
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                    }
                });
                LOG_INFO("Camera processing thread started");
            }

            // Initialize HTTP server
            httpServer = std::make_unique<HttpServer>(8777);
            LOG_INFO("HTTP server created on port 8777");

            // Set request handler
            httpServer->SetRequestHandler([this](const HttpRequest& request, HttpResponse& response) {
                HandleContextRequest(request, response);
            });
            LOG_INFO("Request handler set");

            // Start HTTP server in a separate thread for service mode
            serviceRunning = true;
            serverThread = std::make_unique<std::thread>([this]() {
                RunHttpServer();
            });

            LOG_INFO("HTTP server thread started successfully");
            LOG_INFO("Server accessible at: http://localhost:8777/context");
        }
        catch (const std::exception& e) {
            LOG_FATAL_FMT("Service start error: %s", e.what());
            throw;
        }
    }
    
    void OnStop() override {
        try {
            LOG_INFO("Stopping PerceptionEngineService...");

            // Signal service to stop
            serviceRunning = false;

            // Stop audio engine
            if (audioEngine) {
                audioEngine->Stop();
                LOG_INFO("Audio engine stopped");
            }

            // Wait for audio polling thread
            if (audioPollingThread && audioPollingThread->joinable()) {
                audioPollingThread->join();
                LOG_INFO("Audio polling thread joined");
            }

            // Wait for camera thread
            if (cameraThread && cameraThread->joinable()) {
                cameraThread->join();
                LOG_INFO("Camera thread joined");
            }

            // Clean up camera engine
            if (cameraEngine) {
                cameraEngine.reset();
                LOG_INFO("Camera engine stopped");
            }

            if (httpServer) {
                httpServer->Stop();
                LOG_INFO("HTTP server stop signal sent");
            }

            // Wait for server thread to finish
            if (serverThread && serverThread->joinable()) {
                serverThread->join();
                LOG_INFO("HTTP server thread joined");
            }

            if (contextCollector) {
                contextCollector->StopPeriodicUpdate();
                contextCollector.reset();
                LOG_INFO("Context collector stopped");
            }

            audioEngine.reset();
            audioPollingThread.reset();
            httpServer.reset();
            serverThread.reset();

            LOG_INFO("Service stopped successfully");
        }
        catch (...) {
            LOG_WARN("Error during shutdown (ignored)");
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
            LOG_INFO("Starting HTTP server in service thread...");
            
            if (!httpServer->Start()) {
                LOG_ERROR("Failed to start HTTP server in service mode!");
                serviceRunning = false;
                return;
            }
            
            LOG_INFO("HTTP server started successfully on port 8777");
            LOG_INFO("Server is now listening on: http://localhost:8777");
            LOG_INFO("API endpoint: http://localhost:8777/context");
            
            // Run the server loop
            httpServer->Run();
            
            LOG_INFO("HTTP server loop ended");
        }
        catch (const std::exception& e) {
            LOG_ERROR_FMT("HTTP server thread exception: %s", e.what());
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
            LOG_DEBUG_FMT("Handling request: %s %s", request.method.c_str(), request.path.c_str());

            if (request.path == "/context" && request.method == "GET") {
                if (contextCollector) {
                    Json context = contextCollector->CollectCurrentContext();
                    response.SetHeader("Content-Type", "application/json");
                    response.SetBody(context.toString());
                    response.status = 200;
                    LOG_DEBUG("Returned context data successfully");
                } else {
                    response.SetBody("{\"error\":\"Service not initialized\"}");
                    response.status = 500;
                    LOG_ERROR("Context collector not initialized");
                }
            }
            else if (request.path == "/dashboard" && request.method == "GET") {
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                LOG_DEBUG("Served dashboard HTML");
            }
            else if (request.path == "/" && request.method == "GET") {
                // Redirect root to dashboard
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                LOG_DEBUG("Served dashboard HTML from root");
            }
            else {
                response.SetBody("{\"error\":\"Not found\"}");
                response.status = 404;
                LOG_DEBUG_FMT("Path not found: %s", request.path.c_str());
            }
        }
        catch (const std::exception& e) {
            response.SetBody("{\"error\":\"Internal server error\"}");
            response.status = 500;
            LOG_ERROR_FMT("Exception in request handler: %s", e.what());
        }
    }
};

int main(int argc, char* argv[]) {
    // =========================================
    // Initialize Logger FIRST (before anything)
    // =========================================
    Logger::GetInstance().Initialize("PerceptionEngine.log", LogLevel::DEBUG_L);
    
    LOG_INFO("=====================================");
    LOG_INFO("Perception Engine v1.0");
    LOG_INFO("=====================================");
    
    // Parse command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        
        PerceptionEngineService service;
        
        if (arg == "--install") {
            LOG_INFO("Installing Windows service...");
            if (service.Install()) {
                LOG_INFO("Service installed successfully.");
                Logger::GetInstance().Shutdown();
                return 0;
            } else {
                LOG_ERROR("Failed to install service. Run as administrator.");
                Logger::GetInstance().Shutdown();
                return 1;
            }
        }
        else if (arg == "--uninstall") {
            LOG_INFO("Uninstalling Windows service...");
            if (service.Uninstall()) {
                LOG_INFO("Service uninstalled successfully.");
                Logger::GetInstance().Shutdown();
                return 0;
            } else {
                LOG_ERROR("Failed to uninstall service. Run as administrator.");
                Logger::GetInstance().Shutdown();
                return 1;
            }
        }
        else if (arg == "--start") {
            LOG_INFO("Starting Windows service...");
            if (service.Start()) {
                LOG_INFO("Service started successfully.");
                Logger::GetInstance().Shutdown();
                return 0;
            } else {
                LOG_ERROR("Failed to start service.");
                Logger::GetInstance().Shutdown();
                return 1;
            }
        }
        else if (arg == "--stop") {
            LOG_INFO("Stopping Windows service...");
            if (service.Stop()) {
                LOG_INFO("Service stopped successfully.");
                Logger::GetInstance().Shutdown();
                return 0;
            } else {
                LOG_ERROR("Failed to stop service.");
                Logger::GetInstance().Shutdown();
                return 1;
            }
        }
        else if (arg == "--console") {
            // Run as console application for testing
            LOG_INFO("Running Perception Engine as console application...");
            LOG_INFO("Press Ctrl+C to stop.");
            LOG_INFO("-----------------------------------------------------");
            
            try {
                // Create separate instances for console mode
                HttpServer server(8777);
                ContextCollector collector;
                AudioCaptureEngine audioEngine;

                LOG_INFO("Starting context collector...");
                collector.StartPeriodicUpdate();

                // Initialize audio engine
                LOG_INFO("Initializing audio engine...");
                std::atomic<bool> audioRunning{false};
                std::unique_ptr<std::thread> audioPollingThread;

                if (audioEngine.Initialize("D:/PerceiptionEngine_Howard/perception_engine/windows_code/build/bin/Release/models/whisper/ggml-tiny.en.bin")) {
                    LOG_INFO("Audio engine initialized");

                    // Set callback
                    audioEngine.SetTranscriptionCallback([&collector](const std::string& transcription) {
                        collector.UpdateVoiceContext(transcription);
                        LOG_INFO_FMT("Voice: %s", transcription.c_str());
                    });

                    if (audioEngine.Start()) {
                        LOG_INFO("Audio capture started");
                        audioRunning = true;

                        // Start polling thread
                        audioPollingThread = std::make_unique<std::thread>([&audioEngine, &audioRunning]() {
                            while (audioRunning.load()) {
                                audioEngine.GetLatestUserSpeech();
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            }
                        });
                    } else {
                        LOG_WARN("Failed to start audio capture");
                    }
                } else {
                    LOG_WARN("Failed to initialize audio engine");
                }

                LOG_INFO("Camera vision: Using Python client (C++ ONNX disabled)");

                LOG_INFO("Setting up request handler...");
                server.SetRequestHandler([&collector](const HttpRequest& request, HttpResponse& response) {
                    LOG_DEBUG_FMT("Received request: %s %s", request.method.c_str(), request.path.c_str());

                    if (request.path == "/context" && request.method == "GET") {
                        Json context = collector.CollectCurrentContext();
                        response.SetHeader("Content-Type", "application/json");
                        response.SetBody(context.toString());
                        response.status = 200;
                        LOG_DEBUG("Sent context response");
                    }
                    else if (request.path == "/dashboard" || request.path == "/" && request.method == "GET") {
                        std::ifstream file("dashboard.html");
                        if (file.is_open()) {
                            std::string html((std::istreambuf_iterator<char>(file)),
                                           std::istreambuf_iterator<char>());
                            response.SetHeader("Content-Type", "text/html; charset=utf-8");
                            response.SetBody(html);
                            response.status = 200;
                            LOG_DEBUG("Served dashboard HTML");
                        } else {
                            response.SetBody("<html><body><h1>Error: dashboard.html not found</h1></body></html>");
                            response.SetHeader("Content-Type", "text/html");
                            response.status = 500;
                            LOG_ERROR("dashboard.html not found");
                        }
                    }
                    else if (request.path == "/update_context" && request.method == "POST") {
                        try {
                            std::string body = request.body;
                            LOG_DEBUG_FMT("POST body: %s", body.c_str());

                            // Extract device type
                            size_t devicePos = body.find("\"device\"");
                            if (devicePos == std::string::npos) {
                                LOG_ERROR("Missing device field in body");
                                response.SetBody("{\"error\":\"Missing device field\"}");
                                response.status = 400;
                            } else {
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
                                    LOG_INFO_FMT("Camera update: %s", caption.c_str());

                                    response.SetHeader("Content-Type", "application/json");
                                    response.SetBody("{\"status\":\"ok\"}");
                                    response.status = 200;
                                } else {
                                    response.SetBody("{\"error\":\"Unknown device type\"}");
                                    response.status = 400;
                                }
                            }
                        } catch (const std::exception& e) {
                            LOG_ERROR_FMT("Failed to parse update_context: %s", e.what());
                            response.SetBody("{\"error\":\"Invalid JSON\"}");
                            response.status = 400;
                        }
                    }
                    else {
                        response.SetBody("{\"error\":\"Not found\"}");
                        response.status = 404;
                        LOG_DEBUG_FMT("Sent 404 response for: %s", request.path.c_str());
                    }
                });
                
                LOG_INFO("Starting HTTP server on port 8777...");
                if (!server.Start()) {
                    LOG_ERROR("Failed to start HTTP server!");
                    LOG_ERROR("Possible causes:");
                    LOG_ERROR("1. Port 8777 is already in use");
                    LOG_ERROR("2. Insufficient permissions");
                    LOG_ERROR("3. Firewall blocking the connection");
                    Logger::GetInstance().Shutdown();
                    return 1;
                }
                
                LOG_INFO("HTTP server started successfully!");
                LOG_INFO("Server is now listening on: http://localhost:8777");
                LOG_INFO("Dashboard: http://localhost:8777/dashboard");
                LOG_INFO("API endpoint: http://localhost:8777/context");
                LOG_INFO("-----------------------------------------------------");
                
                LOG_INFO("Starting server loop (blocking)...");
                server.Run(); // Blocking call

                LOG_INFO("Server loop ended, cleaning up...");

                // Stop audio engine
                if (audioRunning.load()) {
                    audioRunning = false;
                    audioEngine.Stop();
                    if (audioPollingThread && audioPollingThread->joinable()) {
                        audioPollingThread->join();
                    }
                    LOG_INFO("Audio engine stopped");
                }

                collector.StopPeriodicUpdate();
            }
            catch (const std::exception& e) {
                LOG_FATAL_FMT("Exception: %s", e.what());
                Logger::GetInstance().Shutdown();
                return 1;
            }
            
            LOG_INFO("Console mode shutting down normally");
            Logger::GetInstance().Shutdown();
            return 0;
        }
        else {
            LOG_ERROR_FMT("Unknown argument: %s", arg.c_str());
            LOG_INFO("Usage: PerceptionEngine.exe [--install|--uninstall|--start|--stop|--console]");
            Logger::GetInstance().Shutdown();
            return 1;
        }
    }
    
    // If no arguments, run as Windows service
    LOG_INFO("Starting as Windows service...");
    try {
        PerceptionEngineService service;
        WindowsService::RunAsService(&service);
    }
    catch (...) {
        LOG_FATAL("Failed to start as Windows service");
        Logger::GetInstance().Shutdown();
        return 1;
    }
    
    LOG_INFO("Application exiting normally");
    Logger::GetInstance().Shutdown();
    return 0;
}
