/**
 * @file LinguaCore.cpp
 * @brief Implementation of LinguaCore service
 */

#include "pe_base/logger.h"  // Add logger first
#include "linguacore/LinguaCore.h"
#include "DatabaseClientFactory.h"
#include "IDatabaseClient.h"
#include "DatabaseTypes.h"
#include "LLMClient.h"
#include "VectorStore.h"
#include "QdrantClient.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;

namespace linguacore {

// ============================================================================
// Configuration Loading
// ============================================================================

LinguaCoreConfig loadConfiguration(const std::string& config_path) {
    LinguaCoreConfig config;
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        PE_WARN("Could not open " << config_path << ", using defaults");
        return config;
    }
    
    std::string line;
    std::string current_section;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key=value
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Process based on section
        if (current_section == "linguacore") {
            if (key == "check_interval_seconds") {
                config.check_interval_seconds = std::stoi(value);
            } else if (key == "batch_size") {
                config.batch_size = std::stoi(value);
            } else if (key == "verbose") {
                config.verbose = (value == "true" || value == "1");
            }
        } else if (current_section == "postgresql" || current_section == "postgres") {
            // UPDATED: Changed from "elasticsearch" to "postgresql"
            if (key == "host") {
                config.pg_host = value;
            } else if (key == "port") {
                config.pg_port = std::stoi(value);
            } else if (key == "dbname" || key == "database") {
                config.pg_dbname = value;
            } else if (key == "user" || key == "username") {
                config.pg_user = value;
            } else if (key == "password") {
                config.pg_password = value;
            } else if (key == "table") {
                config.pg_table = value;
            }
        } else if (current_section == "llm") {
            if (key == "model_path") {
                config.llm_model_path = value;
            } else if (key == "max_tokens") {
                config.llm_max_tokens = std::stoi(value);
            } else if (key == "temperature") {
                config.llm_temperature = std::stof(value);
            }
        } else if (current_section == "qdrant") {
            if (key == "host") {
                config.qdrant_host = value;
            } else if (key == "port") {
                config.qdrant_port = std::stoi(value);
            } else if (key == "collection") {
                config.qdrant_collection = value;
            }
        } else if (current_section == "embedding") {
            if (key == "model_path") {
                config.embedding_model_path = value;
            }
        }
    }
    
    return config;
}

// ============================================================================
// LinguaCore Implementation
// ============================================================================

LinguaCore::LinguaCore(const LinguaCoreConfig& config)
    : config_(config) {
    
    PE_INFO("Initializing LinguaCore service...");
    
    // Initialize PostgreSQL client (UPDATED: Changed from Elasticsearch)
    try {
        // Build PostgreSQL connection string
        std::string pg_conn_str = "host=" + config_.pg_host + 
                                 " port=" + std::to_string(config_.pg_port) +
                                 " dbname=" + config_.pg_dbname +
                                 " user=" + config_.pg_user;
        
        if (!config_.pg_password.empty()) {
            pg_conn_str += " password=" + config_.pg_password;
        }
        
        // Create PostgreSQL client (only takes connection string parameter)
        pg_client_ = database::DatabaseClientFactory::createPostgreSQL(pg_conn_str);
        
        // Initialize the table (PostgreSQL client handles table name in initializeCollection)
        pg_client_->initializeCollection(config_.pg_table);
        
        PE_INFO("Connected to PostgreSQL at " << config_.pg_host << ":" << 
            config_.pg_port << "/" << config_.pg_dbname);
    } catch (const std::exception& e) {
        PE_ERROR("Failed to connect to PostgreSQL: " << e.what());
        throw;
    }
    
    // Initialize LLM client
    try {
        perception::LLMConfig llm_config;
        llm_config.model_path = config_.llm_model_path;
        llm_config.temperature = config_.llm_temperature;
        llm_config.max_tokens = config_.llm_max_tokens;
        llm_config.verbose = config_.verbose;
        
        llm_client_ = std::make_unique<perception::LLMClient>(llm_config);
        PE_INFO("Initialized LLM client with model: " << config_.llm_model_path);
    } catch (const std::exception& e) {
        PE_ERROR("Failed to initialize LLM client: " << e.what());
        throw;
    }
    
    // Initialize Vector Store (Qdrant)
    try {
        std::string qdrant_url = "http://" + config_.qdrant_host + ":" + 
                                std::to_string(config_.qdrant_port);
        
        // Create VectorStore with collection name, embedding model path, and Qdrant config
        vectordb::QdrantClient::Config qdrant_config = 
            vectordb::QdrantClient::Config::remote(qdrant_url);

        vector_store_ = std::make_unique<vectordb::VectorStore>(
            config_.qdrant_collection,
            config_.embedding_model_path,
            qdrant_config
        );
        
        if (!vector_store_->initialize()) {
            throw std::runtime_error("Failed to initialize VectorStore");
        }
        
        PE_INFO("Connected to Qdrant at " << qdrant_url);
    } catch (const std::exception& e) {
        PE_ERROR("Failed to initialize Vector Store: " << e.what());
        throw;
    }
    
    PE_INFO("LinguaCore initialization complete");
}

LinguaCore::~LinguaCore() {
    stop();
}

bool LinguaCore::start() {
    if (running_) {
        PE_WARN("Service already running");
        return false;
    }
    
    PE_INFO("Starting LinguaCore service...");
    PE_INFO("Check interval: " << config_.check_interval_seconds << " seconds");
    
    running_ = true;
    worker_thread_ = std::thread(&LinguaCore::serviceLoop, this);
    
    PE_INFO("LinguaCore service started successfully");
    return true;
}

void LinguaCore::stop() {
    if (!running_) {
        return;
    }
    
    PE_INFO("Stopping LinguaCore service...");
    running_ = false;
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    PE_INFO("LinguaCore service stopped");
}

void LinguaCore::serviceLoop() {
    PE_INFO("Service loop started");
    
    while (running_) {
        try {
            int processed = processOnce();
            
            if (processed > 0) {
                PE_INFO("Processed " << processed << " events in this cycle");
            }
            
        } catch (const std::exception& e) {
            PE_ERROR("ERROR in service loop: " << e.what());
            total_errors_++;
        }
        
        // Wait for next cycle
        auto wait_until = std::chrono::steady_clock::now() + 
                         std::chrono::seconds(config_.check_interval_seconds);
        
        while (running_ && std::chrono::steady_clock::now() < wait_until) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    PE_INFO("Service loop ended");
}

int LinguaCore::processOnce() {
    int total_processed = 0;
    
    // ? FIX: Only process ONE batch per call to prevent re-querying before ES refresh
    // The while loop in serviceLoop() will call processOnce() again after the interval
    
    // Query for unsummarized events
    auto events = queryUnsummarizedEvents(last_process_time_);
    
    if (events.empty()) {
        // No events to process
        return 0;
    }
    
    PE_INFO("Found " << events.size() << " unsummarized event(s)");
    
    // Process based on count
    bool success = false;
    if (events.size() == 1) {
        success = processSingleEvent(events[0]);
    } else {
        success = processMultipleEvents(events);
    }
    
    if (success) {
        total_processed = events.size();
        total_processed_ += events.size();
        last_process_time_ = std::time(nullptr);
    } else {
        PE_ERROR("Failed to process events");
        total_errors_++;
    }
    
    return total_processed;
}

std::vector<database::RawEvent> LinguaCore::queryUnsummarizedEvents(
    std::time_t last_processed_time) {
    
    // Build PostgreSQL query using nlohmann::json
    // OPTIMIZED: Get all events from one session, ordered by timestamp
    json query;
    query["keyword"] = "";  // Empty keyword to get all events
    query["startTime"] = 0LL;  // From beginning (milliseconds)
    
    // FIX: Convert to milliseconds (PostgreSQL client expects milliseconds)
    query["endTime"] = static_cast<long long>(std::time(nullptr)) * 1000LL;  // Current time in milliseconds
    
    // OPTIMIZED: Request more results to get full session
    query["size"] = config_.batch_size * 10;  // Multiply by 10 to get full session
    
    // FIX: Use includeSummarized to filter for unsummarized events at database level
    query["includeCompressed"] = true;    // Include compressed events (we want grouped events)
    query["includeSummarized"] = false;   // Exclude summarized events (only unsummarized)
    
    // NEW: Sort by timestamp ascending (oldest first) for chronological processing
    query["sortOrder"] = "asc";  // "asc" for oldest first, "desc" for newest first
    
    try {
        // Call search with PostgreSQL client
        // PostgreSQL returns events ordered by timestamp ASC (oldest first)
        auto result = pg_client_->search(
            config_.pg_table,
            query.dump(),
            0,
            config_.batch_size * 10  // Get more results
        );
        
        if (result.events.empty()) {
            return {};
        }
        
        // FIX: Additional client-side filter for events with session_id and compressed=true
        // PostgreSQL returns: compressed=true, summarized=false (from database filter)
        // We additionally require: has session_id
        std::vector<database::RawEvent> unsummarized;
        for (const auto& event : result.events) {
            // Only process events that:
            // 1. Have a session_id (required for grouping)
            // 2. Are compressed (already filtered by database, but double-check)
            // 3. Are NOT summarized (already filtered by database)
            if (event.sessionId.has_value() && event.compressed) {
                unsummarized.push_back(event);
            }
        }
        
        if (unsummarized.empty()) {
            return {};
        }
        
        // OPTIMIZED: Since database already sorted by timestamp ASC,
        // the first event is the earliest one, so we just need its session_id
        std::string target_session_id = *unsummarized[0].sessionId;
        
        PE_INFO("Selected session (earliest event): " << target_session_id);
        
        // Collect all events with the same session_id (maintain chronological order)
        std::vector<database::RawEvent> session_events;
        for (const auto& event : unsummarized) {
            if (event.sessionId.has_value() && *event.sessionId == target_session_id) {
                session_events.push_back(event);
            }
        }
        
        PE_INFO("Collected " << session_events.size() << 
            " event(s) for session " << target_session_id);

        // Events are already in chronological order from database query
        return session_events;
        
    } catch (const std::exception& e) {
        PE_ERROR("ERROR querying PostgreSQL: " << e.what());
    }
    
    return {};
}

bool LinguaCore::processSingleEvent(const database::RawEvent& event) {
    PE_INFO("Processing single event: " << event.eventId);
    
    // Check if screen_content is available
    if (!event.screenContent.has_value() || event.screenContent->empty()) {
        PE_WARN("Event has no screen_content, marking as summarized without processing");
        
        // Still mark the event as summarized in PostgreSQL
        std::vector<std::string> event_ids = {event.eventId};
        if (!updateSummarizedFlag(event_ids)) {
            PE_ERROR("Failed to update summarized flag for empty content event");
            return false;
        }
        
        PE_INFO("Successfully marked empty content event as summarized: " << event.eventId);
        return true;  // Return true since we successfully handled the case
    }
    
    const std::string& content = *event.screenContent;
    
    // Generate summary
    std::string summary = generateSummary(content);
    if (summary.empty()) {
        PE_ERROR("Failed to generate summary");
        return false;
    }
    
    PE_INFO("Generated summary (" << summary.length() << " chars)");
    
    // Store in vector database with created_at timestamp
    std::string session_id = event.sessionId.value_or(event.eventId);
    if (!storeSummaryInVectorDB(session_id, summary, content, event.createdAt)) {
        PE_ERROR("Failed to store summary in vector DB");
        return false;
    }
    
    // Update PostgreSQL (UPDATED: Changed from Elasticsearch)
    std::vector<std::string> event_ids = {event.eventId};
    if (!updateSummarizedFlag(event_ids)) {
        PE_ERROR("Failed to update summarized flag");
        return false;
    }
    
    PE_INFO("Successfully processed event: " << event.eventId);
    return true;
}

bool LinguaCore::processMultipleEvents(const std::vector<database::RawEvent>& events) {
    PE_INFO("Processing " << events.size() << " events with same session_id");
    
    // Extract session_id (all events should have the same one)
    std::string session_id = events[0].sessionId.value_or(events[0].eventId);
    
    // Combine similar_screen_content from all events
    std::ostringstream combined_content;
    std::vector<std::string> event_ids;
    
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        event_ids.push_back(event.eventId);
        
        if (event.similarScreenContent.has_value() && !event.similarScreenContent->empty()) {
            if (i > 0) {
                combined_content << "\n\n--- Next Event ---\n\n";
            }
            combined_content << *event.similarScreenContent;
        }
    }
    
    std::string content = combined_content.str();
    
    if (content.empty()) {
        PE_WARN("No similar_screen_content found in any event, skipping");
        return false;
    }
    
    PE_INFO("Combined content length: " << content.length() << " chars");
    
    // Generate summary
    std::string summary = generateSummary(content);
    if (summary.empty()) {
        PE_ERROR("Failed to generate summary");
        return false;
    }
    
    PE_INFO("Generated summary (" << summary.length() << " chars)");
    
    // Store in vector database with first event's created_at timestamp
    if (!storeSummaryInVectorDB(session_id, summary, content, events[0].createdAt)) {
        PE_ERROR("Failed to store summary in vector DB");
        return false;
    }
    
    // Update Elasticsearch for all events
    if (!updateSummarizedFlag(event_ids)) {
        PE_ERROR("Failed to update summarized flags");
        return false;
    }
    
    PE_INFO("Successfully processed " << events.size() << " events");
    return true;
}

std::string LinguaCore::generateSummary(const std::string& content) {
    try {
        return llm_client_->summarize(content, config_.llm_max_tokens);
    } catch (const std::exception& e) {
        PE_ERROR("ERROR generating summary: " << e.what());
        return "";
    }
}

bool LinguaCore::storeSummaryInVectorDB(
    const std::string& session_id,
    const std::string& summary,
    const std::string& original_content,
    std::time_t created_at) {
    
    try {
        // Check if vector store is properly initialized
        if (!vector_store_) {
            PE_ERROR("Vector store is null");
            return false;
        }
        
        // Check if embedding model is loaded
        if (vector_store_->getEmbeddingDimension() == 0) {
            PE_ERROR("Embedding model is not loaded (dimension = 0)");
            PE_ERROR("Embedding model error: " << 
                (vector_store_->getEmbeddingModel().has_value() 
                 ? vector_store_->getEmbeddingModel().value().get().getLastError()
                 : "Model not initialized"));
            return false;
        }
        
        PE_INFO("Embedding model dimension: " << vector_store_->getEmbeddingDimension());
        
        // Create metadata payload using vectordb::Payload
        vectordb::Payload metadata;
        metadata["session_id"] = session_id;
        metadata["summary"] = summary;
        metadata["timestamp"] = static_cast<int64_t>(std::time(nullptr));
        metadata["created_at"] = static_cast<int64_t>(created_at);
        metadata["original_length"] = static_cast<int64_t>(original_content.length());
        metadata["summary_length"] = static_cast<int64_t>(summary.length());
        
        PE_INFO("Storing in Qdrant - session_id: " << session_id << 
            ", summary length: " << summary.length() <<
            ", created_at: " << created_at);
        
        // ? FIX: Use session_id as point_id to prevent duplicates
        // If the same session is processed multiple times, it will update the existing point
        // instead of creating a new one
        bool success = vector_store_->storeText(summary, metadata, session_id);
        
        if (!success) {
            PE_ERROR("VectorStore::storeText returned false");
            PE_ERROR("Qdrant client error: " << vector_store_->getClient().getLastError());
            
            // Check collection exists
            if (!vector_store_->getClient().collectionExists(vector_store_->getCollectionName())) {
                PE_ERROR("Collection does not exist: " << vector_store_->getCollectionName());
            } else {
                PE_INFO("Collection exists: " << vector_store_->getCollectionName());
                
                // Get collection info
                auto collectionInfo = vector_store_->getClient().getCollectionInfo(vector_store_->getCollectionName());
                if (collectionInfo.has_value()) {
                    PE_INFO("Collection info - Points: " << collectionInfo->pointsCount <<
                        ", Vector size: " << collectionInfo->vectorSize);
                }
            }
            return false;
        }
        
        PE_INFO("Successfully stored summary in Qdrant");
        
        // Verify by checking collection point count
        auto collectionInfo = vector_store_->getClient().getCollectionInfo(vector_store_->getCollectionName());
        if (collectionInfo.has_value()) {
            PE_INFO("Collection now has " << collectionInfo->pointsCount << " points");
        }
        
        return true;
    } catch (const std::exception& e) {
        PE_ERROR("ERROR storing in vector DB (exception): " << e.what());
        return false;
    }
}

bool LinguaCore::updateSummarizedFlag(const std::vector<std::string>& event_ids) {
    try {
        // UPDATED: Changed from Elasticsearch update to PostgreSQL update
        for (const auto& event_id : event_ids) {
            // Build update request using nlohmann::json
            // FIX: Should update 'summarized' field, not 'compressed'
            json doc = {
                {"doc", {
                    {"summarized", true}
                }}
            };
            
            // Update the document in PostgreSQL
            bool success = pg_client_->updateDocument(
                config_.pg_table,
                event_id,
                doc.dump()
            );
            
            if (!success) {
                PE_ERROR("Failed to update event " << event_id);
                return false;
            }
        }
        
        // PostgreSQL changes are immediately visible (ACID properties)
        // No need for explicit refresh like Elasticsearch
        PE_INFO("Successfully updated " << event_ids.size() << 
            " event(s) as summarized in PostgreSQL");
        
        return true;
    } catch (const std::exception& e) {
        PE_ERROR("ERROR updating PostgreSQL: " << e.what());
        return false;
    }
}

std::string LinguaCore::getStatistics() const {
    json stats;
    stats["running"] = running_.load();
    stats["total_processed"] = total_processed_.load();
    stats["total_errors"] = total_errors_.load();
    stats["last_process_time"] = last_process_time_;
    
    // Build config object (UPDATED: Changed from es_* to pg_*)
    stats["config"] = {
        {"check_interval_seconds", config_.check_interval_seconds},
        {"batch_size", config_.batch_size},
        {"pg_host", config_.pg_host},
        {"pg_port", config_.pg_port},
        {"pg_dbname", config_.pg_dbname},
        {"pg_table", config_.pg_table}
    };
    
    return stats.dump(2);  // Pretty print with 2 spaces indentation
}

} // namespace linguacore
