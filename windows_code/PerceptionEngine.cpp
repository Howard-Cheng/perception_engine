#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_    // Prevent inclusion of winsock.h
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
            LogMessage("[DEBUG] Starting PerceptionEngineService...");

            // Initialize context collector
            contextCollector = std::make_unique<ContextCollector>();
            contextCollector->StartPeriodicUpdate();
            LogMessage("[DEBUG] Context collector started");

            // Initialize audio capture engine
            audioEngine = std::make_unique<AudioCaptureEngine>();
            if (!audioEngine->Initialize("models/whisper/ggml-tiny.en.bin")) {
                LogMessage("[WARNING] Failed to initialize audio engine");
                audioEngine.reset();
            } else {
                LogMessage("[DEBUG] Audio engine initialized");

                // Set callback to update context when new transcription arrives
                audioEngine->SetTranscriptionCallback([this](const std::string& transcription) {
                    if (contextCollector) {
                        // Get latency from audio engine metrics
                        auto metrics = audioEngine->GetMetrics();
                        contextCollector->UpdateVoiceContext(transcription, metrics.whisperLatencyMs);
                        LogMessage("[DEBUG] Voice transcription: " + transcription);
                    }
                });

                // Start audio capture
                if (audioEngine->Start()) {
                    LogMessage("[DEBUG] Audio capture started");

                    // Start polling thread to pull transcriptions
                    audioPollingThread = std::make_unique<std::thread>([this]() {
                        while (serviceRunning.load() && audioEngine) {
                            audioEngine->GetLatestUserSpeech(); // Triggers callback if new result
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    });
                } else {
                    LogMessage("[WARNING] Failed to start audio capture");
                }
            }

            // Initialize camera vision engine
            cameraEngine = std::make_unique<CameraVisionEngine>();
            if (!cameraEngine->Initialize("models/fastvlm", 0)) {
                LogMessage("[WARNING] Failed to initialize camera engine");
                cameraEngine.reset();
            } else {
                LogMessage("[DEBUG] Camera vision engine initialized");

                // Start camera processing thread (every 10 seconds)
                cameraThread = std::make_unique<std::thread>([this]() {
                    while (serviceRunning.load() && cameraEngine) {
                        if (cameraEngine->IsReady()) {
                            std::string description = cameraEngine->DescribeScene();
                            if (!description.empty()) {
                                float latency = cameraEngine->GetLastLatencyMs();
                                if (contextCollector) {
                                    contextCollector->UpdateCameraContext(description, latency);
                                    LogMessage("[DEBUG] Camera scene: " + description + " (latency: " + std::to_string(static_cast<int>(latency)) + "ms)");
                                }
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                    }
                });
                LogMessage("[DEBUG] Camera processing thread started");
            }

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

            // Stop audio engine
            if (audioEngine) {
                audioEngine->Stop();
                LogMessage("[DEBUG] Audio engine stopped");
            }

            // Wait for audio polling thread
            if (audioPollingThread && audioPollingThread->joinable()) {
                audioPollingThread->join();
                LogMessage("[DEBUG] Audio polling thread joined");
            }

            // Wait for camera thread
            if (cameraThread && cameraThread->joinable()) {
                cameraThread->join();
                LogMessage("[DEBUG] Camera thread joined");
            }

            // Clean up camera engine
            if (cameraEngine) {
                cameraEngine.reset();
                LogMessage("[DEBUG] Camera engine stopped");
            }

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

            audioEngine.reset();
            audioPollingThread.reset();
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
            }
            else if (request.path == "/dashboard" && request.method == "GET") {
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                LogMessage("[DEBUG] Served dashboard HTML");
            }
            else if (request.path == "/" && request.method == "GET") {
                // Redirect root to dashboard
                std::string html = LoadDashboardHTML();
                response.SetHeader("Content-Type", "text/html; charset=utf-8");
                response.SetBody(html);
                response.status = 200;
                LogMessage("[DEBUG] Served dashboard HTML from root");
            }
            else {
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
                AudioCaptureEngine audioEngine;

                std::cout << "[DEBUG] Starting context collector..." << std::endl;
                collector.StartPeriodicUpdate();

                // Initialize audio engine
                std::cout << "[DEBUG] Initializing audio engine..." << std::endl;
                std::atomic<bool> audioRunning{false};
                std::unique_ptr<std::thread> audioPollingThread;

                if (audioEngine.Initialize("models/whisper/ggml-tiny.en.bin")) {
                    std::cout << "[DEBUG] Audio engine initialized" << std::endl;

                    // Set callback
                    audioEngine.SetTranscriptionCallback([&collector](const std::string& transcription) {
                        collector.UpdateVoiceContext(transcription);
                        std::cout << "[DEBUG] Voice: " << transcription << std::endl;
                    });

                    if (audioEngine.Start()) {
                        std::cout << "[DEBUG] Audio capture started" << std::endl;
                        audioRunning = true;

                        // Start polling thread
                        audioPollingThread = std::make_unique<std::thread>([&audioEngine, &audioRunning]() {
                            while (audioRunning.load()) {
                                audioEngine.GetLatestUserSpeech();
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            }
                        });
                    } else {
                        std::cout << "[WARNING] Failed to start audio capture" << std::endl;
                    }
                } else {
                    std::cout << "[WARNING] Failed to initialize audio engine" << std::endl;
                }

                // NOTE: C++ camera vision disabled - using Python client instead
                // The Python client (win_camera_fastvlm_pytorch.py) POSTs to /update_context
                // This avoids camera access conflicts and allows us to use PyTorch FastVLM
                std::cout << "[INFO] Camera vision: Using Python client (C++ ONNX disabled)" << std::endl;

                // Initialize camera vision engine
                // std::cout << "[DEBUG] Initializing camera vision engine..." << std::endl;
                // CameraVisionEngine cameraEngine;
                // std::atomic<bool> cameraRunning{false};
                // std::unique_ptr<std::thread> cameraThread;

                // if (cameraEngine.Initialize("models/fastvlm", 0)) {
                //     std::cout << "[DEBUG] Camera vision engine initialized" << std::endl;
                //     cameraRunning = true;

                //     // Start camera processing thread
                //     cameraThread = std::make_unique<std::thread>([&cameraEngine, &collector, &cameraRunning]() {
                //         while (cameraRunning.load()) {
                //             if (cameraEngine.IsReady()) {
                //                 std::string description = cameraEngine.DescribeScene();
                //                 if (!description.empty()) {
                //                     float latency = cameraEngine.GetLastLatencyMs();
                //                     collector.UpdateCameraContext(description, latency);
                //                     std::cout << "[DEBUG] Camera: " << description
                //                               << " (latency: " << static_cast<int>(latency) << "ms)" << std::endl;
                //                 }
                //             }
                //             std::this_thread::sleep_for(std::chrono::seconds(10));
                //         }
                //     });
                //     std::cout << "[DEBUG] Camera processing thread started" << std::endl;
                // } else {
                //     std::cout << "[WARNING] Failed to initialize camera engine" << std::endl;
                // }

                std::cout << "[DEBUG] Setting up request handler..." << std::endl;
                server.SetRequestHandler([&collector](const HttpRequest& request, HttpResponse& response) {
                    std::cout << "[DEBUG] Received request: " << request.method << " " << request.path << std::endl;

                    if (request.path == "/context" && request.method == "GET") {
                        Json context = collector.CollectCurrentContext();
                        response.SetHeader("Content-Type", "application/json");
                        response.SetBody(context.toString());
                        response.status = 200;
                        std::cout << "[DEBUG] Sent context response" << std::endl;
                    }
                    else if (request.path == "/dashboard" || request.path == "/" && request.method == "GET") {
                        std::ifstream file("dashboard.html");
                        if (file.is_open()) {
                            std::string html((std::istreambuf_iterator<char>(file)),
                                           std::istreambuf_iterator<char>());
                            response.SetHeader("Content-Type", "text/html; charset=utf-8");
                            response.SetBody(html);
                            response.status = 200;
                            std::cout << "[DEBUG] Served dashboard HTML" << std::endl;
                        } else {
                            response.SetBody("<html><body><h1>Error: dashboard.html not found</h1></body></html>");
                            response.SetHeader("Content-Type", "text/html");
                            response.status = 500;
                            std::cout << "[ERROR] dashboard.html not found" << std::endl;
                        }
                    }
                    else if (request.path == "/update_context" && request.method == "POST") {
                        try {
                            // Simple JSON parsing for {"device":"Camera","data":{"objects":["description"]}}
                            std::string body = request.body;
                            std::cout << "[DEBUG] POST body: " << body << std::endl;

                            // Extract device type
                            size_t devicePos = body.find("\"device\"");
                            if (devicePos == std::string::npos) {
                                std::cout << "[ERROR] Missing device field in body" << std::endl;
                                response.SetBody("{\"error\":\"Missing device field\"}");
                                response.status = 400;
                            } else {
                                size_t deviceStart = body.find("\"", devicePos + 9);
                                size_t deviceEnd = body.find("\"", deviceStart + 1);
                                std::string device = body.substr(deviceStart + 1, deviceEnd - deviceStart - 1);

                                if (device == "Camera") {
                                    // Extract camera description from objects array
                                    std::string caption;
                                    size_t objectsPos = body.find("\"objects\"");
                                    if (objectsPos != std::string::npos) {
                                        size_t captionStart = body.find("\"", objectsPos + 12);
                                        size_t captionEnd = body.find("\"", captionStart + 1);
                                        if (captionStart != std::string::npos && captionEnd != std::string::npos) {
                                            caption = body.substr(captionStart + 1, captionEnd - captionStart - 1);
                                        }
                                    }

                                    // Update context
                                    collector.UpdateCameraContext(caption, 0.0f);
                                    std::cout << "[DEBUG] Camera update: " << caption << std::endl;

                                    response.SetHeader("Content-Type", "application/json");
                                    response.SetBody("{\"status\":\"ok\"}");
                                    response.status = 200;
                                } else {
                                    response.SetBody("{\"error\":\"Unknown device type\"}");
                                    response.status = 400;
                                }
                            }
                        } catch (const std::exception& e) {
                            std::cout << "[ERROR] Failed to parse update_context: " << e.what() << std::endl;
                            response.SetBody("{\"error\":\"Invalid JSON\"}");
                            response.status = 400;
                        }
                    }
                    else {
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
                std::cout << "[INFO] Dashboard: http://localhost:8777/dashboard" << std::endl;
                std::cout << "[INFO] API endpoint: http://localhost:8777/context" << std::endl;
                std::cout << std::string(50, '-') << std::endl;
                
                std::cout << "Starting server loop (blocking)..." << std::endl;
                server.Run(); // Blocking call

                std::cout << "[DEBUG] Server loop ended, cleaning up..." << std::endl;

                // Stop audio engine
                if (audioRunning.load()) {
                    audioRunning = false;
                    audioEngine.Stop();
                    if (audioPollingThread && audioPollingThread->joinable()) {
                        audioPollingThread->join();
                    }
                    std::cout << "[DEBUG] Audio engine stopped" << std::endl;
                }

                // Stop camera engine
                // (Camera disabled - using Python client)
                // if (cameraRunning.load()) {
                //     cameraRunning = false;
                //     if (cameraThread && cameraThread->joinable()) {
                //         cameraThread->join();
                //     }
                //     std::cout << "[DEBUG] Camera engine stopped" << std::endl;
                // }

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
