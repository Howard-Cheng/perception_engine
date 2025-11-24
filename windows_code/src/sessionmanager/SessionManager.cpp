#include "sessionmanager/SessionManager.h"
#include "embeddingmodel/E5EmbeddingDLL.h"
#include "utils/Logger.h"
#include "config/ConfigManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace sessionmanager {

SessionManager::SessionManager(
    std::shared_ptr<database::IDatabaseClient> dbClient,
    const std::string& indexName,
    const Config& config)
    : dbClient_(dbClient)
    , indexName_(indexName)
    , config_(config)
{
    // Generate unique device ID for session ID generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    deviceId_ = "device_" + std::to_string(dis(gen));
    int result = E5_Initialize(L"D:\\Hanson Programs\\test_embedding\\model_q4.onnx");
    if (result != 0) {
        std::cerr << "Initialization failed: " << E5_GetLastError() << std::endl;
    }
    
    std::cout << "[SessionManager] Created with device ID: " << deviceId_ << std::endl;
    std::cout << "[SessionManager] Config: threshold=" << config_.compressionThreshold
              << ", similarity=" << config_.similarityThreshold
              << ", batchSize=" << config_.batchSize << std::endl;
}

SessionManager::~SessionManager() {
    Stop();
    E5_Cleanup();
}

void SessionManager::Start() {
    if (running_.load()) {
        std::cout << "[SessionManager] Already running" << std::endl;
        return;
    }
    
    if (!config_.enabled) {
        std::cout << "[SessionManager] Disabled by configuration" << std::endl;
        return;
    }
    
    running_.store(true);
    workerThread_ = std::thread(&SessionManager::WorkerThread, this);
    std::cout << "[SessionManager] Worker thread started" << std::endl;
}

void SessionManager::Stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Wake up worker thread
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        taskPending_.store(true);
    }
    taskCV_.notify_one();
    
    if (workerThread_.joinable()) {
        workerThread_.join();
        std::cout << "[SessionManager] Worker thread stopped" << std::endl;
    }
}

bool SessionManager::CheckAndTriggerCompression() {
    if (!config_.enabled) {
        return false;
    }
    
    try {
        int uncompressedCount = GetUncompressedCount();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.currentUncompressedCount = uncompressedCount;
        }
        
        std::cout << "[SessionManager] Uncompressed count: " << uncompressedCount 
                 << " (threshold: " << config_.compressionThreshold << ")" << std::endl;
        
        if (uncompressedCount > config_.compressionThreshold) {
            PostCompressionTask();
            return true;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[SessionManager] CheckAndTriggerCompression exception: " 
                 << e.what() << std::endl;
        return false;
    }
}

int SessionManager::GetUncompressedCount() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    
    if (!dbClient_) {
        return 0;
    }
    
    try {
        // Query to count uncompressed records
        std::string query = R"({
            "query": {
                "term": {
                    "compressed": false
                }
            },
            "size": 0
        })";
        
        database::SearchResult result = dbClient_->search(indexName_, query, 0, 0);
        return result.totalHits;
        
    } catch (const std::exception& e) {
        std::cerr << "[SessionManager] GetUncompressedCount exception: " 
                 << e.what() << std::endl;
        return 0;
    }
}

void SessionManager::PostCompressionTask() {
    std::lock_guard<std::mutex> lock(taskMutex_);
    
    if (!taskPending_.load()) {
        taskPending_.store(true);
        taskCV_.notify_one();
        std::cout << "[SessionManager] Compression task posted" << std::endl;
    }
}

void SessionManager::WorkerThread() {
    std::cout << "[SessionManager] Worker thread running" << std::endl;
    
    while (running_.load()) {
        // Wait for compression task
        {
            std::unique_lock<std::mutex> lock(taskMutex_);
            taskCV_.wait(lock, [this] {
                return taskPending_.load() || !running_.load();
            });
            
            if (!running_.load()) {
                break;
            }
            
            taskPending_.store(false);
        }
        
        try {
            ProcessCompressionBatch();
        } catch (const std::exception& e) {
            std::cerr << "[SessionManager] Worker thread exception: " 
                     << e.what() << std::endl;
        }
    }
    
    std::cout << "[SessionManager] Worker thread exiting" << std::endl;
}

void SessionManager::ProcessCompressionBatch() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    
    if (!dbClient_) {
        std::cerr << "[SessionManager] Database client not available" << std::endl;
        return;
    }
    
    try {
        std::cout << "[SessionManager] Starting batch processing..." << std::endl;
        auto startTime = std::chrono::steady_clock::now();
        
        // Get uncompressed records sorted by timestamp (oldest first)
        std::ostringstream queryBuilder;
        queryBuilder << "{"
                     << "\"query\":{\"term\":{\"compressed\":false}},"
                     << "\"sort\":[{\"timestamp\":{\"order\":\"asc\"}}],"
                     << "\"size\":" << config_.batchSize
                     << "}";
        
        database::SearchResult result = dbClient_->search(
            indexName_, 
            queryBuilder.str(), 
            0, 
            config_.batchSize
        );
        
        if (result.events.empty()) {
            std::cout << "[SessionManager] No uncompressed records found" << std::endl;
            return;
        }
        
        std::cout << "[SessionManager] Found " << result.events.size() 
                 << " uncompressed records" << std::endl;
        
        // Process records into sessions
        std::vector<std::string> currentSession;
        Json previousRecord;
        bool firstRecord = true;
        int sessionsCreated = 0;
        int recordsProcessed = 0;
        
        for (const auto& event : result.events) {
            Json currentRecord = ConvertEventToJson(event);
            
            if (firstRecord) {
                // First record starts a new session
                currentSession.push_back(event.eventId);
                previousRecord = currentRecord;
                firstRecord = false;
            } else {
                // Compare with previous record
                int similarity = CompareContent(previousRecord, currentRecord);
                
                std::cout << "[SessionManager] Similarity: " << similarity 
                         << " (threshold: " << config_.similarityThreshold << ")" << std::endl;
                
                if (similarity > config_.similarityThreshold) {
                    // Add to current session
                    currentSession.push_back(event.eventId);
                    previousRecord = currentRecord;
                } else {
                    // End current session and start new one
                    if (!currentSession.empty()) {
                        std::string sessionId = GenerateSessionId();
                        if (MarkRecordsCompressed(currentSession, sessionId)) {
                            sessionsCreated++;
                            recordsProcessed += currentSession.size();
                            std::cout << "[SessionManager] Session " << sessionId 
                                     << " completed (" << currentSession.size() 
                                     << " records)" << std::endl;
                        }
                    }
                    
                    // Start new session
                    currentSession.clear();
                    currentSession.push_back(event.eventId);
                    previousRecord = currentRecord;
                }
            }
        }
        
        // Mark remaining session
        if (!currentSession.empty()) {
            std::string sessionId = GenerateSessionId();
            if (MarkRecordsCompressed(currentSession, sessionId)) {
                sessionsCreated++;
                recordsProcessed += currentSession.size();
                std::cout << "[SessionManager] Final session " << sessionId 
                         << " completed (" << currentSession.size() 
                         << " records)" << std::endl;
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );
        
        // Update statistics
        {
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            stats_.totalSessionsCreated += sessionsCreated;
            stats_.totalRecordsCompressed += recordsProcessed;
            stats_.lastBatchSize = recordsProcessed;
            stats_.lastCompressionTime = endTime;
        }
        
        std::cout << "[SessionManager] Batch processing completed" << std::endl;
        std::cout << "[SessionManager] Created " << sessionsCreated << " sessions, "
                 << "processed " << recordsProcessed << " records in "
                 << duration.count() << " ms" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[SessionManager] ProcessCompressionBatch exception: " 
                 << e.what() << std::endl;
    }
}

Json SessionManager::ConvertEventToJson(const database::RawEvent& event) {
    Json record;
    record.set("app_name", event.appName);
    record.set("window_title", event.windowTitle.value_or(""));
    record.set("screen_content", event.screenContent.value_or(""));
    record.set("timestamp", static_cast<long long>(event.timestamp));
    
    // Build mouse_events array
    std::ostringstream mouseEventsJson;
    mouseEventsJson << "[";
    bool firstMouse = true;
    for (const auto& me : event.mouseEvents) {
        if (!firstMouse) mouseEventsJson << ",";
        firstMouse = false;
        mouseEventsJson << "{"
                       << "\"x\":" << me.posX << ","
                       << "\"y\":" << me.posY << ","
                       << "\"timestamp\":" << me.timestamp
                       << "}";
    }
    mouseEventsJson << "]";
    record.setRaw("mouse_events", mouseEventsJson.str());
    
    return record;
}

int SessionManager::CompareContent(const Json& record1, const Json& record2) {
    switch (algorithm_) {
        case SimilarityAlgorithm::SIMPLE:
            return CompareContentSimple(record1, record2);
        case SimilarityAlgorithm::CONTENT_BASED:
            return CompareContentWithText(record1, record2);
        case SimilarityAlgorithm::TIME_AWARE:
            return CompareContentWithTime(record1, record2);
        case SimilarityAlgorithm::ML_BASED:
            return CompareContentMLBased(record1, record2);
        default:
            return CompareContentSimple(record1, record2);
    }
}

int SessionManager::CompareContentMLBased(const Json& record1, const Json& record2) {

    std::cout << "Model loaded successfully!\n" << std::endl;

    // Test Case 1: Similar documents (sample text)
    std::cout << "=== Test Case 1: Similar Documents ===" << std::endl;
    std::string content1 = record1.getString("screen_content", "");
    std::string content2 = record2.getString("screen_content", "");
    LOG_INFO(std::string("content1:").append(content1));
    LOG_INFO(std::string("content2:").append(content2));

    float similarity1;
    auto result = E5_CompareDocumentsSimple(content1.c_str(), content2.c_str(), &similarity1);

    if (result != 0) {
        std::cerr << "Comparison failed: " << E5_GetLastError() << std::endl;
        LOG_INFO("Comparison failed:");
    }
    else {
        LOG_INFO(std::string("Similarity: ").append(std::to_string(similarity1 / 100).c_str()));
        if (similarity1 >= 70.0f) {
            LOG_INFO("Result: SIMILAR documents ✓");
        }
        else {
            LOG_INFO("RResult: NOT similar documents");
        }
    }

}

int SessionManager::CompareContentSimple(const Json& record1, const Json& record2) {
    std::string app1 = record1.getString("app_name", "");
    std::string app2 = record2.getString("app_name", "");
    std::string window1 = record1.getString("window_title", "");
    std::string window2 = record2.getString("window_title", "");
    
    if (app1 == app2) {
        if (window1 == window2) {
            return 100;  // Identical
        } else {
            return 70;   // Same app, different window
        }
    }
    
    return 0;  // Different app
}

int SessionManager::CompareContentWithText(const Json& record1, const Json& record2) {
    // TODO: Implement text-based similarity
    // For now, fall back to simple comparison
    int baseSimilarity = CompareContentSimple(record1, record2);
    
    if (baseSimilarity == 0) {
        return 0;
    }
    
    // Could add text content similarity here
    // std::string content1 = record1.getString("screen_content", "");
    // std::string content2 = record2.getString("screen_content", "");
    // ... calculate text similarity ...
    
    return baseSimilarity;
}

int SessionManager::CompareContentWithTime(const Json& record1, const Json& record2) {
    int baseSimilarity = CompareContentSimple(record1, record2);
    
    if (baseSimilarity == 0) {
        return 0;
    }
    
    // Apply time decay
    long long timestamp1 = record1.getInt("timestamp", 0);
    long long timestamp2 = record2.getInt("timestamp", 0);
    long long timeDiff = std::abs(timestamp2 - timestamp1);
    
    // If more than 5 minutes apart, reduce similarity
    if (timeDiff > 300) {
        baseSimilarity = static_cast<int>(baseSimilarity * 0.7);
    }
    
    return baseSimilarity;
}

std::string SessionManager::GenerateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;
    
    std::ostringstream oss;
    oss << "session_" << deviceId_ << "_" 
        << timestamp << "_" 
        << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

bool SessionManager::MarkRecordsCompressed(
    const std::vector<std::string>& recordIds,
    const std::string& sessionId)
{
    if (recordIds.empty()) {
        return true;
    }
    
    try {
        bool success = dbClient_->markEventsAsCompressed(
            indexName_, 
            recordIds, 
            sessionId
        );
        
        if (success) {
            std::cout << "[SessionManager] Marked " << recordIds.size() 
                     << " records as compressed with session: " << sessionId << std::endl;
        } else {
            std::cerr << "[SessionManager] Failed to mark records as compressed" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "[SessionManager] MarkRecordsCompressed exception: " 
                 << e.what() << std::endl;
        return false;
    }
}

SessionManager::Config SessionManager::GetConfig() const {
    return config_;
}

void SessionManager::UpdateConfig(const Config& config) {
    config_ = config;
    std::cout << "[SessionManager] Config updated: threshold=" 
             << config_.compressionThreshold
             << ", similarity=" << config_.similarityThreshold
             << ", batchSize=" << config_.batchSize << std::endl;
}

SessionManager::Statistics SessionManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void SessionManager::SetSimilarityAlgorithm(SimilarityAlgorithm algorithm) {
    algorithm_ = algorithm;
    std::cout << "[SessionManager] Similarity algorithm changed to: " 
             << static_cast<int>(algorithm) << std::endl;
}

} // namespace sessionmanager
