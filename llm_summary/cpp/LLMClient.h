/**
 * @file LLMClient.h
 * @brief LLM Client using llama.cpp for C++
 * 
 * A C++ wrapper for llama.cpp providing text generation, summarization,
 * Q&A, and database reading capabilities.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <filesystem>

// Forward declaration for llama.cpp structs
struct llama_model;
struct llama_context;
struct llama_sampling_context;

namespace perception {

/**
 * @brief Configuration for LLM inference
 */
struct LLMConfig {
    std::filesystem::path model_path;
    int n_ctx = 2048;           // Context window size
    int n_threads = -1;         // Number of threads (-1 for auto)
    int n_gpu_layers = 35;      // Number of GPU layers
    bool verbose = true;
    
    // Sampling parameters
    float temperature = 0.7f;
    int max_tokens = 512;
    float top_p = 0.9f;
    int top_k = 40;
    float repeat_penalty = 1.1f;
};

/**
 * @brief Message structure for chat conversations
 */
struct ChatMessage {
    std::string role;      // "system", "user", or "assistant"
    std::string content;
    
    ChatMessage(const std::string& r, const std::string& c)
        : role(r), content(c) {}
};

/**
 * @brief Database record structure
 */
struct DatabaseRecord {
    std::string id;
    std::string type;
    std::string title;
    std::string url;
    std::string summary;
    std::string timestamp;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> key_points;
};

/**
 * @brief LLM Client for text generation and database processing
 */
class LLMClient {
public:
    /**
     * @brief Constructor
     * @param config LLM configuration
     */
    explicit LLMClient(const LLMConfig& config = LLMConfig{});
    
    /**
     * @brief Destructor
     */
    ~LLMClient();
    
    // Prevent copying
    LLMClient(const LLMClient&) = delete;
    LLMClient& operator=(const LLMClient&) = delete;
    
    // Allow moving
    LLMClient(LLMClient&&) noexcept;
    LLMClient& operator=(LLMClient&&) noexcept;
    
    /**
     * @brief Generate text response from a prompt
     * @param prompt Input prompt
     * @param temperature Temperature parameter (0.0-2.0)
     * @param max_tokens Maximum tokens to generate
     * @return Generated text
     */
    std::string generate(
        const std::string& prompt,
        float temperature = 0.7f,
        int max_tokens = 512
    );
    
    /**
     * @brief Chat with multi-turn conversation support
     * @param messages List of conversation messages
     * @param temperature Temperature parameter
     * @param max_tokens Maximum tokens to generate
     * @return Generated response
     */
    std::string chat(
        const std::vector<ChatMessage>& messages,
        float temperature = 0.7f,
        int max_tokens = 512
    );
    
    /**
     * @brief Summarize text
     * @param text Text to summarize
     * @param max_tokens Maximum length of summary
     * @return Summary text
     */
    std::string summarize(
        const std::string& text,
        int max_tokens = 200
    );
    
    /**
     * @brief Answer a question with optional context
     * @param question Question to answer
     * @param context Optional context information
     * @return Answer
     */
    std::string answerQuestion(
        const std::string& question,
        const std::optional<std::string>& context = std::nullopt
    );
    
    /**
     * @brief Read records from database (DuckDB or SQLite)
     * @param database_path Path to database file
     * @param table_name Table name (optional)
     * @param limit Maximum number of records (optional)
     * @return Vector of database records
     */
    std::vector<DatabaseRecord> readFromDatabase(
        const std::filesystem::path& database_path,
        const std::optional<std::string>& table_name = std::nullopt,
        const std::optional<int>& limit = std::nullopt
    );
    
    /**
     * @brief Process database content with LLM
     * @param database_path Path to database file
     * @param operation Operation type ("summarize", "answer", etc.)
     * @param limit Maximum number of records to process
     * @return Vector of processed records
     */
    std::vector<DatabaseRecord> processDatabaseContent(
        const std::filesystem::path& database_path,
        const std::string& operation = "summarize",
        const std::optional<int>& limit = std::nullopt
    );
    
    /**
     * @brief Check if model is loaded
     * @return True if model is loaded
     */
    bool isModelLoaded() const { return model_loaded_; }
    
    /**
     * @brief Get model path
     * @return Path to model file
     */
    std::filesystem::path getModelPath() const { return config_.model_path; }

private:
    /**
     * @brief Load the model (lazy loading)
     */
    void loadModel();
    
    /**
     * @brief Format messages into ChatML prompt
     * @param messages Vector of chat messages
     * @return Formatted prompt string
     */
    std::string formatMessages(const std::vector<ChatMessage>& messages);
    
    /**
     * @brief Detect database type from file extension
     * @param database_path Path to database file
     * @return "duckdb" or "sqlite"
     */
    std::string detectDatabaseType(const std::filesystem::path& database_path);
    
    /**
     * @brief Read from DuckDB database
     */
    std::vector<DatabaseRecord> readFromDuckDB(
        const std::filesystem::path& db_path,
        const std::optional<std::string>& table_name,
        const std::optional<int>& limit
    );
    
    /**
     * @brief Read from SQLite database
     */
    std::vector<DatabaseRecord> readFromSQLite(
        const std::filesystem::path& db_path,
        const std::optional<std::string>& table_name,
        const std::optional<int>& limit
    );

private:
    LLMConfig config_;
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    bool model_loaded_ = false;
};

/**
 * @brief Get default model path
 * @return Default path to model file
 */
std::filesystem::path getDefaultModelPath();

} // namespace perception
