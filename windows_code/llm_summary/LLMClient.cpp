/**
 * @file LLMClient.cpp
 * @brief Implementation of LLM Client
 */

#include "LLMClient.h"
#include <llama.h>
#include <common.h>
#include <sampling.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <thread>

// For database support
#ifdef USE_DUCKDB
#include <duckdb.hpp>
#endif

#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

namespace perception
{

    // ============================================================================
    // Helper Functions
    // ============================================================================

    std::filesystem::path getDefaultModelPath()
    {
        // Default model path: ../../../models/phi4-aitc/Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf
        std::filesystem::path current_file(__FILE__);
        auto models_dir = current_file.parent_path().parent_path().parent_path() / "models" / "phi4-aitc";
        return models_dir / "Phi4_FP16-3.8B-Q41-g32d-1027-v1.3.1.gguf";
    }

    // ============================================================================
    // LLMClient Implementation
    // ============================================================================

    LLMClient::LLMClient(const LLMConfig &config)
        : config_(config)
    {

        // Set default model path if not provided
        if (config_.model_path.empty())
        {
            config_.model_path = getDefaultModelPath();
        }

        // Verify model file exists
        if (!std::filesystem::exists(config_.model_path))
        {
            throw std::runtime_error(
                "Model file not found: " + config_.model_path.string() +
                "\nPlease ensure the model file is downloaded to the correct location.");
        }

        if (config_.verbose)
        {
            std::cout << "LLM Client initialized, model path: "
                      << config_.model_path << std::endl;
        }
    }

    LLMClient::~LLMClient()
    {
        if (ctx_)
        {
            llama_free(ctx_);
            ctx_ = nullptr;
        }
        if (model_)
        {
            llama_free_model(model_);
            model_ = nullptr;
        }
    }

    LLMClient::LLMClient(LLMClient &&other) noexcept
        : config_(std::move(other.config_)), model_(other.model_), ctx_(other.ctx_), model_loaded_(other.model_loaded_)
    {
        other.model_ = nullptr;
        other.ctx_ = nullptr;
        other.model_loaded_ = false;
    }

    LLMClient &LLMClient::operator=(LLMClient &&other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing resources
            if (ctx_)
                llama_free(ctx_);
            if (model_)
                llama_free_model(model_);

            // Move resources
            config_ = std::move(other.config_);
            model_ = other.model_;
            ctx_ = other.ctx_;
            model_loaded_ = other.model_loaded_;

            other.model_ = nullptr;
            other.ctx_ = nullptr;
            other.model_loaded_ = false;
        }
        return *this;
    }

    void LLMClient::loadModel()
    {
        if (model_loaded_)
        {
            return;
        }

        if (config_.verbose)
        {
            std::cout << "Loading model: " << config_.model_path << std::endl;
        }

        // Initialize llama backend
        llama_backend_init();

        // Set up model parameters
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = config_.n_gpu_layers;

        // Load model
        model_ = llama_load_model_from_file(config_.model_path.string().c_str(), model_params);
        if (!model_)
        {
            throw std::runtime_error("Failed to load model: " + config_.model_path.string());
        }

        // Set up context parameters
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = config_.n_ctx;
        ctx_params.n_threads = config_.n_threads > 0 ? config_.n_threads : std::thread::hardware_concurrency();

        // Create context
        ctx_ = llama_new_context_with_model(model_, ctx_params);
        if (!ctx_)
        {
            llama_free_model(model_);
            model_ = nullptr;
            throw std::runtime_error("Failed to create llama context");
        }

        model_loaded_ = true;

        if (config_.verbose)
        {
            std::cout << "✓ Model loaded successfully" << std::endl;
        }
    }

    std::string LLMClient::generate(
        const std::string &prompt,
        float temperature,
        int max_tokens)
    {
        if (!model_loaded_)
        {
            loadModel();
        }

        // Tokenize prompt
        std::vector<llama_token> tokens;
        tokens.resize(prompt.size() + 16);
        int n_tokens = llama_tokenize(
            llama_model_get_vocab(model_),
            prompt.c_str(),
            prompt.size(),
            tokens.data(),
            tokens.size(),
            true, // add_special (add_bos)
            false // parse_special
        );

        if (n_tokens < 0)
        {
            tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(
                llama_model_get_vocab(model_),
                prompt.c_str(),
                prompt.size(),
                tokens.data(),
                tokens.size(),
                true,
                false);
        }
        tokens.resize(n_tokens);

        // Set up sampling parameters
        common_params_sampling sparams;
        sparams.temp = temperature;
        sparams.top_p = config_.top_p;
        sparams.top_k = config_.top_k;
        sparams.penalty_repeat = config_.repeat_penalty;

        common_sampler *sampling_ctx = common_sampler_init(model_, sparams);

        std::string result;
        result.reserve(max_tokens * 4); // Rough estimate

        // Generate tokens
        for (int i = 0; i < max_tokens; ++i)
        {
            // Evaluate tokens
            if (llama_decode(ctx_, llama_batch_get_one(tokens.data(), tokens.size())))
            {
                common_sampler_free(sampling_ctx);
                throw std::runtime_error("Failed to evaluate tokens");
            }

            // Sample next token
            llama_token new_token = common_sampler_sample(sampling_ctx, ctx_, -1);

            // Check for end of generation
            if (llama_token_is_eog(llama_model_get_vocab(model_), new_token))
            {
                break;
            }

            // Convert token to text
            char buf[128];
            int n = llama_token_to_piece(llama_model_get_vocab(model_), new_token, buf, sizeof(buf), 0, false);
            if (n > 0)
            {
                result.append(buf, n);
            }

            // Add token to context for next iteration
            tokens.clear();
            tokens.push_back(new_token);
        }

        common_sampler_free(sampling_ctx);

        // Trim whitespace
        result.erase(0, result.find_first_not_of(" \n\r\t"));
        result.erase(result.find_last_not_of(" \n\r\t") + 1);

        return result;
    }

    std::string LLMClient::formatMessages(const std::vector<ChatMessage> &messages)
    {
        std::ostringstream oss;

        // Format using ChatML
        for (const auto &msg : messages)
        {
            if (msg.role == "system")
            {
                oss << "<|system|>\n"
                    << msg.content << "<|end|>\n";
            }
            else if (msg.role == "user")
            {
                oss << "<|user|>\n"
                    << msg.content << "<|end|>\n";
            }
            else if (msg.role == "assistant")
            {
                oss << "<|assistant|>\n"
                    << msg.content << "<|end|>\n";
            }
        }

        // Add assistant start marker
        oss << "<|assistant|>\n";

        return oss.str();
    }

    std::string LLMClient::chat(
        const std::vector<ChatMessage> &messages,
        float temperature,
        int max_tokens)
    {
        std::string prompt = formatMessages(messages);
        return generate(prompt, temperature, max_tokens);
    }

    std::string LLMClient::summarize(const std::string &text, int max_tokens)
    {
        std::ostringstream prompt;
        prompt << "Please summarize the following text concisely:\n\n"
               << text << "\n\nSummary:";

        return generate(prompt.str(), 0.3f, max_tokens); // Lower temperature
    }

    std::string LLMClient::answerQuestion(
        const std::string &question,
        const std::optional<std::string> &context)
    {
        std::ostringstream prompt;

        if (context)
        {
            prompt << "Answer the question based on the following context:\n\n"
                   << "Context:\n"
                   << *context << "\n\n"
                   << "Question: " << question << "\n\nAnswer:";
        }
        else
        {
            prompt << "Please answer the following question:\n\n"
                   << "Question: " << question << "\n\nAnswer:";
        }

        return generate(prompt.str());
    }

    std::string LLMClient::detectDatabaseType(const std::filesystem::path &database_path)
    {
        std::string ext = database_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".duckdb")
        {
            return "duckdb";
        }
        else if (ext == ".db" || ext == ".sqlite" || ext == ".sqlite3")
        {
            return "sqlite";
        }

        throw std::runtime_error("Cannot determine database type from extension: " + ext);
    }

    std::vector<DatabaseRecord> LLMClient::readFromDatabase(
        const std::filesystem::path &database_path,
        const std::optional<std::string> &table_name,
        const std::optional<int> &limit)
    {
        if (!std::filesystem::exists(database_path))
        {
            throw std::runtime_error("Database file not found: " + database_path.string());
        }

        std::string db_type = detectDatabaseType(database_path);

        if (db_type == "duckdb")
        {
            return readFromDuckDB(database_path, table_name, limit);
        }
        else
        {
            return readFromSQLite(database_path, table_name, limit);
        }
    }

#ifdef USE_DUCKDB
    std::vector<DatabaseRecord> LLMClient::readFromDuckDB(
        const std::filesystem::path &db_path,
        const std::optional<std::string> &table_name,
        const std::optional<int> &limit)
    {
        std::vector<DatabaseRecord> records;

        try
        {
            duckdb::DuckDB db(db_path.string());
            duckdb::Connection conn(db);

            // Build query
            std::ostringstream query;
            query << "SELECT * FROM " << table_name.value_or("compressed_content");
            if (limit)
            {
                query << " LIMIT " << *limit;
            }

            auto result = conn.Query(query.str());

            while (auto row = result->Fetch())
            {
                DatabaseRecord record;
                // Parse row data...
                // This is simplified - you'd extract each column properly
                records.push_back(record);
            }
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Error reading DuckDB: " + std::string(e.what()));
        }

        return records;
    }
#else
    std::vector<DatabaseRecord> LLMClient::readFromDuckDB(
        const std::filesystem::path &,
        const std::optional<std::string> &,
        const std::optional<int> &)
    {
        throw std::runtime_error("DuckDB support not compiled. Rebuild with -DUSE_DUCKDB=ON");
    }
#endif

#ifdef USE_SQLITE
    std::vector<DatabaseRecord> LLMClient::readFromSQLite(
        const std::filesystem::path &db_path,
        const std::optional<std::string> &table_name,
        const std::optional<int> &limit)
    {
        std::vector<DatabaseRecord> records;

        sqlite3 *db = nullptr;
        if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK)
        {
            throw std::runtime_error("Cannot open SQLite database: " + db_path.string());
        }

        // Build query
        std::ostringstream query;
        query << "SELECT * FROM " << table_name.value_or("raw_events");
        if (limit)
        {
            query << " LIMIT " << *limit;
        }

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db, query.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare statement");
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            DatabaseRecord record;

            int col_count = sqlite3_column_count(stmt);
            for (int i = 0; i < col_count; ++i)
            {
                const char *col_name = sqlite3_column_name(stmt, i);
                const unsigned char *text = sqlite3_column_text(stmt, i);

                if (text)
                {
                    std::string value(reinterpret_cast<const char *>(text));

                    if (std::string(col_name) == "event_id" || std::string(col_name) == "id")
                    {
                        record.id = value;
                    }
                    else if (std::string(col_name) == "content_type")
                    {
                        record.type = value;
                    }
                    else if (std::string(col_name) == "window_title" || std::string(col_name) == "title")
                    {
                        record.title = value;
                    }
                    else if (std::string(col_name) == "url")
                    {
                        record.url = value;
                    }
                    else if (std::string(col_name) == "screen_content" || std::string(col_name) == "summary")
                    {
                        record.summary = value;
                    }
                    else if (std::string(col_name) == "timestamp")
                    {
                        record.timestamp = value;
                    }
                }
            }

            records.push_back(record);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return records;
    }
#else
    std::vector<DatabaseRecord> LLMClient::readFromSQLite(
        const std::filesystem::path &,
        const std::optional<std::string> &,
        const std::optional<int> &)
    {
        throw std::runtime_error("SQLite support not compiled. Rebuild with -DUSE_SQLITE=ON");
    }
#endif

    std::vector<DatabaseRecord> LLMClient::processDatabaseContent(
        const std::filesystem::path &database_path,
        const std::string &operation,
        const std::optional<int> &limit)
    {
        auto records = readFromDatabase(database_path, std::nullopt, limit);

        for (auto &record : records)
        {
            if (operation == "summarize" && !record.summary.empty())
            {
                try
                {
                    std::string summary = summarize(record.summary);
                    record.metadata["llm_summary"] = summary;
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error summarizing record " << record.id
                              << ": " << e.what() << std::endl;
                }
            }
            // Add more operations as needed
        }

        return records;
    }

} // namespace perception
