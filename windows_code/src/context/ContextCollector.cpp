#include "context/ContextCollector.h"
#include "DatabaseClientFactory.h"
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <condition_variable>

ContextCollector::ContextCollector() {
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

ContextCollector::~ContextCollector() {
    StopPeriodicUpdate();
    
    // Stop compression timer
    StopCompressionTimer();
    
    // Stop session manager
    if (sessionManager_) {
        sessionManager_->Stop();
        sessionManager_.reset();
        std::cout << "[ContextCollector] Session manager stopped" << std::endl;
    }
    
    ShutdownDatabase();
    
    // FIX: Clear window switch callback before shutdown
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        appProvider->clearWindowSwitchCallback();
        std::cout << "[ContextCollector] Window switch callback cleared" << std::endl;
    }
    
    contextManager_.shutdown();
}

pe_base::Json ContextCollector::CollectCurrentContext() {
    // Trigger all providers to update
    contextManager_.updateAll();
    
    // Collect all context data
    pe_base::Json context = contextManager_.collectAllContext();
    
    // Add fused context summary
    context.set("fusedContext", GenerateFusedContext());
    
    return context;
}

void ContextCollector::StartPeriodicUpdate() {
    if (updateThreadRunning_.load()) {
        return; // Already running
    }
    
    updateThreadRunning_.store(true);
    updateThread_ = std::thread([this]() {
        updateCacheThread();
    });
}

void ContextCollector::StopPeriodicUpdate() {
    updateThreadRunning_.store(false);
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
}

void ContextCollector::updateCacheThread() {
    while (updateThreadRunning_.load()) {
        if (contextManager_.shouldUpdate(1)) {
            contextManager_.updateAll();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

std::string ContextCollector::GenerateFusedContext() const {
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

bool ContextCollector::InitializeDatabase(const std::string& esHost,
                                          const std::string& indexName) {
    try {
        std::lock_guard<std::mutex> lock(esClientMutex_);
        
        std::cout << "[InitializeDatabase] Creating ES client for: " << esHost << std::endl;
        esClient_ = database::DatabaseClientFactory::createElasticsearch(esHost);
        esIndexName_ = indexName;
        
        if (!esClient_) {
            std::cerr << "[InitializeDatabase] CRITICAL: Factory returned nullptr!" << std::endl;
            return false;
        }
        
        std::cout << "[InitializeDatabase] Testing connection..." << std::endl;
        if (!esClient_->testConnection()) {
            std::cerr << "[ContextCollector] Failed to connect to Elasticsearch at " << esHost << std::endl;
            esClient_.reset();
            std::cerr << "[InitializeDatabase] esClient_ reset to nullptr due to connection failure" << std::endl;
            return false;
        }
        
        std::cout << "[ContextCollector] Connected to Elasticsearch at " << esHost << std::endl;
        
        std::cout << "[InitializeDatabase] Initializing index: " << indexName << std::endl;
        if (!esClient_->initializeCollection(indexName)) {
            std::cerr << "[ContextCollector] Failed to initialize index: " << indexName << std::endl;
            esClient_.reset();
            std::cerr << "[InitializeDatabase] esClient_ reset to nullptr due to index init failure" << std::endl;
            return false;
        }
        
        std::cout << "[ContextCollector] Elasticsearch index initialized: " << indexName << std::endl;
        esStorageRunning_.store(true);
        std::cout << "[InitializeDatabase] ES storage flag set to true" << std::endl;
        
        sessionManager_ = std::make_unique<sessionmanager::SessionManager>(
            esClient_,
            indexName
        );
        sessionManager_->Start();
        std::cout << "[ContextCollector] Session manager initialized and started" << std::endl;
        
        // Start compression timer (runs every 1 minute)
        StartCompressionTimer();
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ContextCollector] Exception initializing Elasticsearch: " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(esClientMutex_);
            esClient_.reset();
            std::cerr << "[InitializeDatabase] esClient_ reset to nullptr due to exception" << std::endl;
        }
        return false;
    }
}

void ContextCollector::ShutdownDatabase() {
    esStorageRunning_.store(false);
    
    std::lock_guard<std::mutex> lock(esClientMutex_);
    esClient_.reset();
}

void ContextCollector::StoreContextToES(const pe_base::Json& context) {
    std::lock_guard<std::mutex> lock(esClientMutex_);
    
    if (!esClient_) {
        // Add detailed logging for debugging
        std::cerr << "[StoreContextToES] Warning: esClient_ is nullptr, cannot store to ES" << std::endl;
        std::cerr << "[StoreContextToES] ES storage running flag: " << esStorageRunning_.load() << std::endl;
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

database::RawEvent ContextCollector::jsonContextToRawEvent(const pe_base::Json& context) {
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

pe_base::Json ContextCollector::GetESDBData(const std::string& keyword,
                                    std::time_t startTime,
                                    std::time_t endTime,
                                    int maxResults) {
    pe_base::Json result;
    
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
                         << "          \"query\":\"" << pe_base::Json::escapeJsonString(keyword) << "\","
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
        
        // Convert to pe_base::Json
        std::ostringstream resultsArray;
        resultsArray << "[";
        
        bool first = true;
        for (const auto& event : searchResult.events) {
            if (!first) resultsArray << ",";
            first = false;
            
            resultsArray << "{"
                         << "\"eventId\":\"" << pe_base::Json::escapeJsonString(event.eventId) << "\","
                         << "\"timestamp\":" << event.timestamp << ","
                         << "\"deviceId\":\"" << pe_base::Json::escapeJsonString(event.deviceId) << "\","
                         << "\"appName\":\"" << pe_base::Json::escapeJsonString(event.appName) << "\"";
            
            if (event.windowTitle.has_value()) {
                resultsArray << ",\"windowTitle\":\"" << pe_base::Json::escapeJsonString(event.windowTitle.value()) << "\"";
            }
            
            if (event.screenContent.has_value()) {
                resultsArray << ",\"screenContent\":\"" << pe_base::Json::escapeJsonString(event.screenContent.value()) << "\"";
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
        result.setRaw("error", "\"" + pe_base::Json::escapeJsonString(e.what()) + "\"");
        result.setRaw("results", "[]");
        return result;
    }
}

bool ContextCollector::IsElasticsearchAvailable() const {
    std::lock_guard<std::mutex> lock(esClientMutex_);
    return esClient_ != nullptr && esClient_->testConnection();
}

void ContextCollector::OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record) {
    std::cout << "[ContextCollector] User switched window: " << record.appName << std::endl;
    std::cout << "[OnUserSwitchWindow] ES storage running: " << esStorageRunning_.load() << std::endl;
    std::cout << "[OnUserSwitchWindow] ES client valid: " << (esClient_ ? "YES" : "NO") << std::endl;
    
    try {
        // Collect current context
        pe_base::Json context = CollectCurrentContext();
        
        // Store to Elasticsearch
        StoreContextToES(context);
        
        // NOTE: Compression is now handled by timer (CompressionTimerCallback)
        // No longer calling sessionManager_->CheckAndTriggerCompression() here
        // to prevent blocking due to expensive CompareContent operations
        
        // Reset mouse records
        if (auto appProvider = contextManager_.getAppActivityProvider()) {
            appProvider->resetMouseRecords();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[OnUserSwitchWindow] Exception: " << e.what() << std::endl;
    }
}

// ========================================
//  Compression Timer Functions
// ========================================

void ContextCollector::StartCompressionTimer() {
    if (compressionTimerRunning_.load()) {
        std::cout << "[ContextCollector] Compression timer already running" << std::endl;
        return;
    }
    
    if (!sessionManager_) {
        std::cerr << "[ContextCollector] Cannot start compression timer: SessionManager not initialized" << std::endl;
        return;
    }
    
    // Create threadpool timer
    compressionTimer_ = CreateThreadpoolTimer(
        CompressionTimerCallback,
        this,  // Pass 'this' as context
        nullptr  // Use default threadpool environment
    );
    
    if (!compressionTimer_) {
        std::cerr << "[ContextCollector] Failed to create compression timer" << std::endl;
        return;
    }
    
    // Set timer to fire every 1 minute
    // First parameter: relative time (negative value = relative time)
    // 1 minute = 60,000 ms = 60,000,000 microseconds = 600,000,000 100-nanosecond intervals
    FILETIME dueTime;
    ULARGE_INTEGER ulDueTime;
    ulDueTime.QuadPart = static_cast<ULONGLONG>(-(60LL * 10000000LL)); // 1 minute in 100-nanosecond intervals (negative = relative)
    dueTime.dwHighDateTime = ulDueTime.HighPart;
    dueTime.dwLowDateTime = ulDueTime.LowPart;
    
    // Period in milliseconds (1 minute = 60,000 ms)
    DWORD period = 60000;
    
    // Window length (0 = no window)
    DWORD windowLength = 0;
    
    SetThreadpoolTimer(compressionTimer_, &dueTime, period, windowLength);
    
    compressionTimerRunning_.store(true);
    std::cout << "[ContextCollector] Compression timer started (runs every 1 minute)" << std::endl;
}

void ContextCollector::StopCompressionTimer() {
    if (!compressionTimerRunning_.load()) {
        return;
    }
    
    compressionTimerRunning_.store(false);
    
    if (compressionTimer_) {
        // Cancel timer and wait for callbacks to complete
        SetThreadpoolTimer(compressionTimer_, nullptr, 0, 0);
        WaitForThreadpoolTimerCallbacks(compressionTimer_, TRUE);
        CloseThreadpoolTimer(compressionTimer_);
        compressionTimer_ = nullptr;
        std::cout << "[ContextCollector] Compression timer stopped" << std::endl;
    }
}

VOID CALLBACK ContextCollector::CompressionTimerCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID Context,
    PTP_TIMER Timer)
{
    // Context is the 'this' pointer
    ContextCollector* collector = static_cast<ContextCollector*>(Context);
    
    if (!collector || !collector->compressionTimerRunning_.load()) {
        return;
    }
    
    // Check if a compression task is already running
    bool expected = false;
    if (!collector->compressionTaskRunning_.compare_exchange_strong(expected, true)) {
        // Another compression task is still running, skip this tick
        std::cout << "[CompressionTimer] Previous compression task still running, skipping this tick" << std::endl;
        return;
    }
    
    // Execute the compression check
    collector->OnCompressionTimerTick();
    
    // Mark task as completed
    collector->compressionTaskRunning_.store(false);
}

void ContextCollector::OnCompressionTimerTick() {
    if (!sessionManager_ || !sessionManager_->IsRunning()) {
        return;
    }
    
    try {
        auto startTime = std::chrono::steady_clock::now();
        std::cout << "[CompressionTimer] Checking compression status..." << std::endl;
        
        // This call is now executed asynchronously by the timer
        // It won't block OnUserSwitchWindow anymore
        sessionManager_->CheckAndTriggerCompression();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        std::cout << "[CompressionTimer] Compression check completed in " << duration.count() << " ms" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CompressionTimer] Exception: " << e.what() << std::endl;
    }
}
