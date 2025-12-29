#include "context/ContextCollector.h"
#include "DatabaseClientFactory.h"
#include "VectorStore.h"
#include "pe_base/config_manager.h"
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
    
    // Cleanup VectorStore
    if (vectorStore_) {
        vectorStore_.reset();
        std::cout << "[ContextCollector] VectorStore cleaned up" << std::endl;
    }
    
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
// PostgreSQL Integration
// ========================================

bool ContextCollector::InitializeDatabase(const std::string& connectionString,
                                          const std::string& tableName) {
    try {
        std::lock_guard<std::mutex> lock(dbClientMutex_);
        
        std::cout << "[InitializeDatabase] Creating PostgreSQL client for: " << connectionString << std::endl;
        
        // Create PostgreSQL client (with automatic database creation if needed)
        dbClient_ = database::DatabaseClientFactory::createPostgreSQL(connectionString);
        dbCollectionName_ = tableName;
        
        if (!dbClient_) {
            std::cerr << "[InitializeDatabase] CRITICAL: Factory returned nullptr!" << std::endl;
            return false;
        }
        
        std::cout << "[InitializeDatabase] Testing connection..." << std::endl;
        if (!dbClient_->testConnection()) {
            std::cerr << "[InitializeDatabase] Failed to connect to database" << std::endl;
            dbClient_.reset();
            return false;
        }
        
        std::cout << "[ContextCollector] Connected to PostgreSQL database successfully" << std::endl;
        
        std::cout << "[InitializeDatabase] Initializing table: " << tableName << std::endl;
        if (!dbClient_->initializeCollection(tableName)) {
            std::cerr << "[ContextCollector] Failed to initialize table: " << tableName << std::endl;
            dbClient_.reset();
            return false;
        }
        
        std::cout << "[ContextCollector] PostgreSQL table initialized: " << tableName << std::endl;
        dbStorageRunning_.store(true);
        std::cout << "[InitializeDatabase] Database storage flag set to true" << std::endl;
        
        sessionManager_ = std::make_unique<sessionmanager::SessionManager>(
            dbClient_,
            tableName
        );
        sessionManager_->Start();
        std::cout << "[ContextCollector] Session manager initialized and started" << std::endl;
        
        // Start compression timer (runs every 1 minute)
        StartCompressionTimer();
        
        // Initialize VectorStore for session summaries (Qdrant)
        try {
            // Get configuration from ConfigManager
            auto& configMgr = pe_base::ConfigManager::GetInstance();
            
            // Get Qdrant configuration
            std::string qdrantHost = configMgr.GetQdrantHost();
            int qdrantPort = configMgr.GetQdrantPort();
            std::string qdrantCollection = configMgr.GetQdrantCollection();
            
            // Get embedding model path
            std::string embeddingModelPath = configMgr.GetEmbeddingModelPathUtf8();
            
            // Build Qdrant URL
            std::string qdrantUrl = "http://" + qdrantHost + ":" + std::to_string(qdrantPort);
            
            std::cout << "[ContextCollector] Initializing VectorStore..." << std::endl;
            std::cout << "[ContextCollector]   Qdrant URL: " << qdrantUrl << std::endl;
            std::cout << "[ContextCollector]   Collection: " << qdrantCollection << std::endl;
            std::cout << "[ContextCollector]   Model path: " << embeddingModelPath << std::endl;
            
            // Create Qdrant config
            auto qdrantConfig = vectordb::QdrantClient::Config::remote(qdrantUrl);
            
            // Create VectorStore
            vectorStore_ = std::make_unique<vectordb::VectorStore>(
                qdrantCollection,
                embeddingModelPath,
                qdrantConfig
            );
            
            // Initialize VectorStore
            if (!vectorStore_->initialize()) {
                std::cerr << "[ContextCollector] Failed to initialize VectorStore" << std::endl;
                vectorStore_.reset();
                // Don't fail the whole initialization, just log warning
                std::cerr << "[ContextCollector] Warning: Vector DB unavailable, GetVectorDBData will not work" << std::endl;
            } else {
                std::cout << "[ContextCollector] VectorStore initialized successfully" << std::endl;
                std::cout << "[ContextCollector]   Embedding dimension: " << vectorStore_->getEmbeddingDimension() << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ContextCollector] Exception initializing VectorStore: " << e.what() << std::endl;
            vectorStore_.reset();
            // Don't fail the whole initialization
            std::cerr << "[ContextCollector] Warning: Vector DB initialization failed, GetVectorDBData will not work" << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ContextCollector] Exception initializing PostgreSQL: " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(dbClientMutex_);
            dbClient_.reset();
        }
        return false;
    }
}

void ContextCollector::ShutdownDatabase() {
    dbStorageRunning_.store(false);
    
    std::lock_guard<std::mutex> lock(dbClientMutex_);
    dbClient_.reset();
}

void ContextCollector::StoreContextToES(const pe_base::Json& context) {
    std::lock_guard<std::mutex> lock(dbClientMutex_);
    
    if (!dbClient_) {
        // Add detailed logging for debugging
        std::cerr << "[StoreContextToES] Warning: dbClient_ is nullptr, cannot store to database" << std::endl;
        std::cerr << "[StoreContextToES] Database storage running flag: " << dbStorageRunning_.load() << std::endl;
        return;
    }
    
    try {
        database::RawEvent event = jsonContextToRawEvent(context);
        
        std::string eventId = dbClient_->indexDocument(dbCollectionName_, event);
        
        if (!eventId.empty()) {
            std::cout << "[DBStorage] Stored event: " << event.eventId
                      << " | App: " << event.appName << std::endl;
        } else {
            std::cerr << "[DBStorage] Failed to store event" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[DBStorage] Exception: " << e.what() << std::endl;
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
    
    std::lock_guard<std::mutex> lock(dbClientMutex_);
    
    if (!dbClient_) {
        std::cerr << "[GetESDBData] Database client not initialized" << std::endl;
        result.setRaw("error", "\"Database not initialized\"");
        result.setRaw("results", "[]");
        return result;
    }
    
    try {
        long long startTimeMs = static_cast<long long>(startTime) * 1000;
        long long endTimeMs = static_cast<long long>(endTime) * 1000;
        
        // DEBUG: Log query parameters
        std::cout << "[GetESDBData] Query parameters:" << std::endl;
        std::cout << "  keyword: " << keyword << std::endl;
        std::cout << "  startTime: " << startTime << " (" << startTimeMs << " ms)" << std::endl;
        std::cout << "  endTime: " << endTime << " (" << endTimeMs << " ms)" << std::endl;
        std::cout << "  maxResults: " << maxResults << std::endl;
        
        // FIX: Include compressed events by default (set includeCompressed: true)
        // This ensures we return all matching events, not just uncompressed ones
        std::ostringstream queryBuilder;
        queryBuilder << "{"
                     << "\"keyword\":\"" << pe_base::Json::escapeJsonString(keyword) << "\","
                     << "\"startTime\":" << startTimeMs << ","
                     << "\"endTime\":" << endTimeMs << ","
                     << "\"size\":" << maxResults << ","
                     << "\"includeCompressed\":true"  // ← FIX: 包含已压缩的数据
                     << "}";
        
        std::string query = queryBuilder.str();
        std::cout << "[GetESDBData] Generated JSON query: " << query << std::endl;
        
        database::SearchResult searchResult = dbClient_->search(dbCollectionName_, query, 0, maxResults);
        
        std::cout << "[GetESDBData] Search returned: " << searchResult.events.size() 
                  << " events (totalHits: " << searchResult.totalHits << ")" << std::endl;
        
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

            // Add location information
            if (event.systemInfo.locationLat.has_value() && event.systemInfo.locationLon.has_value()) {
                resultsArray << ",\"location\":{"
                             << "\"lat\":" << event.systemInfo.locationLat.value() << ","
                             << "\"lon\":" << event.systemInfo.locationLon.value()
                             << "}";
            }

            // Add mouse events
            if (!event.mouseEvents.empty()) {
                resultsArray << ",\"mouseEvents\":[";
                bool firstMe = true;
                for (const auto& me : event.mouseEvents) {
                    if (!firstMe) resultsArray << ",";
                    firstMe = false;

                    resultsArray << "{";
                    // timestamp (seconds)
                    resultsArray << "\"timestamp\":" << me.timestamp;

                    // eventType
                    if (!me.eventType.empty()) {
                        resultsArray << ",\"eventType\":\"" << pe_base::Json::escapeJsonString(me.eventType) << "\"";
                    }

                    // content
                    if (!me.content.empty()) {
                        resultsArray << ",\"content\":\"" << pe_base::Json::escapeJsonString(me.content) << "\"";
                    }

                    // positions
                    resultsArray << ",\"posX\":" << me.posX << ",\"posY\":" << me.posY;

                    // elementType
                    if (!me.elementType.empty()) {
                        resultsArray << ",\"elementType\":\"" << pe_base::Json::escapeJsonString(me.elementType) << "\"";
                    }

                    resultsArray << "}";
                }
                resultsArray << "]";
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
    std::lock_guard<std::mutex> lock(dbClientMutex_);
    return dbClient_ != nullptr && dbClient_->testConnection();
}

pe_base::Json ContextCollector::GetVectorDBData(const std::string& keyword,
                                    std::time_t startTime,
                                    std::time_t endTime,
                                    int maxResults) {
    pe_base::Json result;
    
    try {
        // Check if VectorStore is initialized
        if (!vectorStore_) {
            std::cerr << "[GetVectorDBData] VectorStore not initialized" << std::endl;
            result.setRaw("error", "\"VectorStore not initialized\"");
            result.setRaw("results", "[]");
            return result;
        }
        
        // DEBUG: Log query parameters
        std::cout << "[GetVectorDBData] Query parameters:" << std::endl;
        std::cout << "  keyword: " << keyword << std::endl;
        std::cout << "  startTime: " << startTime << std::endl;
        std::cout << "  endTime: " << endTime << std::endl;
        std::cout << "  maxResults: " << maxResults << std::endl;
        
        // Call VectorStore's querySessionSummaries method
        std::vector<vectordb::SearchResult> searchResults = vectorStore_->querySessionSummaries(
            keyword,
            startTime,
            endTime,
            maxResults,
            std::nullopt  // No score threshold by default
        );
        
        std::cout << "[GetVectorDBData] Search returned: " << searchResults.size() << " results" << std::endl;
        
        // Convert to pe_base::Json
        std::ostringstream resultsArray;
        resultsArray << "[";
        
        bool first = true;
        for (const auto& searchResult : searchResults) {
            if (!first) resultsArray << ",";
            first = false;
            
            resultsArray << "{";
            
            // Add point ID
            if (std::holds_alternative<std::string>(searchResult.id)) {
                resultsArray << "\"id\":\"" << pe_base::Json::escapeJsonString(std::get<std::string>(searchResult.id)) << "\"";
            } else {
                resultsArray << "\"id\":" << std::get<uint64_t>(searchResult.id);
            }
            
            // Add similarity score
            resultsArray << ",\"score\":" << searchResult.score;
            
            // Add payload (metadata) if present
            if (searchResult.payload.has_value()) {
                const auto& payload = searchResult.payload.value();
                
                // Extract common fields from payload
                auto it = payload.find("summary");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultsArray << ",\"summary\":\"" << pe_base::Json::escapeJsonString(std::get<std::string>(it->second)) << "\"";
                }
                
                it = payload.find("timestamp");
                if (it != payload.end() && std::holds_alternative<int64_t>(it->second)) {
                    resultsArray << ",\"timestamp\":" << std::get<int64_t>(it->second);
                }
                
                it = payload.find("session_id");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultsArray << ",\"sessionId\":\"" << pe_base::Json::escapeJsonString(std::get<std::string>(it->second)) << "\"";
                }
                
                it = payload.find("activity_summary");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultsArray << ",\"activitySummary\":\"" << pe_base::Json::escapeJsonString(std::get<std::string>(it->second)) << "\"";
                }
                
                it = payload.find("app_name");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultsArray << ",\"appName\":\"" << pe_base::Json::escapeJsonString(std::get<std::string>(it->second)) << "\"";
                }
                
                it = payload.find("created_at");
                if (it != payload.end() && std::holds_alternative<int64_t>(it->second)) {
                    resultsArray << ",\"createdAt\":" << std::get<int64_t>(it->second);
                }
            }
            
            resultsArray << "}";
        }
        
        resultsArray << "]";
        
        result.set("totalHits", static_cast<int>(searchResults.size()));
        result.setRaw("results", resultsArray.str());
        
    } catch (const std::exception& e) {
        std::cerr << "[GetVectorDBData] Exception: " << e.what() << std::endl;
        result.setRaw("error", "\"" + pe_base::Json::escapeJsonString(e.what()) + "\"");
        result.setRaw("results", "[]");
    }
    
    return result;
}

void ContextCollector::OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record) {
    std::cout << "[ContextCollector] User switched window: " << record.appName << std::endl;
    std::cout << "[OnUserSwitchWindow] Database storage running: " << dbStorageRunning_.load() << std::endl;
    std::cout << "[OnUserSwitchWindow] Database client valid: " << (dbClient_ ? "YES" : "NO") << std::endl;
    
    try {
        // Collect current context
        pe_base::Json context = CollectCurrentContext();
        
        // Store to PostgreSQL
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
