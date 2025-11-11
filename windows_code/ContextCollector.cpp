#include "ContextCollector.h"
#include "MouseTracker.h"  // Include here instead of in header
#include "third-party/elasticsearch/include/ElasticsearchClient.h"
#include "third-party/elasticsearch/include/ElasticsearchTypes.h"
#include <thread>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <iostream>  // For console output
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <ctime>

static std::atomic<bool> updateThreadRunning{false};
static std::thread updateThread;
static std::atomic<bool> activeAppMonitoringInitialized{false};

ContextCollector::ContextCollector()
    : latestCameraLatency(0.0f)
    , latestVoiceLatency(0.0f)
    , latestContextUpdateLatency(0.0f)
{
    lastUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    
    // Generate unique device ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    deviceId = "device_" + std::to_string(dis(gen));
}

bool ContextCollector::ShouldUpdateCache() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate);
    return elapsed.count() >= 1; // Update every 1 second
}

void ContextCollector::UpdateCache() {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Collect all context data (BEFORE locking cacheMutex to avoid deadlock)
    auto activeApp = WindowsAPIs::GetForegroundAppName();
    auto battery = WindowsAPIs::GetBatteryPercentage();
    auto isCharging = WindowsAPIs::IsCharging();
    
    // Collect system performance data
    auto cpuUsage = WindowsAPIs::GetCPUUsage();
    auto memoryUsage = WindowsAPIs::GetMemoryUsage();
    auto memoryUsed = WindowsAPIs::GetMemoryUsed();
    auto totalMemory = WindowsAPIs::GetTotalMemory();
    
    auto networkConnected = WindowsAPIs::IsNetworkConnected();
    auto networkType = WindowsAPIs::GetNetworkType();
    auto location = WindowsAPIs::GetLocation();
    auto timestamp = WindowsAPIs::GetCurrentTimestamp();
    
    // Get recent active apps list
    //auto recentApps = WindowsAPIs::GetRecentPeriodActiveAppList();
    
    // NEW: Get current active app content using UIA
    auto activeAppContent = WindowsAPIs::GetCurrentActiveAppContent();

    // Lock cacheMutex for the entire JSON building process
    std::lock_guard<std::mutex> lock(cacheMutex);

    // Clear and rebuild the context
    cachedContext = Json();

    // Build JSON response
    cachedContext.set("activeApp", activeApp);
    cachedContext.set("battery", battery);
    cachedContext.set("isCharging", isCharging);
    
    // Add system performance data with proper formatting
    if (cpuUsage >= 0) {
        std::ostringstream cpuStream;
        cpuStream << std::fixed << std::setprecision(2) << cpuUsage;
        cachedContext.setRaw("cpuUsage", cpuStream.str());
    } else {
        cachedContext.setRaw("cpuUsage", "null");
    }
    
    // memoryUsage: Memory usage percentage
    if (memoryUsage >= 0) {
        std::ostringstream memPercentStream;
        memPercentStream << std::fixed << std::setprecision(2) << memoryUsage;
        cachedContext.setRaw("memoryUsage", memPercentStream.str());
    } else {
        cachedContext.setRaw("memoryUsage", "null");
    }
    
    // memoryUsed: Used memory in GB
    if (memoryUsed >= 0) {
        std::ostringstream memUsedStream;
        memUsedStream << std::fixed << std::setprecision(2) << memoryUsed;
        cachedContext.setRaw("memoryUsedGB", memUsedStream.str());
    } else {
        cachedContext.setRaw("memoryUsedGB", "null");
    }
    
    // totalMemory: Total memory in GB
    if (totalMemory >= 0) {
        std::ostringstream totalMemStream;
        totalMemStream << std::fixed << std::setprecision(2) << totalMemory;
        cachedContext.setRaw("totalMemoryGB", totalMemStream.str());
    } else {
        cachedContext.setRaw("totalMemoryGB", "null");
    }
    
    cachedContext.set("networkConnected", networkConnected);
    cachedContext.set("networkType", networkType);
    
    // Handle WinRT location result properly
    if (location.valid && location.latitude != 0.0 && location.longitude != 0.0) {
        // Valid GPS coordinates
        std::ostringstream latStream, lonStream;
        latStream << std::fixed << std::setprecision(8) << location.latitude;
        lonStream << std::fixed << std::setprecision(8) << location.longitude;
        cachedContext.setRaw("locationLat", latStream.str());
        cachedContext.setRaw("locationLon", lonStream.str());
        cachedContext.setRaw("locationValid", "true");
    } else {
        // Invalid or unable to get location
        cachedContext.setRaw("locationLat", "null");
        cachedContext.setRaw("locationLon", "null");
        cachedContext.setRaw("locationValid", "false");
    }
    
    // NEW: Add active app content to JSON
    if (!activeAppContent.empty()) {
        cachedContext.set("activeAppContent", activeAppContent);
    } else {
        cachedContext.setRaw("activeAppContent", "null");
    }
    
    cachedContext.set("timestamp", timestamp);

    // cacheMutex is released here automatically

    // Calculate and store latency (AFTER releasing cacheMutex to avoid deadlock)
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    {
        std::lock_guard<std::mutex> metricsLock(metricsMutex);
        latestContextUpdateLatency = duration.count() / 1000.0f;
    }

    lastUpdate = std::chrono::steady_clock::now();
}

Json ContextCollector::CollectCurrentContext() {
    if (ShouldUpdateCache()) {
        UpdateCache();
    }

    std::lock_guard<std::mutex> lock(cacheMutex);

    // Get voice text first (before it's needed by GenerateFusedContext)
    std::string voiceText;
    {
        std::lock_guard<std::mutex> voiceLock(voiceMutex);
        voiceText = latestVoiceTranscription;
        if (!latestVoiceTranscription.empty()) {
            cachedContext.set("voiceTranscription", latestVoiceTranscription);
        } else {
            cachedContext.setRaw("voiceTranscription", "null");
        }
    }

    // Add camera vision to context (thread-safe)
    {
        std::lock_guard<std::mutex> cameraLock(cameraMutex);
        if (!latestCameraDescription.empty()) {
            cachedContext.set("cameraDescription", latestCameraDescription);
            cachedContext.set("cameraLatency", static_cast<int>(latestCameraLatency));
        } else {
            cachedContext.setRaw("cameraDescription", "null");
            cachedContext.set("cameraLatency", 0);
        }
    }

    // Add pipeline latency metrics (thread-safe)
    {
        std::lock_guard<std::mutex> metricsLock(metricsMutex);
        std::ostringstream voiceLatencyStream, contextLatencyStream;
        voiceLatencyStream << std::fixed << std::setprecision(2) << latestVoiceLatency;
        contextLatencyStream << std::fixed << std::setprecision(2) << latestContextUpdateLatency;

        cachedContext.setRaw("voiceLatency", voiceLatencyStream.str());
        cachedContext.setRaw("contextUpdateLatency", contextLatencyStream.str());
    }

    // Add fused context summary (pass voiceText to avoid re-locking voiceMutex)
    cachedContext.set("fusedContext", GenerateFusedContext(voiceText));

    return cachedContext;
}

void ContextCollector::UpdateVoiceContext(const std::string& transcription) {
    std::lock_guard<std::mutex> lock(voiceMutex);

    // Clean up common Whisper hallucinations
    std::string cleaned = transcription;

    // List of common hallucination patterns to remove
    const std::vector<std::string> hallucinations = {
        "[no audio]", "[NO AUDIO]",
        "[BLANK_AUDIO]", "[blank_audio]",
        "[BLANK AUDIO]", "[blank audio]",
        "(silence)", "(Silence)", "(SILENCE)",
        "(blank)", "(Blank)", "(BLANK)",
        "[Music]", "[music]", "(Music)", "(music)",
        "[Applause]", "[applause]",
        "Thanks for watching!", "Thank you for watching!",
        "(upbeat music)", "(soft music)"
    };

    // Remove hallucination patterns (trim whitespace after removal)
    for (const auto& hallucination : hallucinations) {
        size_t pos = 0;
        while ((pos = cleaned.find(hallucination, pos)) != std::string::npos) {
            cleaned.erase(pos, hallucination.length());
        }
    }

    // Trim leading/trailing whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        cleaned = cleaned.substr(start, end - start + 1);
    } else {
        cleaned = "";
    }

    latestVoiceTranscription = cleaned;
}

void ContextCollector::UpdateVoiceContext(const std::string& transcription, float latencyMs) {
    // Update latency FIRST (before locking voiceMutex to avoid deadlock)
    {
        std::lock_guard<std::mutex> lock(metricsMutex);
        latestVoiceLatency = latencyMs;
    }

    // Then update transcription (locks voiceMutex)
    UpdateVoiceContext(transcription);
}

void ContextCollector::UpdateCameraContext(const std::string& description, float latencyMs) {
    std::lock_guard<std::mutex> lock(cameraMutex);

    // Store camera vision data in member variables (persists across cache rebuilds)
    latestCameraDescription = description;
    latestCameraLatency = latencyMs;
}

// Overload that fetches voice text itself (may cause deadlock if voiceMutex already locked)
std::string ContextCollector::GenerateFusedContext() const {
    std::string voiceText;
    {
        std::lock_guard<std::mutex> voiceLock(voiceMutex);
        voiceText = latestVoiceTranscription;
    }
    return GenerateFusedContext(voiceText);
}

// Overload that accepts voice text to avoid deadlock
std::string ContextCollector::GenerateFusedContext(const std::string& voiceText) const {
    // NOTE: cacheMutex must already be locked by caller!
    // Do not lock here to avoid deadlock.
    std::ostringstream fused;

    // Current activity
    std::string activeApp = cachedContext.getString("activeApp", "Unknown");
    if (activeApp != "Unknown" && !activeApp.empty()) {
        fused << "Active: " << activeApp;
    }

    // Voice transcription (if recent) - passed as parameter to avoid mutex deadlock
    if (!voiceText.empty()) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "Said: \"" << voiceText << "\"";
    }

    // Battery status (if critical)
    int battery = cachedContext.getInt("battery", 100);
    bool isCharging = cachedContext.getBool("isCharging", false);
    if (battery < 20 && !isCharging) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "Low battery: " << battery << "%";
    }

    // Network status
    bool networkConnected = cachedContext.getBool("networkConnected", true);
    if (!networkConnected) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "Offline";
    }

    // CPU usage (if high)
    double cpuUsage = cachedContext.getDouble("cpuUsage", 0.0);
    if (cpuUsage > 80.0) {
        if (fused.tellp() > 0) fused << " | ";
        fused << "High CPU: " << static_cast<int>(cpuUsage) << "%";
    }

    std::string result = fused.str();
    return result.empty() ? "System running normally" : result;
}

void ContextCollector::StartPeriodicUpdate() {
    if (updateThreadRunning.load()) {
        return; // Already running
    }
    
    updateThreadRunning.store(true);
    updateThread = std::thread([this]() {
        while (updateThreadRunning.load()) {
            if (ShouldUpdateCache()) {
                UpdateCache();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
}

void ContextCollector::StopPeriodicUpdate() {
    updateThreadRunning.store(false);
    if (updateThread.joinable()) {
        updateThread.join();
    }
}

ContextCollector::~ContextCollector() {
    StopPeriodicUpdate();
    
    // ? Shutdown Elasticsearch before other cleanup
    ShutdownElasticsearch();
    
    // Cleanup active app monitoring when the collector is destroyed
    if (activeAppMonitoringInitialized.load()) {
        WindowsAPIs::CleanupActiveAppMonitoring();
        activeAppMonitoringInitialized.store(false);
    }

    // ADD: Stop and cleanup MouseTracker
    if (mouseTracker) {
        try {
            std::lock_guard<std::mutex> lock(mouseTrackerMutex);
            mouseTracker->Stop();
            mouseTracker.reset();
            std::cout << "[ContextCollector] MouseTracker stopped and cleaned up" << std::endl;
        } catch (...) {
            // Ignore cleanup errors in destructor
        }
    }
}

// ??? NEW: Elasticsearch Integration Implementation ???

bool ContextCollector::InitializeElasticsearch(const std::string& esHost, const std::string& indexName) {
    try {
        std::lock_guard<std::mutex> lock(esClientMutex);
        
        // Create Elasticsearch client
        esClient = std::make_unique<elasticsearch::ElasticsearchClient>(esHost);
        esIndexName = indexName;
        
        // Test connection
        if (!esClient->testConnection()) {
            std::cerr << "[ContextCollector] Failed to connect to Elasticsearch at " << esHost << std::endl;
            esClient.reset();
            return false;
        }
        
        std::cout << "[ContextCollector] Connected to Elasticsearch at " << esHost << std::endl;
        
        // Initialize index with proper mapping
        if (!esClient->initializeIndex(indexName)) {
            std::cerr << "[ContextCollector] Failed to initialize index: " << indexName << std::endl;
            esClient.reset();
            return false;
        }
        std::cout << "[ContextCollector] Elasticsearch index initialized: " << indexName << std::endl;
        
        // Start background storage thread
        esStorageRunning.store(true);
        esStorageThread = std::thread(&ContextCollector::ESStorageThreadFunc, this);
        
        std::cout << "[ContextCollector] Elasticsearch storage thread started (5-second interval)" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ContextCollector] Exception initializing Elasticsearch: " << e.what() << std::endl;
        esClient.reset();
        return false;
    } catch (...) {
        std::cerr << "[ContextCollector] Unknown exception initializing Elasticsearch" << std::endl;
        esClient.reset();
        return false;
    }
}

void ContextCollector::ShutdownElasticsearch() {
    // Stop storage thread
    esStorageRunning.store(false);
    if (esStorageThread.joinable()) {
        esStorageThread.join();
        std::cout << "[ContextCollector] Elasticsearch storage thread stopped" << std::endl;
    }
    
    // Cleanup client
    {
        std::lock_guard<std::mutex> lock(esClientMutex);
        esClient.reset();
    }
}

void ContextCollector::ESStorageThreadFunc() {
    std::cout << "[ESStorageThread] Started" << std::endl;
    
    while (esStorageRunning.load()) {
        try {
            // Collect current context
            Json context = CollectCurrentContext();
            
            // Store to Elasticsearch
            StoreContextToES(context);
            
            // Sleep for 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            std::cerr << "[ESStorageThread] Exception: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } catch (...) {
            std::cerr << "[ESStorageThread] Unknown exception" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    std::cout << "[ESStorageThread] Stopped" << std::endl;
}

void ContextCollector::StoreContextToES(const Json& context) {
    std::lock_guard<std::mutex> lock(esClientMutex);
    
    if (!esClient) {
        return;  // ES not initialized
    }
    
    try {
        // Create RawEvent from Json context
        elasticsearch::RawEvent event;
        
        // Generate unique event ID
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        event.eventId = deviceId + "_" + std::to_string(timestamp) + "_" + 
                       std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()).count() % 1000);
        
        event.timestamp = timestamp;
        event.createdAt = timestamp;
        event.deviceId = deviceId;
        
        // Extract app context
        event.appName = context.getString("activeApp", "Unknown");
        
        // Extract content
        std::string activeAppContent = context.getString("activeAppContent", "");
        if (!activeAppContent.empty()) {
            event.screenContent = activeAppContent;
            
            // Simple hash for deduplication (you can improve this)
            std::hash<std::string> hasher;
            event.screenContentHash = std::to_string(hasher(activeAppContent));
        }
        
        // Extract multimodal data
        std::string voiceText = context.getString("voiceTranscription", "");
        if (!voiceText.empty() && voiceText != "null") {
            event.voiceTranscription = voiceText;
        }
        
        std::string cameraDesc = context.getString("cameraDescription", "");
        if (!cameraDesc.empty() && cameraDesc != "null") {
            event.cameraDescription = cameraDesc;
        }
        
        // Extract mouse events from recentMouseTrack
        // TODO: Parse recentMouseTrack JSON and populate event.mouseEvents
        // For now, set interaction count based on presence of data
        event.interactionCount = 0;
        event.dwellTimeSeconds = 0;
        
        // Extract system info
        event.systemInfo.batteryPercent = context.getInt("battery", -1);
        if (event.systemInfo.batteryPercent.value() < 0) {
            event.systemInfo.batteryPercent.reset();
        }
        
        event.systemInfo.isCharging = context.getBool("isCharging", false);
        event.systemInfo.networkType = context.getString("networkType", "Unknown");
        
        double cpuUsage = context.getDouble("cpuUsage", -1.0);
        if (cpuUsage >= 0) {
            event.systemInfo.cpuUsage = cpuUsage;
        }
        
        double memoryUsage = context.getDouble("memoryUsage", -1.0);
        if (memoryUsage >= 0) {
            event.systemInfo.memoryUsage = memoryUsage;
        }
        
        bool locationValid = context.getBool("locationValid", false);
        if (locationValid) {
            double lat = context.getDouble("locationLat", 0.0);
            double lon = context.getDouble("locationLon", 0.0);
            if (lat != 0.0 || lon != 0.0) {
                event.systemInfo.locationLat = lat;
                event.systemInfo.locationLon = lon;
            }
        }
        
        // Status
        event.compressed = false;
        
        // Index document
        std::string eventId = esClient->indexDocument(esIndexName, event);
        
        if (!eventId.empty()) {
            std::cout << "[ESStorage] Stored event: " << event.eventId 
                     << " | App: " << event.appName << std::endl;
        } else {
            std::cerr << "[ESStorage] Failed to store event" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ESStorage] Exception storing context: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[ESStorage] Unknown exception storing context" << std::endl;
    }
}

Json ContextCollector::GetESDBData(const std::string& keyword,
                                   std::time_t startTime,
                                   std::time_t endTime,
                                   int maxResults) {
    Json result;
    
    std::lock_guard<std::mutex> lock(esClientMutex);
    
    if (!esClient) {
        std::cerr << "[GetESDBData] Elasticsearch client not initialized" << std::endl;
        result.setRaw("error", "\"Elasticsearch not initialized\"");
        result.setRaw("results", "[]");
        return result;
    }
    
    try {
        // Build Elasticsearch query
        std::ostringstream queryBuilder;
        queryBuilder << "{"
                    << "\"query\":{"
                    << "  \"bool\":{"
                    << "    \"must\":[";
        
        // Add keyword filter (search in multiple fields)
        if (!keyword.empty()) {
            queryBuilder << "      {"
                        << "        \"multi_match\":{"
                        << "          \"query\":\"" << Json::escapeJsonString(keyword) << "\","
                        << "          \"fields\":[\"screen_content\",\"voice_transcription\","
                        << "                     \"camera_description\",\"app_name\","
                        << "                     \"window_title\"],"
                        << "          \"type\":\"best_fields\","
                        << "          \"fuzziness\":\"AUTO\""
                        << "        }"
                        << "      },";
        }
        
        // Add time range filter
        queryBuilder << "      {"
                    << "        \"range\":{"
                    << "          \"timestamp\":{"
                    << "            \"gte\":" << startTime << ","
                    << "            \"lte\":" << endTime
                    << "          }"
                    << "        }"
                    << "      }";
        
        queryBuilder << "    ]"
                    << "  }"
                    << "},"
                    << "\"sort\":[{\"timestamp\":{\"order\":\"desc\"}}],"
                    << "\"size\":" << maxResults
                    << "}";
        
        std::string query = queryBuilder.str();
        
        std::cout << "[GetESDBData] Query: " << query << std::endl;
        
        // Execute search
        elasticsearch::SearchResult searchResult = esClient->search(esIndexName, query, 0, maxResults);
        
        std::cout << "[GetESDBData] Found " << searchResult.totalHits << " matches in " 
                 << searchResult.searchTimeMs << " ms" << std::endl;
        
        // Convert results to Json array
        std::ostringstream resultsArray;
        resultsArray << "[";
        
        bool first = true;
        for (const auto& event : searchResult.events) {
            if (!first) {
                resultsArray << ",";
            }
            first = false;
            
            resultsArray << "{"
                        << "\"eventId\":\"" << Json::escapeJsonString(event.eventId) << "\","
                        << "\"timestamp\":" << event.timestamp << ","
                        << "\"deviceId\":\"" << Json::escapeJsonString(event.deviceId) << "\","
                        << "\"appName\":\"" << Json::escapeJsonString(event.appName) << "\"";
            
            if (event.windowTitle.has_value()) {
                resultsArray << ",\"windowTitle\":\"" << Json::escapeJsonString(event.windowTitle.value()) << "\"";
            }
            
            if (event.screenContent.has_value()) {
                resultsArray << ",\"screenContent\":\"" << Json::escapeJsonString(event.screenContent.value()) << "\"";
            }
            
            if (event.voiceTranscription.has_value()) {
                resultsArray << ",\"voiceTranscription\":\"" << Json::escapeJsonString(event.voiceTranscription.value()) << "\"";
            }
            
            if (event.cameraDescription.has_value()) {
                resultsArray << ",\"cameraDescription\":\"" << Json::escapeJsonString(event.cameraDescription.value()) << "\"";
            }
            
            // Add system info
            resultsArray << ",\"systemInfo\":{";
            if (event.systemInfo.batteryPercent.has_value()) {
                resultsArray << "\"batteryPercent\":" << event.systemInfo.batteryPercent.value() << ",";
            }
            resultsArray << "\"isCharging\":" << (event.systemInfo.isCharging ? "true" : "false")
                        << ",\"networkType\":\"" << Json::escapeJsonString(event.systemInfo.networkType) << "\"";
            if (event.systemInfo.cpuUsage.has_value()) {
                resultsArray << ",\"cpuUsage\":" << std::fixed << std::setprecision(2) << event.systemInfo.cpuUsage.value();
            }
            if (event.systemInfo.memoryUsage.has_value()) {
                resultsArray << ",\"memoryUsage\":" << std::fixed << std::setprecision(2) << event.systemInfo.memoryUsage.value();
            }
            if (event.systemInfo.locationLat.has_value() && event.systemInfo.locationLon.has_value()) {
                resultsArray << ",\"locationLat\":" << std::fixed << std::setprecision(8) << event.systemInfo.locationLat.value()
                           << ",\"locationLon\":" << std::fixed << std::setprecision(8) << event.systemInfo.locationLon.value();
            }
            resultsArray << "}";
            
            resultsArray << "}";
        }
        
        resultsArray << "]";
        
        result.set("totalHits", searchResult.totalHits);
        result.set("searchTimeMs", static_cast<int>(searchResult.searchTimeMs));
        result.setRaw("results", resultsArray.str());
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "[GetESDBData] Exception: " << e.what() << std::endl;
        result.setRaw("error", "\"" + Json::escapeJsonString(e.what()) + "\"");
        result.setRaw("results", "[]");
        return result;
    } catch (...) {
        std::cerr << "[GetESDBData] Unknown exception" << std::endl;
        result.setRaw("error", "\"Unknown error\"");
        result.setRaw("results", "[]");
        return result;
    }
}

bool ContextCollector::IsElasticsearchAvailable() const {
    std::lock_guard<std::mutex> lock(esClientMutex);
    return esClient != nullptr && esClient->testConnection();
}