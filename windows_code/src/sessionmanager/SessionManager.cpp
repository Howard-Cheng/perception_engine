#include "sessionmanager/SessionManager.h"
#include "E5EmbeddingDLL.h"
#include "pe_base/logger.h"
#include "pe_base/config_manager.h"
#include "ElasticsearchClient.h"
#include "PostgreSQLClient.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

// Add UTF-8 validation helper function after includes
namespace {
    // Helper function to validate and sanitize UTF-8 strings
    std::string sanitizeUtf8(const std::string& input) {
        std::string output;
        output.reserve(input.size());

        for (size_t i = 0; i < input.size(); ) {
            unsigned char c = static_cast<unsigned char>(input[i]);

            // Single-byte character (ASCII: 0x00-0x7F)
            if (c <= 0x7F) {
                // Filter out control characters except newline, tab, and carriage return
                if (c >= 0x20 || c == '\n' || c == '\r' || c == '\t') {
                    output.push_back(input[i]);
                }
                i++;
            }
            // Two-byte character (0xC0-0xDF)
            else if ((c & 0xE0) == 0xC0) {
                if (i + 1 < input.size()) {
                    unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                    if ((c2 & 0xC0) == 0x80) {
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        i += 2;
                        continue;
                    }
                }
                // Invalid sequence, skip
                i++;
            }
            // Three-byte character (0xE0-0xEF)
            else if ((c & 0xF0) == 0xE0) {
                if (i + 2 < input.size()) {
                    unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                    unsigned char c3 = static_cast<unsigned char>(input[i + 2]);
                    if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        output.push_back(input[i + 2]);
                        i += 3;
                        continue;
                    }
                }
                // Invalid sequence, skip
                i++;
            }
            // Four-byte character (0xF0-0xF7)
            else if ((c & 0xF8) == 0xF0) {
                if (i + 3 < input.size()) {
                    unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                    unsigned char c3 = static_cast<unsigned char>(input[i + 2]);
                    unsigned char c4 = static_cast<unsigned char>(input[i + 3]);
                    if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
                        output.push_back(input[i]);
                        output.push_back(input[i + 1]);
                        output.push_back(input[i + 2]);
                        output.push_back(input[i + 3]);
                        i += 4;
                        continue;
                    }
                }
                // Invalid sequence, skip
                i++;
            }
            // Invalid UTF-8 start byte, skip
            else {
                i++;
            }
        }

        return output;
    }
}

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
            PE_ERROR("[SessionManager] E5 initialization failed: " << E5_GetLastError());
        }
        else {
            PE_INFO("[SessionManager] E5 model loaded successfully");
        }

        if (pe_base::ConfigManager::GetInstance().IsLoaded()) {
            config_.compressionThreshold = pe_base::ConfigManager::GetInstance().GetCompressionThreshold();
            config_.similarityThreshold = static_cast<int>(pe_base::ConfigManager::GetInstance().GetSimilarityThreshold());
            config_.batchSize = pe_base::ConfigManager::GetInstance().GetBatchSize();

            // Initialize retry configuration with sensible defaults
            config_.maxRetries = 3;
            config_.retryDelayMs = 1000;
            config_.useExponentialBackoff = true;
        }
        PE_INFO("[SessionManager] Created with device ID: " << deviceId_);
        PE_INFO("[SessionManager] Config: threshold=" << config_.compressionThreshold
            << ", similarity=" << config_.similarityThreshold
            << ", batchSize=" << config_.batchSize
            << ", maxRetries=" << config_.maxRetries
            << ", retryDelay=" << config_.retryDelayMs << "ms");
    }

    SessionManager::~SessionManager() {
        Stop();
        E5_Cleanup();
    }

    void SessionManager::Start() {
        if (running_.load()) {
            PE_INFO("[SessionManager] Already running");
            return;
        }

        running_.store(true);
        workerThread_ = std::thread(&SessionManager::WorkerThread, this);
        PE_INFO("[SessionManager] Worker thread started");
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
            PE_INFO("[SessionManager] Worker thread stopped");
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

            PE_INFO("[SessionManager] Uncompressed count: " << uncompressedCount
                << " (threshold: " << config_.compressionThreshold << ")");

            if (uncompressedCount > config_.compressionThreshold) {
                PostCompressionTask();
                return true;
            }

            return false;
        }
        catch (const std::exception& e) {
            PE_ERROR("[SessionManager] CheckAndTriggerCompression exception: " << e.what());
            return false;
        }
    }

    int SessionManager::GetUncompressedCount() {
        std::lock_guard<std::mutex> lock(dbMutex_);

        if (!dbClient_) {
            return 0;
        }

        try {
            // Use database-agnostic method instead of ES-specific query
            return dbClient_->getUncompressedCount(indexName_);
        }
        catch (const std::exception& e) {
            PE_ERROR("[SessionManager] GetUncompressedCount exception: " << e.what());
            return 0;
        }
    }

    void SessionManager::PostCompressionTask() {
        std::lock_guard<std::mutex> lock(taskMutex_);

        if (!taskPending_.load()) {
            taskPending_.store(true);
            taskCV_.notify_one();
            PE_INFO("[SessionManager] Compression task posted");
        }
    }

    void SessionManager::WorkerThread() {
        PE_INFO("[SessionManager] Worker thread running");

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
                PE_ERROR("[SessionManager] Worker thread exception: " << e.what());
            }
        }

        PE_INFO("[SessionManager] Worker thread exiting");
    }

    void SessionManager::ProcessCompressionBatch() {
        std::lock_guard<std::mutex> lock(dbMutex_);

        if (!dbClient_) {
            PE_ERROR("[SessionManager] Database client not available");
            return;
        }

        try {
            PE_INFO("[SessionManager] Starting batch processing...");
            auto startTime = std::chrono::steady_clock::now();

            // Use getUncompressedEvents() which is database-agnostic
            // It fetches events sorted by timestamp (oldest first)
            std::vector<database::RawEvent> result = dbClient_->getUncompressedEvents(indexName_);

            if (result.empty()) {
                PE_INFO("[SessionManager] No uncompressed records found");
                return;
            }

            // Limit to batch size
            if (result.size() > static_cast<size_t>(config_.batchSize)) {
                result.resize(config_.batchSize);
            }

            PE_INFO("[SessionManager] Found " << result.size() << " uncompressed records");

            // Process records into sessions
            std::vector<SessionContent> currentSession;
            nlohmann::json previousRecord;  // Changed from pe_base::Json
            bool firstRecord = true;
            int sessionsCreated = 0;
            int recordsProcessed = 0;

            for (const auto& event : result) {
                nlohmann::json currentRecord = ConvertEventToJson(event);  // Changed from pe_base::Json

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
                                if (MarkRecordsCompressedWithRetry(currentSession, sessionId)) {
                                    sessionsCreated++;
                                    recordsProcessed += static_cast<int>(currentSession.size());
                                    PE_INFO("[SessionManager] Session " << sessionId
                                        << " completed (" << currentSession.size()
                                        << " records)");
                                }
                                else {
                                    PE_ERROR("[SessionManager] Failed to mark session " << sessionId
                                        << " after all retry attempts");
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
                if (MarkRecordsCompressedWithRetry(currentSession, sessionId)) {
                    sessionsCreated++;
                    recordsProcessed += static_cast<int>(currentSession.size());
                    PE_INFO("[SessionManager] Final session " << sessionId
                        << " completed (" << currentSession.size()
                        << " records)");
                }
                else {
                    PE_ERROR("[SessionManager] Failed to mark final session " << sessionId
                        << " after all retry attempts");
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

            PE_INFO("[SessionManager] Batch processing completed");
            PE_INFO("[SessionManager] Created " << sessionsCreated << " sessions, "
                << "processed " << recordsProcessed << " records in "
                << duration.count() << " ms");

        }
        catch (const std::exception& e) {
            PE_ERROR("[SessionManager] ProcessCompressionBatch exception: " << e.what());
        }
    }

    nlohmann::json SessionManager::ConvertEventToJson(const database::RawEvent& event) {
        nlohmann::json record;  // Changed from pe_base::Json
        record["app_name"] = event.appName;
        record["window_title"] = event.windowTitle.value_or("");
        record["screen_content"] = event.screenContent.value_or("");
        record["timestamp"] = static_cast<int64_t>(event.timestamp);

        // Build mouse_events array
        nlohmann::json mouseEventsArray = nlohmann::json::array();
        for (const auto& me : event.mouseEvents) {
            nlohmann::json mouseEvent;
            //mouseEvent["x"] = me.posX;
            //mouseEvent["y"] = me.posY;
            mouseEvent["timestamp"] = me.timestamp;
            mouseEventsArray.push_back(mouseEvent);
        }
        record["mouse_events"] = mouseEventsArray;

        return record;
    }

    int SessionManager::CompareContent(const nlohmann::json& record1, const nlohmann::json& record2) {
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

    int SessionManager::CompareContentMLBased(const nlohmann::json& record1, const nlohmann::json& record2) {

        PE_INFO("=== ML-Based Similarity Comparison ===");

        std::string content1 = record1.value("screen_content", "");
        std::string content2 = record2.value("screen_content", "");

        // Check if content is empty
        if (content1.empty() || content2.empty()) {
            PE_WARN("One or both screen contents are empty, falling back to simple comparison");
            return 0;
        }

        PE_INFO(std::string("Comparing content1 (").append(std::to_string(content1.length())).append(" chars)"));
        PE_INFO(std::string("Comparing content2 (").append(std::to_string(content2.length())).append(" chars)"));

        float similarity;
        auto result = E5_CompareDocumentsSimple(content1.c_str(), content2.c_str(), &similarity);

        if (result != 0) {
            PE_ERROR("Comparison failed: " << E5_GetLastError());
            return 0;
        }

        PE_INFO(std::string("Similarity score: ").append(std::to_string(similarity)));

        // Get similar chunks for detailed information
        E5_SimilarChunkPair chunks[5];
        int num_chunks = 0;

        if (E5_GetSimilarChunks(chunks, 5, &num_chunks) == 0 && num_chunks > 0) {
            // Build Content A summary
            std::ostringstream contentA;

            for (int i = 0; i < num_chunks && i < 3; i++) {
                // Sanitize UTF-8 before adding to content
                std::string chunkText = sanitizeUtf8(std::string(chunks[i].text_A));
                contentA << (i + 1) << ":" << chunkText << ".\n\n";
            }

            // Build Content B summary
            std::ostringstream contentB;

            for (int i = 0; i < num_chunks && i < 3; i++) {
                // Sanitize UTF-8 before adding to content
                std::string chunkText = sanitizeUtf8(std::string(chunks[i].text_B));
                contentB << (i + 1) << ":" << chunkText << ".\n\n";
            }

            // Store separated content
            lastContentA_ = contentA.str();
            lastContentB_ = contentB.str();

            // Also keep combined summary for compatibility
            std::ostringstream combined;
            combined << "Similarity: " << similarity << "%\n";
            combined << "Top " << num_chunks << " matching sections:\n\n";

            for (int i = 0; i < num_chunks && i < 3; i++) {
                // Sanitize UTF-8 for combined summary
                std::string textA = sanitizeUtf8(std::string(chunks[i].text_A));
                std::string textB = sanitizeUtf8(std::string(chunks[i].text_B));

                combined << (i + 1) << ". Score: " << chunks[i].similarity_score << "\n";
                combined << "   Content A: " << textA.substr(0, std::min<size_t>(100, textA.length())) << "...\n";
                combined << "   Content B: " << textB.substr(0, std::min<size_t>(100, textB.length())) << "...\n\n";
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

    int SessionManager::CompareContentSimple(const nlohmann::json& record1, const nlohmann::json& record2) {
        std::string app1 = record1.value("app_name", "");
        std::string app2 = record2.value("app_name", "");
        std::string window1 = record1.value("window_title", "");
        std::string window2 = record2.value("window_title", "");

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

    int SessionManager::CompareContentWithText(const nlohmann::json& record1, const nlohmann::json& record2) {
        // TODO: Implement text-based similarity
        // For now, fall back to simple comparison
        int baseSimilarity = CompareContentSimple(record1, record2);

        if (baseSimilarity == 0) {
            return 0;
        }

        // Could add text content similarity here
        // std::string content1 = record1.value("screen_content", "");
        // std::string content2 = record2.value("screen_content", "");
        // ... calculate text similarity ...

        return baseSimilarity;
    }

    int SessionManager::CompareContentWithTime(const nlohmann::json& record1, const nlohmann::json& record2) {
        int baseSimilarity = CompareContentSimple(record1, record2);

        if (baseSimilarity == 0) {
            return 0;
        }

        // Apply time decay
        int64_t timestamp1 = record1.value("timestamp", static_cast<int64_t>(0));
        int64_t timestamp2 = record2.value("timestamp", static_cast<int64_t>(0));
        int64_t timeDiff = std::abs(timestamp2 - timestamp1);

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
            // Group events by whether they have similarity content
            std::vector<SessionContent> eventsWithSimilarity;
            std::vector<SessionContent> eventsWithoutSimilarity;

            for (const auto& content : sessionContents) {
                if (!content.similarScreenContent.empty()) {
                    eventsWithSimilarity.push_back(content);
                } else {
                    eventsWithoutSimilarity.push_back(content);
                }
            }

            bool allSuccess = true;

            // Process events with similarity content (individual updates)
            for (const auto& content : eventsWithSimilarity) {
                // Sanitize the similarity content before sending to database
                std::string sanitizedContent = sanitizeUtf8(content.similarScreenContent);

                // Validate that sanitization produced valid content
                if (sanitizedContent.empty() && !content.similarScreenContent.empty()) {
                    PE_WARN("[SessionManager] Similarity content for event " << content.eventId
                        << " was completely invalid UTF-8, skipping similarity field");

                    // Fall back to update without similarity
                    std::vector<std::string> singleEventId = { content.eventId };
                    bool success = dbClient_->markEventsAsCompressed(
                        indexName_,
                        singleEventId,
                        sessionId
                    );

                    if (!success) {
                        PE_ERROR("[SessionManager] Failed to mark event " << content.eventId
                            << " as compressed");
                        allSuccess = false;
                    }
                }
                else {
                    // Check if database client supports similarity updates
                    auto esClient = std::dynamic_pointer_cast<database::ElasticsearchClient>(dbClient_);
                    auto pgClient = std::dynamic_pointer_cast<database::PostgreSQLClient>(dbClient_);

                    std::vector<std::string> singleEventId = { content.eventId };

                    if (esClient) {
                        // Use Elasticsearch-specific method
                        bool success = esClient->markEventsAsCompressedWithSimilarity(
                            indexName_,
                            singleEventId,
                            sessionId,
                            sanitizedContent
                        );

                        if (!success) {
                            PE_ERROR("[SessionManager] Failed to mark event " << content.eventId
                                << " as compressed with similarity (ES)");
                            allSuccess = false;
                        }
                    }
                    else if (pgClient) {
                        // Use PostgreSQL-specific method
                        bool success = pgClient->markEventsAsCompressedWithSimilarity(
                            indexName_,
                            singleEventId,
                            sessionId,
                            sanitizedContent
                        );

                        if (!success) {
                            PE_ERROR("[SessionManager] Failed to mark event " << content.eventId
                                << " as compressed with similarity (PG)");
                            allSuccess = false;
                        }
                    }
                    else {
                        // Fallback: use standard method without similarity
                        PE_WARN("[SessionManager] Database client doesn't support similarity updates, "
                            << "falling back to standard compressed marking");

                        bool success = dbClient_->markEventsAsCompressed(
                            indexName_,
                            singleEventId,
                            sessionId
                        );

                        if (!success) {
                            PE_ERROR("[SessionManager] Failed to mark event " << content.eventId
                                << " as compressed (fallback)");
                            allSuccess = false;
                        }
                    }
                }
            }

            // Process events without similarity content (bulk update)
            if (!eventsWithoutSimilarity.empty()) {
                std::vector<std::string> eventIds;
                eventIds.reserve(eventsWithoutSimilarity.size());
                for (const auto& content : eventsWithoutSimilarity) {
                    eventIds.push_back(content.eventId);
                }

                bool success = dbClient_->markEventsAsCompressed(
                    indexName_,
                    eventIds,
                    sessionId
                );

                if (!success) {
                    PE_ERROR("[SessionManager] Failed to bulk mark " << eventIds.size()
                        << " events as compressed");
                    allSuccess = false;
                }
            }

            if (allSuccess) {
                PE_INFO("[SessionManager] Marked " << sessionContents.size()
                    << " records as compressed with session: " << sessionId);
            }

            return allSuccess;

        }
        catch (const std::exception& e) {
            PE_ERROR("[SessionManager] MarkRecordsCompressed exception: " << e.what());

            // Log detailed information about the problematic content
            PE_ERROR("[SessionManager] Session ID: " << sessionId);
            PE_ERROR("[SessionManager] Number of events: " << sessionContents.size());

            for (size_t i = 0; i < sessionContents.size(); i++) {
                const auto& content = sessionContents[i];
                PE_ERROR("[SessionManager]   Event[" << i << "]: " << content.eventId
                    << " (similarity length: " << content.similarScreenContent.length() << ")");
            }

            return false;
        }
    }

    bool SessionManager::MarkRecordsCompressedWithRetry(
        const std::vector<SessionContent>& sessionContents,
        const std::string& sessionId)
    {
        if (sessionContents.empty()) {
            return true;
        }

        int retryDelay = config_.retryDelayMs;
        bool finalSuccess = false;

        for (int attempt = 0; attempt <= config_.maxRetries; attempt++) {
            if (attempt > 0) {
                // This is a retry attempt
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.totalRetryAttempts++;

                PE_INFO("[SessionManager] Retry attempt " << attempt
                    << " of " << config_.maxRetries
                    << " for session " << sessionId
                    << " (delay: " << retryDelay << "ms)");

                // Wait before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));

                // Apply exponential backoff
                if (config_.useExponentialBackoff) {
                    retryDelay *= 2;
                }
            }

            try {
                // Attempt to mark records
                bool success = MarkRecordsCompressed(sessionContents, sessionId);

                if (success) {
                    if (attempt > 0) {
                        // Successful retry
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        stats_.successfulRetries++;
                        PE_INFO("[SessionManager] Session " << sessionId
                            << " succeeded on retry attempt " << attempt);
                    }
                    finalSuccess = true;
                    break;
                }
                else {
                    // Operation failed
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.partialFailures++;
                    stats_.totalFailedOperations += static_cast<int>(sessionContents.size());

                    if (attempt == config_.maxRetries) {
                        // Final attempt failed
                        stats_.failedRetries++;
                        PE_ERROR("[SessionManager] Session " << sessionId
                            << " failed after " << (attempt + 1)
                            << " attempts");
                    }
                    else {
                        PE_WARN("[SessionManager] Attempt " << (attempt + 1)
                            << " failed for session " << sessionId
                            << ", will retry...");
                    }
                }
            }
            catch (const std::exception& e) {
                PE_ERROR("[SessionManager] Exception during attempt " << (attempt + 1)
                    << " for session " << sessionId << ": "
                    << e.what());

                if (attempt == config_.maxRetries) {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    stats_.failedRetries++;
                    stats_.totalFailedOperations += static_cast<int>(sessionContents.size());
                }
            }
        }

        return finalSuccess;
    }

    SessionManager::Config SessionManager::GetConfig() const {
        return config_;
    }

    void SessionManager::UpdateConfig(const Config& config) {
        config_ = config;
        PE_INFO("[SessionManager] Config updated: threshold="
            << config_.compressionThreshold
            << ", similarity=" << config_.similarityThreshold
            << ", batchSize=" << config_.batchSize);
    }

    SessionManager::Statistics SessionManager::GetStatistics() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return stats_;
    }

    void SessionManager::SetSimilarityAlgorithm(SimilarityAlgorithm algorithm) {
        algorithm_ = algorithm;
        PE_INFO("[SessionManager] Similarity algorithm changed to: "
            << static_cast<int>(algorithm));
    }

} // namespace sessionmanager
