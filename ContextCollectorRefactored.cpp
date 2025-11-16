#include "ContextCollectorRefactored.h"
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>

ContextCollectorRefactored::ContextCollectorRefactored() {
    // Generate unique device ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    deviceId_ = "device_" + std::to_string(dis(gen));
    
    // Initialize context manager (all providers)
    if (!contextManager_.initialize()) {
        std::cerr << "[ContextCollector] Warning: Some context providers failed to initialize" << std::endl;
    }
    
    // Register window switch callback
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        std::cout << "[ContextCollector] Registering OnUserSwitchWindow callback..." << std::endl;
        appProvider->registerWindowSwitchCallback(
            [this](const WindowsAPIs::ActiveAppRecord& record) {
                this->OnUserSwitchWindow(record);
            }
        );
        std::cout << "[ContextCollector] OnUserSwitchWindow callback registered!" << std::endl;
    } else {
        std::cerr << "[ContextCollector] Warning: AppActivityProvider not available!" << std::endl;
    }
}

ContextCollectorRefactored::~ContextCollectorRefactored() {
    StopPeriodicUpdate();
    ShutdownElasticsearch();
    
    // ? FIX: Clear window switch callback before shutdown
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        appProvider->clearWindowSwitchCallback();
        std::cout << "[ContextCollector] Window switch callback cleared" << std::endl;
    }
    
    contextManager_.shutdown();
}

Json ContextCollectorRefactored::CollectCurrentContext() {
    // Trigger all providers to update
    contextManager_.updateAll();
    
    // Collect all context data
    Json context = contextManager_.collectAllContext();
    
    // Add fused context summary
    context.set("fusedContext", GenerateFusedContext());
    
    return context;
}

void ContextCollectorRefactored::StartPeriodicUpdate() {
    if (updateThreadRunning_.load()) {
        return; // Already running
    }
    
    updateThreadRunning_.store(true);
    updateThread_ = std::thread([this]() {
        updateCacheThread();
    });
}

void ContextCollectorRefactored::StopPeriodicUpdate() {
    updateThreadRunning_.store(false);
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
}

void ContextCollectorRefactored::updateCacheThread() {
    while (updateThreadRunning_.load()) {
        if (contextManager_.shouldUpdate(1)) {
            contextManager_.updateAll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

std::string ContextCollectorRefactored::GenerateFusedContext() const {
    std::ostringstream fused;
    
    // Get app activity
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        std::string activeApp = appProvider->getCurrentApp();
        if (!activeApp.empty() && activeApp != "Unknown") {
            fused << "Active: " << activeApp;
        }
    }
    
    // Get voice transcription
    if (auto voiceProvider = contextManager_.getVoiceProvider()) {
        std::string voiceText = voiceProvider->getTranscription();
        if (!voiceText.empty()) {
            if (fused.tellp() > 0) fused << " | ";
            fused << "Said: \"" << voiceText << "\"";
        }
    }
    
    // System context is handled by SystemContextProvider
    // Add critical system status if needed
    
    std::string result = fused.str();
    return result.empty() ? "System running normally" : result;
}

// ========================================
// Elasticsearch Integration
// ========================================

bool ContextCollectorRefactored::InitializeElasticsearch(const std::string& esHost, 
                                                         const std::string& indexName) {
    try {
        std::lock_guard<std::mutex> lock(esClientMutex_);
        
        esClient_ = database::DatabaseClientFactory::createElasticsearch(esHost);
        esIndexName_ = indexName;
        
        if (!esClient_->testConnection()) {
            std::cerr << "[ContextCollector] Failed to connect to Elasticsearch at " << esHost << std::endl;
            esClient_.reset();
            return false;
        }
        
        std::cout << "[ContextCollector] Connected to Elasticsearch at " << esHost << std::endl;
        
        if (!esClient_->initializeCollection(indexName)) {
            std::cerr << "[ContextCollector] Failed to initialize index: " << indexName << std::endl;
            esClient_.reset();
            return false;
        }
        
        std::cout << "[ContextCollector] Elasticsearch index initialized: " << indexName << std::endl;
        esStorageRunning_.store(true);
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ContextCollector] Exception initializing Elasticsearch: " << e.what() << std::endl;
        esClient_.reset();
        return false;
    }
}

void ContextCollectorRefactored::ShutdownElasticsearch() {
    esStorageRunning_.store(false);
    
    std::lock_guard<std::mutex> lock(esClientMutex_);
    esClient_.reset();
}

void ContextCollectorRefactored::StoreContextToES(const Json& context) {
    std::lock_guard<std::mutex> lock(esClientMutex_);
    
    if (!esClient_) {
        return;
    }
    
    try {
        database::RawEvent event = jsonContextToRawEvent(context);
        
        std::string eventId = esClient_->indexDocument(esIndexName_, event);
        
        if (!eventId.empty()) {
            std::cout << "[ESStorage] Stored event: " << event.eventId
                      << " | App: " << event.appName << std::endl;
        } else {
            std::cerr << "[ESStorage] Failed to store event" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ESStorage] Exception: " << e.what() << std::endl;
    }
}

database::RawEvent ContextCollectorRefactored::jsonContextToRawEvent(const Json& context) {
    database::RawEvent event;
    
    // Generate unique event ID
    auto nowTime = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(nowTime);
    auto timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        nowTime.time_since_epoch()).count();
    
    event.eventId = deviceId_ + "_" + std::to_string(timestamp) + "_" +
                    std::to_string(timestampMs % 1000);
    event.timestamp = timestampMs / 1000;
    event.createdAt = timestampMs / 1000;
    event.deviceId = deviceId_;
    
    // Extract app context
    event.appName = context.getString("activeApp", "Unknown");
    event.windowTitle = context.getString("windowTitle", "");
    
    // Extract content
    std::string screenContent = context.getString("activeAppContent", "");
    if (!screenContent.empty() && screenContent != "null") {
        event.screenContent = screenContent;
        std::hash<std::string> hasher;
        event.screenContentHash = std::to_string(hasher(screenContent));
    }
    
    // Multimodal data
    std::string voiceText = context.getString("voiceTranscription", "");
    if (!voiceText.empty() && voiceText != "null") {
        event.voiceTranscription = voiceText;
    }
    
    std::string cameraDesc = context.getString("cameraDescription", "");
    if (!cameraDesc.empty() && cameraDesc != "null") {
        event.cameraDescription = cameraDesc;
    }
    
    // Mouse events
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        event.mouseEvents = appProvider->getMouseEvents();
        event.interactionCount = appProvider->getInteractionCount();
    }
    
    event.dwellTimeSeconds = context.getInt("duration", 0);
    
    // System info
    int battery = context.getInt("battery", 0);
    if (battery >= 0 && battery <= 100) {
        event.systemInfo.batteryPercent = battery;
    }
    
    event.systemInfo.isCharging = context.getBool("isCharging", false);
    event.systemInfo.networkType = context.getString("networkType", "Unknown");
    
    double cpuUsage = context.getDouble("cpuUsage", -1.0);
    if (cpuUsage >= 0.0) {
        event.systemInfo.cpuUsage = cpuUsage;
    }
    
    double memoryUsage = context.getDouble("memoryUsage", -1.0);
    if (memoryUsage >= 0.0) {
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
    
    event.compressed = false;
    
    return event;
}

Json ContextCollectorRefactored::GetESDBData(const std::string& keyword,
                                             std::time_t startTime,
                                             std::time_t endTime,
                                             int maxResults) {
    Json result;
    
    std::lock_guard<std::mutex> lock(esClientMutex_);
    
    if (!esClient_) {
        std::cerr << "[GetESDBData] Elasticsearch client not initialized" << std::endl;
        result.setRaw("error", "\"Elasticsearch not initialized\"");
        result.setRaw("results", "[]");
        return result;
    }
    
    try {
        long long startTimeMs = static_cast<long long>(startTime) * 1000;
        long long endTimeMs = static_cast<long long>(endTime) * 1000;
        
        // Build query
        std::ostringstream queryBuilder;
        queryBuilder << "{"
                     << "\"query\":{"
                     << "  \"bool\":{"
                     << "    \"must\":[";
        
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
        
        queryBuilder << "      {"
                     << "        \"range\":{"
                     << "          \"timestamp\":{"
                     << "            \"gte\":" << startTimeMs << ","
                     << "            \"lte\":" << endTimeMs
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
        database::SearchResult searchResult = esClient_->search(esIndexName_, query, 0, maxResults);
        
        // Convert to Json
        std::ostringstream resultsArray;
        resultsArray << "[";
        
        bool first = true;
        for (const auto& event : searchResult.events) {
            if (!first) resultsArray << ",";
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
    }
}

bool ContextCollectorRefactored::IsElasticsearchAvailable() const {
    std::lock_guard<std::mutex> lock(esClientMutex_);
    return esClient_ != nullptr && esClient_->testConnection();
}

void ContextCollectorRefactored::OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record) {
    std::cout << "[ContextCollector] User switched window: " << record.appName << std::endl;
    
    try {
        // Collect current context
        Json context = CollectCurrentContext();
        
        // Store to Elasticsearch
        StoreContextToES(context);
        
        // Reset mouse records
        if (auto appProvider = contextManager_.getAppActivityProvider()) {
            appProvider->resetMouseRecords();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[OnUserSwitchWindow] Exception: " << e.what() << std::endl;
    }
}
