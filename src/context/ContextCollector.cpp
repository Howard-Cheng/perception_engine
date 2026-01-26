#include "context/ContextCollector.h"
#include "DatabaseClientFactory.h"
#include "VectorStore.h"
#include "pe_base/config_manager.h"
#include "pe_base/logger.h"
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <condition_variable>

// Helper function to escape JSON strings (replaces pe_base::Json::escapeJsonString)
static std::string escapeJsonString(const std::string& input) {
    std::ostringstream oss;
    for (char c : input) {
        switch (c) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// Helper function to safely get a string value from JSON (handles null and missing keys)
static std::string safeGetString(const nlohmann::json& j, const std::string& key, const std::string& defaultValue = "") {
    if (!j.contains(key)) {
        return defaultValue;
    }
    const auto& val = j[key];
    if (val.is_null()) {
        return defaultValue;
    }
    if (val.is_string()) {
        return val.get<std::string>();
    }
    // For non-string types, convert to string if possible
    return defaultValue;
}

// Helper function to safely get an int value from JSON (handles null and missing keys)
static int safeGetInt(const nlohmann::json& j, const std::string& key, int defaultValue = 0) {
    if (!j.contains(key)) {
        return defaultValue;
    }
    const auto& val = j[key];
    if (val.is_null()) {
        return defaultValue;
    }
    if (val.is_number_integer()) {
        return val.get<int>();
    }
    if (val.is_number()) {
        return static_cast<int>(val.get<double>());
    }
    return defaultValue;
}

// Helper function to safely get a bool value from JSON (handles null and missing keys)
static bool safeGetBool(const nlohmann::json& j, const std::string& key, bool defaultValue = false) {
    if (!j.contains(key)) {
        return defaultValue;
    }
    const auto& val = j[key];
    if (val.is_null()) {
        return defaultValue;
    }
    if (val.is_boolean()) {
        return val.get<bool>();
    }
    return defaultValue;
}

// Helper function to safely get a double value from JSON (handles null and missing keys)
static double safeGetDouble(const nlohmann::json& j, const std::string& key, double defaultValue = 0.0) {
    if (!j.contains(key)) {
        return defaultValue;
    }
    const auto& val = j[key];
    if (val.is_null()) {
        return defaultValue;
    }
    if (val.is_number()) {
        return val.get<double>();
    }
    return defaultValue;
}

ContextCollector::ContextCollector() {
    // Generate unique device ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    deviceId_ = "device_" + std::to_string(dis(gen));

    // Initialize context manager (all providers)
    if (!contextManager_.initialize()) {
        PE_WARN("[ContextCollector] Warning: Some context providers failed to initialize");
    }

    // Register window switch callback
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        PE_INFO("[ContextCollector] Registering OnUserSwitchWindow callback...");
        appProvider->registerWindowSwitchCallback(
            [this](const WindowsAPIs::ActiveAppRecord& record) {
                this->OnUserSwitchWindow(record);
            }
        );
        PE_INFO("[ContextCollector] OnUserSwitchWindow callback registered!");
    }
    else {
        PE_WARN("[ContextCollector] Warning: AppActivityProvider not available!");
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
        PE_INFO("[ContextCollector] Session manager stopped");
    }

    ShutdownDatabase();

    // Cleanup VectorStore
    if (vectorStore_) {
        vectorStore_.reset();
        PE_INFO("[ContextCollector] VectorStore cleaned up");
    }

    // FIX: Clear window switch callback before shutdown
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        appProvider->clearWindowSwitchCallback();
        PE_INFO("[ContextCollector] Window switch callback cleared");
    }

    contextManager_.shutdown();
}

nlohmann::json ContextCollector::CollectCurrentContext() {
    // Trigger all providers to update
    contextManager_.updateAll();

    // Collect all context data
    nlohmann::json context = contextManager_.collectAllContext();

    // Add fused context summary
    context["fusedContext"] = GenerateFusedContext();

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

        PE_INFO("[InitializeDatabase] Creating PostgreSQL client for: " << connectionString);

        // Create PostgreSQL client (with automatic database creation if needed)
        dbClient_ = database::DatabaseClientFactory::createPostgreSQL(connectionString);
        dbCollectionName_ = tableName;

        if (!dbClient_) {
            PE_ERROR("[InitializeDatabase] CRITICAL: Factory returned nullptr!");
            return false;
        }

        PE_INFO("[InitializeDatabase] Testing connection...");
        if (!dbClient_->testConnection()) {
            PE_ERROR("[InitializeDatabase] Failed to connect to database");
            dbClient_.reset();
            return false;
        }

        PE_INFO("[ContextCollector] Connected to PostgreSQL database successfully");

        PE_INFO("[InitializeDatabase] Initializing table: " << tableName);
        if (!dbClient_->initializeCollection(tableName)) {
            PE_ERROR("[ContextCollector] Failed to initialize table: " << tableName);
            dbClient_.reset();
            return false;
        }

        PE_INFO("[ContextCollector] PostgreSQL table initialized: " << tableName);
        dbStorageRunning_.store(true);
        PE_INFO("[InitializeDatabase] Database storage flag set to true");

        sessionManager_ = std::make_unique<sessionmanager::SessionManager>(
            dbClient_,
            tableName
        );
        sessionManager_->Start();
        PE_INFO("[ContextCollector] Session manager initialized and started");

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

            PE_INFO("[ContextCollector] Initializing VectorStore...");
            PE_INFO("[ContextCollector]   Qdrant URL: " << qdrantUrl);
            PE_INFO("[ContextCollector]   Collection: " << qdrantCollection);
            PE_INFO("[ContextCollector]   Model path: " << embeddingModelPath);

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
                PE_ERROR("[ContextCollector] Failed to initialize VectorStore");
                vectorStore_.reset();
                // Don't fail the whole initialization, just log warning
                PE_WARN("[ContextCollector] Warning: Vector DB unavailable, GetVectorDBData will not work");
            }
            else {
                PE_INFO("[ContextCollector] VectorStore initialized successfully");
                PE_INFO("[ContextCollector]   Embedding dimension: " << vectorStore_->getEmbeddingDimension());
            }

        }
        catch (const std::exception& e) {
            PE_ERROR("[ContextCollector] Exception initializing VectorStore: " << e.what());
            vectorStore_.reset();
            // Don't fail the whole initialization
            PE_WARN("[ContextCollector] Warning: Vector DB initialization failed, GetVectorDBData will not work");
        }

        return true;

    }
    catch (const std::exception& e) {
        PE_ERROR("[ContextCollector] Exception initializing PostgreSQL: " << e.what());
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

void ContextCollector::StoreContextToES(const nlohmann::json& context) {
    std::lock_guard<std::mutex> lock(dbClientMutex_);

    if (!dbClient_) {
        // Add detailed logging for debugging
        PE_WARN("[StoreContextToES] Warning: dbClient_ is nullptr, cannot store to database");
        PE_WARN("[StoreContextToES] Database storage running flag: " << dbStorageRunning_.load());
        return;
    }

    try {
        database::RawEvent event = jsonContextToRawEvent(context);

        std::string eventId = dbClient_->indexDocument(dbCollectionName_, event);

        if (!eventId.empty()) {
            PE_INFO("[DBStorage] Stored event: " << event.eventId
                << " | App: " << event.appName);
        }
        else {
            PE_ERROR("[DBStorage] Failed to store event");
        }

    }
    catch (const std::exception& e) {
        PE_ERROR("[DBStorage] Exception: " << e.what());
    }
}

database::RawEvent ContextCollector::jsonContextToRawEvent(const nlohmann::json& context) {
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

    // Extract app context using safe helper functions (handles null values)
    event.appName = safeGetString(context, "activeApp", "Unknown");
    event.windowTitle = safeGetString(context, "windowTitle", "");

    // Extract content
    std::string screenContent = safeGetString(context, "activeAppContent", "");
    if (!screenContent.empty()) {
        event.screenContent = screenContent;
        std::hash<std::string> hasher;
        event.screenContentHash = std::to_string(hasher(screenContent));
    }

    // Multimodal data
    std::string voiceText = safeGetString(context, "voiceTranscription", "");
    if (!voiceText.empty()) {
        event.voiceTranscription = voiceText;
    }

    std::string cameraDesc = safeGetString(context, "cameraDescription", "");
    if (!cameraDesc.empty()) {
        event.cameraDescription = cameraDesc;
    }

    // Mouse events
    if (auto appProvider = contextManager_.getAppActivityProvider()) {
        event.mouseEvents = appProvider->getMouseEvents();
        // Explicit cast from UINT64 to int with range check
        UINT64 count = appProvider->getInteractionCount();
        event.interactionCount = (count > INT_MAX) ? INT_MAX : static_cast<int>(count);
    }

    event.dwellTimeSeconds = safeGetInt(context, "duration", 0);

    // System info
    int battery = safeGetInt(context, "battery", 0);
    if (battery >= 0 && battery <= 100) {
        event.systemInfo.batteryPercent = battery;
    }

    event.systemInfo.isCharging = safeGetBool(context, "isCharging", false);
    event.systemInfo.networkType = safeGetString(context, "networkType", "Unknown");

    double cpuUsage = safeGetDouble(context, "cpuUsage", -1.0);
    if (cpuUsage >= 0.0) {
        event.systemInfo.cpuUsage = cpuUsage;
    }

    double memoryUsage = safeGetDouble(context, "memoryUsage", -1.0);
    if (memoryUsage >= 0.0) {
        event.systemInfo.memoryUsage = memoryUsage;
    }

    bool locationValid = safeGetBool(context, "locationValid", false);
    if (locationValid) {
        double lat = safeGetDouble(context, "locationLat", 0.0);
        double lon = safeGetDouble(context, "locationLon", 0.0);
        std::string description = safeGetString(context, "locationDescription", "");
        if (lat != 0.0 || lon != 0.0) {
            event.systemInfo.locationLat = lat;
            event.systemInfo.locationLon = lon;
            event.systemInfo.locationDescription = description;
        }
    }

    event.compressed = false;

    return event;
}

nlohmann::json ContextCollector::GetESDBData(const std::string& keyword,
    std::time_t startTime,
    std::time_t endTime,
    int maxResults) {
    nlohmann::json result;

    std::lock_guard<std::mutex> lock(dbClientMutex_);

    if (!dbClient_) {
        PE_ERROR("[GetESDBData] Database client not initialized");
        result["error"] = "Database not initialized";
        result["results"] = nlohmann::json::array();
        return result;
    }

    try {
        long long startTimeMs = static_cast<long long>(startTime) * 1000;
        long long endTimeMs = static_cast<long long>(endTime) * 1000;

        // DEBUG: Log query parameters
        PE_INFO("[GetESDBData] Query parameters:");
        PE_INFO("  keyword: " << keyword);
        PE_INFO("  startTime: " << startTime << " (" << startTimeMs << " ms)");
        PE_INFO("  endTime: " << endTime << " (" << endTimeMs << " ms)");
        PE_INFO("  maxResults: " << maxResults);

        // FIX: Include compressed events by default (set includeCompressed: true)
        // This ensures we return all matching events, not just uncompressed ones
        std::ostringstream queryBuilder;
        queryBuilder << "{"
            << "\"keyword\":\"" << escapeJsonString(keyword) << "\","
            << "\"startTime\":" << startTimeMs << ","
            << "\"endTime\":" << endTimeMs << ","
            << "\"size\":" << maxResults << ","
            << "\"includeCompressed\":true"
            << "}";

        std::string query = queryBuilder.str();
        PE_INFO("[GetESDBData] Generated JSON query: " << query);

        database::SearchResult searchResult = dbClient_->search(dbCollectionName_, query, 0, maxResults);

        PE_INFO("[GetESDBData] Search returned: " << searchResult.events.size()
            << " events (totalHits: " << searchResult.totalHits << ")");

        // Convert to nlohmann::json
        nlohmann::json resultsArray = nlohmann::json::array();

        for (const auto& event : searchResult.events) {
            nlohmann::json eventJson;
            eventJson["eventId"] = event.eventId;
            eventJson["timestamp"] = event.timestamp;
            eventJson["deviceId"] = event.deviceId;
            eventJson["appName"] = event.appName;

            if (event.windowTitle.has_value()) {
                eventJson["windowTitle"] = event.windowTitle.value();
            }

            if (event.screenContent.has_value()) {
                eventJson["screenContent"] = event.screenContent.value();
            }

            // Add location information
            if (event.systemInfo.locationLat.has_value() && event.systemInfo.locationLon.has_value()) {
                eventJson["location"] = {
                    {"lat", event.systemInfo.locationLat.value()},
                    {"lon", event.systemInfo.locationLon.value()},
                    {"description", event.systemInfo.locationDescription.value() }
                };
            }

            // Add mouse events
            if (!event.mouseEvents.empty()) {
                nlohmann::json mouseEventsArray = nlohmann::json::array();
                for (const auto& me : event.mouseEvents) {
                    nlohmann::json meJson;
                    meJson["timestamp"] = me.timestamp;
                    if (!me.eventType.empty()) {
                        meJson["eventType"] = me.eventType;
                    }
                    if (!me.content.empty()) {
                        meJson["content"] = me.content;
                    }
                    meJson["posX"] = me.posX;
                    meJson["posY"] = me.posY;
                    if (!me.elementType.empty()) {
                        meJson["elementType"] = me.elementType;
                    }
                    mouseEventsArray.push_back(meJson);
                }
                eventJson["mouseEvents"] = mouseEventsArray;
            }

            resultsArray.push_back(eventJson);
        }

        result["totalHits"] = searchResult.totalHits;
        result["searchTimeMs"] = static_cast<int>(searchResult.searchTimeMs);
        result["results"] = resultsArray;

        return result;

    }
    catch (const std::exception& e) {
        PE_ERROR("[GetESDBData] Exception: " << e.what());
        result["error"] = e.what();
        result["results"] = nlohmann::json::array();
        return result;
    }
}

bool ContextCollector::IsElasticsearchAvailable() const {
    std::lock_guard<std::mutex> lock(dbClientMutex_);
    return dbClient_ != nullptr && dbClient_->testConnection();
}

nlohmann::json ContextCollector::GetVectorDBData(const std::string& keyword,
    std::time_t startTime,
    std::time_t endTime,
    int maxResults) {
    nlohmann::json result;

    try {
        // Check if VectorStore is initialized
        if (!vectorStore_) {
            PE_ERROR("[GetVectorDBData] VectorStore not initialized");
            result["error"] = "VectorStore not initialized";
            result["results"] = nlohmann::json::array();
            return result;
        }

        // DEBUG: Log query parameters
        PE_INFO("[GetVectorDBData] Query parameters:");
        PE_INFO("  keyword: " << keyword);
        PE_INFO("  startTime: " << startTime);
        PE_INFO("  endTime: " << endTime);
        PE_INFO("  maxResults: " << maxResults);

        auto searchStartTime = std::chrono::steady_clock::now();

        // Call VectorStore's querySessionSummaries method
        std::vector<vectordb::SearchResult> searchResults = vectorStore_->querySessionSummaries(
            keyword,
            startTime,
            endTime,
            maxResults,
            std::nullopt  // No score threshold by default
        );

        auto searchEndTime = std::chrono::steady_clock::now();
        auto searchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(searchEndTime - searchStartTime);
        int vectorSearchTimeMs = static_cast<int>(searchDuration.count());

        PE_INFO("[GetVectorDBData] Vector search completed in " << vectorSearchTimeMs << " ms");
        PE_INFO("[GetVectorDBData] Search returned: " << searchResults.size() << " results");
        
        // Convert to nlohmann::json
        nlohmann::json resultsArray = nlohmann::json::array();

        for (const auto& searchResult : searchResults) {
            nlohmann::json resultJson;

            // Add point ID
            if (std::holds_alternative<std::string>(searchResult.id)) {
                resultJson["id"] = std::get<std::string>(searchResult.id);
            }
            else {
                resultJson["id"] = std::get<uint64_t>(searchResult.id);
            }

            // Add similarity score
            resultJson["score"] = searchResult.score;

            // Add payload (metadata) if present
            if (searchResult.payload.has_value()) {
                const auto& payload = searchResult.payload.value();

                // Extract common fields from payload
                auto it = payload.find("summary");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultJson["summary"] = std::get<std::string>(it->second);
                }

                it = payload.find("timestamp");
                if (it != payload.end() && std::holds_alternative<int64_t>(it->second)) {
                    resultJson["timestamp"] = std::get<int64_t>(it->second);
                }

                it = payload.find("session_id");
                if (it != payload.end() && std::holds_alternative<std::string>(it->second)) {
                    resultJson["sessionId"] = std::get<std::string>(it->second);
                }

                it = payload.find("created_at");
                if (it != payload.end() && std::holds_alternative<int64_t>(it->second)) {
                    resultJson["createdAt"] = std::get<int64_t>(it->second);
                }
            }

            resultsArray.push_back(resultJson);
        }

        result["totalHits"] = static_cast<int>(searchResults.size());
        result["searchTimeMs"] = vectorSearchTimeMs;
        result["results"] = resultsArray;

    }
    catch (const std::exception& e) {
        PE_ERROR("[GetVectorDBData] Exception: " << e.what());
        result["error"] = e.what();
        result["results"] = nlohmann::json::array();
    }

    return result;
}

void ContextCollector::OnUserSwitchWindow(const WindowsAPIs::ActiveAppRecord& record) {
    PE_INFO("[ContextCollector] User switched window: " << record.appName);
    PE_INFO("[OnUserSwitchWindow] Database storage running: " << dbStorageRunning_.load());
    PE_INFO("[OnUserSwitchWindow] Database client valid: " << (dbClient_ ? "YES" : "NO"));

    try {
        // Collect current context
        nlohmann::json context = CollectCurrentContext();

        // Store to PostgreSQL
        StoreContextToES(context);

        // NOTE: Compression is now handled by timer (CompressionTimerCallback)
        // No longer calling sessionManager_->CheckAndTriggerCompression() here
        // to prevent blocking due to expensive CompareContent operations

        // Reset mouse records
        if (auto appProvider = contextManager_.getAppActivityProvider()) {
            appProvider->resetMouseRecords();
        }

    }
    catch (const std::exception& e) {
        PE_ERROR("[OnUserSwitchWindow] Exception: " << e.what());
    }
}

// ========================================
//  Compression Timer Functions
// ========================================

void ContextCollector::StartCompressionTimer() {
    if (compressionTimerRunning_.load()) {
        PE_INFO("[ContextCollector] Compression timer already running");
        return;
    }

    if (!sessionManager_) {
        PE_ERROR("[ContextCollector] Cannot start compression timer: SessionManager not initialized");
        return;
    }

    // Create threadpool timer
    compressionTimer_ = CreateThreadpoolTimer(
        CompressionTimerCallback,
        this,  // Pass 'this' as context
        nullptr  // Use default threadpool environment
    );

    if (!compressionTimer_) {
        PE_ERROR("[ContextCollector] Failed to create compression timer");
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
    PE_INFO("[ContextCollector] Compression timer started (runs every 1 minute)");
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
        PE_INFO("[ContextCollector] Compression timer stopped");
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
        PE_INFO("[CompressionTimer] Previous compression task still running, skipping this tick");
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
        PE_INFO("[CompressionTimer] Checking compression status...");

        // This call is now executed asynchronously by the timer
        // It won't block OnUserSwitchWindow anymore
        sessionManager_->CheckAndTriggerCompression();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        PE_INFO("[CompressionTimer] Compression check completed in " << duration.count() << " ms");

    }
    catch (const std::exception& e) {
        PE_ERROR("[CompressionTimer] Exception: " << e.what());
    }
}
