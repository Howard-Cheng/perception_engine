/**
 * @file LinguaCore.h
 * @brief LinguaCore - Automatic content summarization and vector storage service
 * 
 * This service periodically checks PostgreSQL for unsummarized content,
 * uses LLM to generate summaries, and stores them in Qdrant vector database.
 */

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <ctime>

// Forward declarations
namespace database {
    class IDatabaseClient;
    struct RawEvent;
}

namespace perception {
    class LLMClient;
}

namespace vectordb {
    class VectorStore;
}

namespace pe_base {
    class TaskQueue;
}

class QtCoreManager;  // Forward declaration for QtCoreManager

namespace linguacore {

/**
 * @brief Configuration for LinguaCore service
 */
struct LinguaCoreConfig {
    // PostgreSQL settings (UPDATED: Changed from Elasticsearch)
    std::string pg_host = "localhost";
    int pg_port = 5432;
    std::string pg_dbname = "perception_engine";
    std::string pg_user = "postgres";
    std::string pg_password = "";
    std::string pg_table = "perception_context";
	int pg_max_undelete_length = 100;
    float pg_out_of_date_hour = 24.0f;
    
    // Check interval
    int check_interval_seconds = 60;  // Default 1 minute
    
    // LLM settings
    std::string llm_model_path = "";
    int llm_max_tokens = 200;
    float llm_temperature = 0.7f;
    
    // Qdrant settings
    std::string qdrant_host = "localhost";
    int qdrant_port = 6333;
    std::string qdrant_collection = "perception_summaries";
    
    // Embedding model settings
    std::string embedding_model_path = "models/embedding/model_q4.onnx";
    
    // Processing settings
    int batch_size = 10;  // Maximum events to process per batch
    bool verbose = true;
    
    // QtCore SDK settings
    std::string qtcore_dll_path = "quantum-sdk-1.0.10.dll";
    std::string qtcore_model = "lucene_LATC-Srv";
    bool qtcore_enabled = true;  // Enable/disable QtCore memory sync
};

/**
 * @brief Main LinguaCore service class
 * 
 * Manages periodic content summarization and vector storage workflow:
 * 1. Query PostgreSQL for unsummarized events
 * 2. Generate summaries using LLM
 * 3. Store summaries in Qdrant vector database
 * 4. Update PostgreSQL summarized flag
 */
class LinguaCore {
public:
    /**
     * @brief Constructor (lightweight, does not throw)
     * @param config Service configuration
     */
    explicit LinguaCore(const LinguaCoreConfig& config);
    
    /**
     * @brief Destructor
     */
    ~LinguaCore();
    
    // Prevent copying
    LinguaCore(const LinguaCore&) = delete;
    LinguaCore& operator=(const LinguaCore&) = delete;
    
    /**
     * @brief Initialize all components (PostgreSQL, LLM, Qdrant)
     * @return true if initialized successfully, false otherwise
     * @note Must be called after construction and before start()
     */
    bool initialize();
    
    /**
     * @brief Start the service (non-blocking)
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the service
     */
    void stop();
    
    /**
     * @brief Check if service is running
     * @return true if running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get service statistics
     * @return JSON string with statistics
     */
    std::string getStatistics() const;
    
    /**
     * @brief Manually trigger a processing cycle (for testing)
     * @return Number of events processed
     */
    int processOnce();

private:
    /**
     * @brief Main service loop (runs in background thread)
     */
    void serviceLoop();
    
    /**
     * @brief Query PostgreSQL for unsummarized events with same session_id
     * @param last_processed_time Timestamp of last processed event
     * @return Vector of events to process (grouped by session_id)
     */
    std::vector<database::RawEvent> queryUnsummarizedEvents(
        std::time_t last_processed_time = 0);
    
    /**
     * @brief Process a single event (screen_content only)
     * @param event Event to process
     * @return true if processed successfully
     */
    bool processSingleEvent(const database::RawEvent& event);
    
    /**
     * @brief Process multiple events (similar_screen_content combined)
     * @param events Events with same session_id to process
     * @return true if processed successfully
     */
    bool processMultipleEvents(const std::vector<database::RawEvent>& events);
    
    /**
     * @brief Generate summary using LLM
     * @param content Content to summarize
     * @return Summary text
     */
    std::string generateSummary(const std::string& content);
    
    /**
     * @brief Store summary in Qdrant vector database
     * @param session_id Session ID
     * @param summary Summary text
     * @param original_content Original content (for metadata)
     * @param created_at Event creation timestamp
     * @param audio_summary Audio transcription summary (optional)
     * @return true if stored successfully
     */
    bool storeSummaryInVectorDB(
        const std::string& session_id,
        const std::string& summary,
        const std::string& original_content,
        std::time_t created_at,
        const std::string& audio_summary = "");  //Add audio_summary parameter with default value
    
    /**
     * @brief Update summarized flag in PostgreSQL
     * @param event_ids Vector of event IDs to update
     * @return true if updated successfully
     */
    bool updateSummarizedFlag(const std::vector<std::string>& event_ids);
    
    /**
     * @brief Async add memory to QtCore (runs in task queue)
     * @param model Model name
     * @param summary Summary text
     * @param date_str Date string
     */
    void asyncAddMemoryToQtCore(const std::string& model, 
                                const std::string& summary, 
                                const std::string& date_str);

private:
    LinguaCoreConfig config_;
    
    // Initialization state
    bool initialized_{false};
    
    // Components (UPDATED: Changed from es_client_ to pg_client_)
    std::unique_ptr<database::IDatabaseClient> pg_client_;
    std::unique_ptr<perception::LLMClient> llm_client_;
    std::unique_ptr<vectordb::VectorStore> vector_store_;
    std::unique_ptr<QtCoreManager> qtcore_manager_;  // QtCore SDK manager
    std::shared_ptr<pe_base::TaskQueue> qtcore_task_queue_;  // Task queue for async QtCore operations
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    
    // Statistics
    std::atomic<int> total_processed_{0};
    std::atomic<int> total_errors_{0};
    std::time_t last_process_time_{0};
};

} // namespace linguacore
