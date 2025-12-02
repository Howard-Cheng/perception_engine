#include "sessionmanager/SessionManager.h"
#include "E5EmbeddingDLL.h"
#include "pe_base/logger.h"
#include "pe_base/config_manager.h"
#include "ElasticsearchClient.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace sessionmanager {

    SessionManager::SessionManager(
        std::shared_ptr<database::IDatabaseClient> dbClient,
        const std::string& indexName)
        : dbClient_(dbClient)
        , indexName_(indexName)
    {
        // Generate unique device ID for session ID generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        deviceId_ = "device_" + std::to_string(dis(gen));
        // Initialize E5 Embedding using pe_base::ConfigManager
        std::wstring modelPath = pe_base::ConfigManager::GetInstance().GetEmbeddingModelPath();
        int result = E5_Initialize(modelPath.c_str());
        if (result != 0) {
            std::cerr << "[SessionManager] E5 initialization failed: " << E5_GetLastError() << std::endl;
            PE_ERROR(std::string("E5 initialization failed: ") + E5_GetLastError());
        }
        else {
            std::cout << "[SessionManager] E5 model loaded successfully" << std::endl;
            PE_INFO("E5 model loaded successfully");
        }

        if(pe_base::ConfigManager::GetInstance().IsLoaded()) {
            config_.compressionThreshold = pe_base::ConfigManager::GetInstance().GetCompressionThreshold();
            config_.similarityThreshold = pe_base::ConfigManager::GetInstance().GetSimilarityThreshold();
            config_.batchSize = pe_base::ConfigManager::GetInstance().GetBatchSize();
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
        }
        catch (const std::exception& e) {
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

        }
        catch (const std::exception& e) {
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
            }
            catch (const std::exception& e) {
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
            std::vector<SessionContent> currentSession;
            pe_base::Json previousRecord;
            bool firstRecord = true;
            int sessionsCreated = 0;
            int recordsProcessed = 0;

            for (const auto& event : result.events) {
                pe_base::Json currentRecord = ConvertEventToJson(event);

                if (firstRecord) {
                    // First record starts a new session
                    currentSession.push_back(SessionContent(event.eventId));
                    previousRecord = currentRecord;
                    firstRecord = false;
                }
                else {
                    // Compare with previous record
                    int similarity = CompareContent(previousRecord, currentRecord);

                    auto threshold = pe_base::ConfigManager::GetInstance().GetSimilarityThreshold();
                    PE_INFO_THIS("Similarity: " << similarity << " (threshold: " << threshold << ")")

                        if (similarity > threshold) {
                            // Add to current session with similarity info
                            SessionContent content(event.eventId);

                            // If using ML-based algorithm, attach similarity summary
                            if (algorithm_ == SimilarityAlgorithm::ML_BASED) {
                                // Update the last element in currentSession with Content A
                                if (!currentSession.empty() && !lastContentA_.empty()) {
                                    currentSession.back().similarScreenContent = lastContentA_;
                                    PE_INFO(std::string("Updated previous event with Content A (")
                                        .append(std::to_string(lastContentA_.length())).append(" chars)"));
                                }

                                // Assign Content B to current element
                                if (!lastContentB_.empty()) {
                                    content.similarScreenContent = lastContentB_;
                                    PE_INFO(std::string("Assigned Content B to current event (")
                                        .append(std::to_string(lastContentB_.length())).append(" chars)"));
                                }

                                // Clear the content after use to prevent reuse
                                lastContentA_.clear();
                                lastContentB_.clear();
                                lastSimilaritySummary_.clear();
                            }

                            currentSession.push_back(content);
                            previousRecord = currentRecord;
                        }
                        else {
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

                            // Clear similarity content when starting new session
                            lastContentA_.clear();
                            lastContentB_.clear();
                            lastSimilaritySummary_.clear();

                            // Start new session
                            currentSession.clear();
                            currentSession.push_back(SessionContent(event.eventId));
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

        }
        catch (const std::exception& e) {
            std::cerr << "[SessionManager] ProcessCompressionBatch exception: "
                << e.what() << std::endl;
        }
    }

    pe_base::Json SessionManager::ConvertEventToJson(const database::RawEvent& event) {
        pe_base::Json record;
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

    int SessionManager::CompareContent(const pe_base::Json& record1, const pe_base::Json& record2) {
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

    int SessionManager::CompareContentMLBased(const pe_base::Json& record1, const pe_base::Json& record2) {

        std::cout << "=== ML-Based Similarity Comparison ===" << std::endl;

        std::string content1 = record1.getString("screen_content", "");
        std::string content2 = record2.getString("screen_content", "");

        // Check if content is empty
        if (content1.empty() || content2.empty()) {
            PE_WARN("One or both screen contents are empty, falling back to simple comparison");
            return CompareContentSimple(record1, record2);
        }

        PE_INFO(std::string("Comparing content1 (").append(std::to_string(content1.length())).append(" chars)"));
        PE_INFO(std::string("Comparing content2 (").append(std::to_string(content2.length())).append(" chars)"));

        float similarity;
        auto result = E5_CompareDocumentsSimple(content1.c_str(), content2.c_str(), &similarity);

        if (result != 0) {
            std::cerr << "Comparison failed: " << E5_GetLastError() << std::endl;
            PE_ERROR(std::string("E5 comparison failed: ") + E5_GetLastError());
            return 0;
        }

        PE_INFO(std::string("Similarity score: ").append(std::to_string(similarity)));

        // Get similar chunks for detailed information
        E5_SimilarChunkPair chunks[5];
        int num_chunks = 0;

        if (E5_GetSimilarChunks(chunks, 5, &num_chunks) == 0 && num_chunks > 0) {
            // Build Content A summary
            std::ostringstream contentA;
            //contentA << "Similarity: " << similarity << "%\n";
            //contentA << "Top " << num_chunks << " matching sections (Previous Content):\n\n";

            for (int i = 0; i < num_chunks && i < 3; i++) {
                contentA << (i + 1) << ":" << std::string(chunks[i].text_A) << ".\n\n";
            }

            // Build Content B summary
            std::ostringstream contentB;
            //contentB << "Similarity: " << similarity << "%\n";
            //contentB << "Top " << num_chunks << " matching sections (Current Content):\n\n";

            for (int i = 0; i < num_chunks && i < 3; i++) {
                contentB << (i + 1) << ":" << std::string(chunks[i].text_B) << ".\n\n";
            }

            // Store separated content
            lastContentA_ = contentA.str();
            lastContentB_ = contentB.str();

            // Also keep combined summary for compatibility
            std::ostringstream combined;
            combined << "Similarity: " << similarity << "%\n";
            combined << "Top " << num_chunks << " matching sections:\n\n";

            for (int i = 0; i < num_chunks && i < 3; i++) {
                combined << (i + 1) << ". Score: " << chunks[i].similarity_score << "\n";
                combined << "   Content A: " << std::string(chunks[i].text_A).substr(0, 100) << "...\n";
                combined << "   Content B: " << std::string(chunks[i].text_B).substr(0, 100) << "...\n\n";
            }

            lastSimilaritySummary_ = combined.str();

            PE_INFO(std::string("Generated similarity summary - Content A (")
                .append(std::to_string(lastContentA_.length())).append(" chars), Content B (")
                .append(std::to_string(lastContentB_.length())).append(" chars)"));
        }
        else {
            lastSimilaritySummary_.clear();
            lastContentA_.clear();
            lastContentB_.clear();
        }

        return static_cast<int>(similarity);
    }

    int SessionManager::CompareContentSimple(const pe_base::Json& record1, const pe_base::Json& record2) {
        std::string app1 = record1.getString("app_name", "");
        std::string app2 = record2.getString("app_name", "");
        std::string window1 = record1.getString("window_title", "");
        std::string window2 = record2.getString("window_title", "");

        if (app1 == app2) {
            if (window1 == window2) {
                return 100;  // Identical
            }
            else {
                return 70;   // Same app, different window
            }
        }

        return 0;  // Different app
    }

    int SessionManager::CompareContentWithText(const pe_base::Json& record1, const pe_base::Json& record2) {
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

    int SessionManager::CompareContentWithTime(const pe_base::Json& record1, const pe_base::Json& record2) {
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
        const std::vector<SessionContent>& sessionContents,
        const std::string& sessionId)
    {
        if (sessionContents.empty()) {
            return true;
        }

        try {
            auto esClient = std::dynamic_pointer_cast<database::ElasticsearchClient>(dbClient_);

            if (esClient) {
                // Use Elasticsearch client - update each event individually with its own similarity content
                bool allSuccess = true;

                for (const auto& content : sessionContents) {
                    std::vector<std::string> singleEventId = { content.eventId };

                    if (!content.similarScreenContent.empty()) {
                        // Update this event with its specific similarity content
                        bool success = esClient->markEventsAsCompressedWithSimilarity(
                            indexName_,
                            singleEventId,
                            sessionId,
                            content.similarScreenContent
                        );

                        if (!success) {
                            std::cerr << "[SessionManager] Failed to mark event " << content.eventId
                                << " as compressed with similarity" << std::endl;
                            allSuccess = false;
                        }
                    }
                    else {
                        // No similarity info for this event, use standard method
                        bool success = esClient->markEventsAsCompressed(
                            indexName_,
                            singleEventId,
                            sessionId
                        );

                        if (!success) {
                            std::cerr << "[SessionManager] Failed to mark event " << content.eventId
                                << " as compressed" << std::endl;
                            allSuccess = false;
                        }
                    }
                }

                if (allSuccess) {
                    std::cout << "[SessionManager] Marked " << sessionContents.size()
                        << " records as compressed with session: " << sessionId
                        << " (with individual similarity info)" << std::endl;
                }

                return allSuccess;
            }
            else {
                // Fallback for non-Elasticsearch clients - use bulk operation without similarity
                std::vector<std::string> recordIds;
                recordIds.reserve(sessionContents.size());
                for (const auto& content : sessionContents) {
                    recordIds.push_back(content.eventId);
                }

                bool success = dbClient_->markEventsAsCompressed(
                    indexName_,
                    recordIds,
                    sessionId
                );

                if (success) {
                    std::cout << "[SessionManager] Marked " << recordIds.size()
                        << " records as compressed with session: " << sessionId << std::endl;
                }
                else {
                    std::cerr << "[SessionManager] Failed to mark records as compressed" << std::endl;
                }

                return success;
            }

        }
        catch (const std::exception& e) {
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
