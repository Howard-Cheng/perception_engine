#include "collector/DataCollector.h"
#include "layer0/DataIngestion.h"
#ifdef ELASTICSEARCH_ENABLED
#include "layer0/ElasticsearchClient.h"
#endif
#include "common/Logger.h"
#include "common/Utils.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <exception>

using json = nlohmann::json;

namespace perception {
namespace collector {

// CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

DataCollector::DataCollector(const CollectorConfig& config)
    : config_(config)
    , running_(false)
    , totalEvents_(0)
    , failedRequests_(0) {
    
    LOG_INFO("Initializing Data Collector Service");
    LOG_INFO("API URL: " + config_.apiUrl);
    LOG_INFO("Device ID: " + config_.deviceId);
    LOG_INFO("Poll interval: " + std::to_string(config_.pollIntervalSeconds) + "s");
    
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Initialize storage backend
    if (config_.storageBackend == StorageBackend::ELASTICSEARCH) {
#ifdef ELASTICSEARCH_ENABLED
        LOG_INFO("Storage Backend: Elasticsearch");
        LOG_INFO("Elasticsearch URL: " + config_.elasticsearchUrl);
        LOG_INFO("Index: " + config_.elasticsearchIndex);
        
        try {
            esClient_ = std::make_unique<layer0::ElasticsearchClient>(config_.elasticsearchUrl);
            
            // Initialize index
            if (!esClient_->initializeIndex(config_.elasticsearchIndex)) {
                throw std::runtime_error("Failed to initialize Elasticsearch index");
            }
            
            LOG_INFO("? Elasticsearch client initialized successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize Elasticsearch: " + std::string(e.what()));
            throw;
        }
#else
        LOG_ERROR("Elasticsearch support not enabled! Rebuild with -DENABLE_ELASTICSEARCH=ON");
        throw std::runtime_error("Elasticsearch not enabled");
#endif
    } else {
        LOG_INFO("Storage Backend: SQLite");
        LOG_INFO("Database: " + config_.dbPath);
        
        try {
            ingestion_ = std::make_unique<layer0::DataIngestion>(
                config_.dbPath,
                config_.deviceId
            );
            LOG_INFO("? Data ingestion initialized successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize data ingestion: " + std::string(e.what()));
            throw;
        }
    }
}

DataCollector::~DataCollector() {
    stop();
    curl_global_cleanup();
}

void DataCollector::start() {
    if (running_) {
        LOG_WARNING("Data collector is already running");
        return;
    }
    
    // Test connection first
    LOG_INFO("Testing connection to Perception Engine API...");
    if (!testConnection()) {
        LOG_WARNING("Cannot connect to Perception Engine API at " + config_.apiUrl);
        LOG_WARNING("Make sure perception_engine.exe is running on port 8777");
        LOG_WARNING("Will continue attempting to collect data...");
    } else {
        LOG_INFO("? Connected to API");
    }
    
    running_ = true;
    LOG_INFO("");
    LOG_INFO("Starting data collection... (Press Ctrl+C to stop)");
    LOG_INFO("");
    
    while (running_) {
        std::string jsonData;
        
        if (fetchContextData(jsonData)) {
            if (processContextData(jsonData)) {
                totalEvents_++;
            }
        } else {
            failedRequests_++;
        }
        
        // Sleep for the configured interval
        for (int i = 0; i < config_.pollIntervalSeconds && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LOG_INFO("Data collector stopped");
}

void DataCollector::stop() {
    running_ = false;
}

bool DataCollector::isRunning() const {
    return running_;
}

int DataCollector::getTotalEventsCollected() const {
    return totalEvents_;
}

int DataCollector::getFailedRequests() const {
    return failedRequests_;
}

bool DataCollector::testConnection() {
    std::string response;
    return httpGet(config_.apiUrl, response, 5);
}

bool DataCollector::httpGet(const std::string& url, std::string& response, int timeoutSeconds) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }
    
    response.clear();
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // Disable SSL verification for localhost (development only)
    if (url.find("localhost") != std::string::npos || url.find("127.0.0.1") != std::string::npos) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    bool success = (res == CURLE_OK);
    
    if (!success) {
        LOG_ERROR("HTTP request failed: " + std::string(curl_easy_strerror(res)));
    }
    
    curl_easy_cleanup(curl);
    return success;
}

bool DataCollector::fetchContextData(std::string& jsonData) {
    int retries = 0;
    
    while (retries < config_.maxRetries) {
        if (httpGet(config_.apiUrl, jsonData, config_.connectionTimeoutSeconds)) {
            return true;
        }
        
        retries++;
        if (retries < config_.maxRetries) {
            LOG_WARNING("Request failed, retrying... (" + 
                       std::to_string(retries) + "/" + 
                       std::to_string(config_.maxRetries) + ")");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LOG_ERROR("Failed to fetch context data after " + 
             std::to_string(config_.maxRetries) + " retries");
    return false;
}

bool DataCollector::processContextData(const std::string& jsonData) {
    try {
        // Parse JSON
        auto apiData = json::parse(jsonData);
        
        // Create raw event
        layer0::RawEvent event;
        
        // Generate event ID
        event.eventId = utils::generateUUID();
        
        // Set timestamp - use API timestamp if available, otherwise current time
        if (apiData.contains("timestamp") && !apiData["timestamp"].is_null()) {
            // TODO: Parse ISO timestamp string if needed
            // For now, use current time
            event.timestamp = utils::now();
        } else {
            event.timestamp = utils::now();
        }
        
        event.createdAt = utils::now();
        event.deviceId = config_.deviceId;
        
        // Extract app_name (required field with default)
        event.appName = apiData.value("activeApp", "unknown.exe");
        
        // Extract window title (optional)
        if (apiData.contains("activeWindowTitle") && !apiData["activeWindowTitle"].is_null()) {
            event.windowTitle = apiData["activeWindowTitle"].get<std::string>();
        }
        
        // Extract browser URL (optional)
        if (apiData.contains("browserUrl") && !apiData["browserUrl"].is_null()) {
            event.url = apiData["browserUrl"].get<std::string>();
        }
        
        // Extract screen content and compute hash
        if (apiData.contains("activeAppContent") && !apiData["activeAppContent"].is_null()) {
            std::string screenContent = apiData["activeAppContent"].get<std::string>();
            if (!screenContent.empty()) {
                event.screenContent = screenContent;
                event.screenContentHash = utils::computeMD5(screenContent);
            }
        }
        
        // Extract mouse events and calculate interaction count
        int interactionCount = 0;
        if (apiData.contains("mouseEvents") && apiData["mouseEvents"].is_array()) {
            const auto& mouseEventsArray = apiData["mouseEvents"];
            interactionCount = static_cast<int>(mouseEventsArray.size());
            
            for (const auto& me : mouseEventsArray) {
                if (me.is_null()) {
                    continue;
                }
                
                MouseEvent mouseEvent;
                mouseEvent.timestamp = utils::now();
                
                // Extract event type
                mouseEvent.eventType = me.value("event_type", "Unknown");
                
                // Extract content
                mouseEvent.content = me.value("content", "");
                
                // Extract position
                mouseEvent.posX = me.value("pos_x", 0);
                mouseEvent.posY = me.value("pos_y", 0);
                
                // Extract element type
                mouseEvent.elementType = me.value("element_type", "Unknown");
                
                event.mouseEvents.push_back(mouseEvent);
            }
        }
        
        event.interactionCount = interactionCount;
        
        // Set dwell time to poll interval (time since last poll)
        event.dwellTimeSeconds = config_.pollIntervalSeconds;
        
        // Extract voice transcription (optional)
        if (apiData.contains("voiceTranscription") && !apiData["voiceTranscription"].is_null()) {
            std::string transcription = apiData["voiceTranscription"].get<std::string>();
            if (!transcription.empty()) {
                event.voiceTranscription = transcription;
            }
        }
        
        // Extract camera description (optional)
        if (apiData.contains("cameraDescription") && !apiData["cameraDescription"].is_null()) {
            std::string description = apiData["cameraDescription"].get<std::string>();
            if (!description.empty()) {
                event.cameraDescription = description;
            }
        }
        
        // Extract system info
        // Battery percent
        if (apiData.contains("battery") && !apiData["battery"].is_null()) {
            if (apiData["battery"].is_number()) {
                event.systemInfo.batteryPercent = apiData["battery"].get<int>();
            }
        }
        
        // Charging status (with default false)
        event.systemInfo.isCharging = apiData.value("isCharging", false);
        
        // Network type
        if (apiData.contains("networkType") && !apiData["networkType"].is_null()) {
            event.systemInfo.networkType = apiData["networkType"].get<std::string>();
        } else {
            event.systemInfo.networkType = "Unknown";
        }
        
        // Location (optional)
        if (apiData.contains("locationLat") && !apiData["locationLat"].is_null()) {
            if (apiData["locationLat"].is_number()) {
                event.systemInfo.locationLat = apiData["locationLat"].get<double>();
            }
        }
        
        if (apiData.contains("locationLon") && !apiData["locationLon"].is_null()) {
            if (apiData["locationLon"].is_number()) {
                event.systemInfo.locationLon = apiData["locationLon"].get<double>();
            }
        }
        
        // CPU usage (optional)
        if (apiData.contains("cpuUsage") && !apiData["cpuUsage"].is_null()) {
            if (apiData["cpuUsage"].is_number()) {
                event.systemInfo.cpuUsage = apiData["cpuUsage"].get<double>();
            }
        }
        
        // Memory usage (optional)
        if (apiData.contains("memoryUsage") && !apiData["memoryUsage"].is_null()) {
            if (apiData["memoryUsage"].is_number()) {
                event.systemInfo.memoryUsage = apiData["memoryUsage"].get<double>();
            }
        }
        
        event.compressed = false;
        
        // Ingest the event to the selected backend
        std::string eventId;
        
        if (config_.storageBackend == StorageBackend::ELASTICSEARCH) {
#ifdef ELASTICSEARCH_ENABLED
            eventId = esClient_->indexDocument(config_.elasticsearchIndex, event);
            if (eventId.empty()) {
                LOG_ERROR("Failed to index document to Elasticsearch");
                return false;
            }
#endif
        } else {
            eventId = ingestion_->ingestEvent(event);
        }
        
        // Log success with formatted output
        std::ostringstream oss;
        oss << "✅ Collected: ";
        oss << std::left << std::setw(25) << event.appName;
        
        // Show CPU usage if available
        if (event.systemInfo.cpuUsage.has_value()) {
            oss << " | CPU: " << std::setw(5) << std::fixed << std::setprecision(1) 
                << event.systemInfo.cpuUsage.value() << "%";
        } else {
            oss << " | CPU:     - ";
        }
        
        // Show memory usage if available
        if (event.systemInfo.memoryUsage.has_value()) {
            oss << " | Mem: " << std::setw(5) << std::fixed << std::setprecision(1) 
                << event.systemInfo.memoryUsage.value() << "%";
        } else {
            oss << " | Mem:     - ";
        }
        
        oss << " | Events: " << totalEvents_ + 1;
        
        // Show backend
        if (config_.storageBackend == StorageBackend::ELASTICSEARCH) {
            oss << " [ES]";
        } else {
            oss << " [SQLite]";
        }
        
        LOG_INFO(oss.str());
        
        return true;
        
    } catch (const json::parse_error& e) {
        LOG_ERROR("❌ JSON parse error: " + std::string(e.what()));
        return false;
    } catch (const json::type_error& e) {
        LOG_ERROR("❌ JSON type error: " + std::string(e.what()));
        return false;
    } catch (const json::out_of_range& e) {
        LOG_ERROR("❌ JSON out of range error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("❌ Failed to process context data: " + std::string(e.what()));
        return false;
    }
}

} // namespace collector
} // namespace perception
