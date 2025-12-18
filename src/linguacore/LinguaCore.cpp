/**
 * @file LinguaCore.cpp
 * @brief Implementation of LinguaCore service
 */

#include "linguacore/LinguaCore.h"
#include "DatabaseClientFactory.h"
#include "IDatabaseClient.h"
#include "DatabaseTypes.h"
#include "LLMClient.h"
#include "VectorStore.h"
#include "QdrantClient.h"

#include <windows.h>
#include <iostream>
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
        std::cerr << "Warning: Could not open " << config_path 
                  << ", using defaults" << std::endl;
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
        } else if (current_section == "elasticsearch") {
            if (key == "host") {
                config.es_host = value;
            } else if (key == "port") {
                config.es_port = std::stoi(value);
            } else if (key == "index") {
                config.es_index = value;
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
    
    log("Initializing LinguaCore service...");
    
    // Initialize Elasticsearch client
    try {
        std::string es_url = "http://" + config_.es_host + ":" + 
                            std::to_string(config_.es_port);
        es_client_ = database::DatabaseClientFactory::createElasticsearch(es_url);
        log("Connected to Elasticsearch at " + es_url);
    } catch (const std::exception& e) {
        log("ERROR: Failed to connect to Elasticsearch: " + std::string(e.what()));
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
        log("Initialized LLM client with model: " + config_.llm_model_path);
    } catch (const std::exception& e) {
        log("ERROR: Failed to initialize LLM client: " + std::string(e.what()));
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
        
        log("Connected to Qdrant at " + qdrant_url);
    } catch (const std::exception& e) {
        log("ERROR: Failed to initialize Vector Store: " + std::string(e.what()));
        throw;
    }
    
    log("LinguaCore initialization complete");
}

LinguaCore::~LinguaCore() {
    stop();
}

bool LinguaCore::start() {
    if (running_) {
        log("WARNING: Service already running");
        return false;
    }
    
    log("Starting LinguaCore service...");
    log("Check interval: " + std::to_string(config_.check_interval_seconds) + " seconds");
    
    running_ = true;
    worker_thread_ = std::thread(&LinguaCore::serviceLoop, this);
    
    log("LinguaCore service started successfully");
    return true;
}

void LinguaCore::stop() {
    if (!running_) {
        return;
    }
    
    log("Stopping LinguaCore service...");
    running_ = false;
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    log("LinguaCore service stopped");
}

void LinguaCore::serviceLoop() {
    log("Service loop started");
    
    while (running_) {
        try {
            int processed = processOnce();
            
            if (processed > 0) {
                log("Processed " + std::to_string(processed) + " events in this cycle");
            }
            
        } catch (const std::exception& e) {
            log("ERROR in service loop: " + std::string(e.what()));
            total_errors_++;
        }
        
        // Wait for next cycle
        auto wait_until = std::chrono::steady_clock::now() + 
                         std::chrono::seconds(config_.check_interval_seconds);
        
        while (running_ && std::chrono::steady_clock::now() < wait_until) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    log("Service loop ended");
}

int LinguaCore::processOnce() {
    int total_processed = 0;
    
    while (running_) {
        // Query for unsummarized events
        auto events = queryUnsummarizedEvents(last_process_time_);
        
        if (events.empty()) {
            // No more events to process
            break;
        }
        
        log("Found " + std::to_string(events.size()) + " unsummarized event(s)");
        
        // Process based on count
        bool success = false;
        if (events.size() == 1) {
            success = processSingleEvent(events[0]);
        } else {
            success = processMultipleEvents(events);
        }
        
        if (success) {
            total_processed += events.size();
            total_processed_ += events.size();
            last_process_time_ = std::time(nullptr);
        } else {
            log("ERROR: Failed to process events");
            total_errors_++;
            break;  // Stop processing on error
        }
    }
    
    return total_processed;
}

std::vector<database::RawEvent> LinguaCore::queryUnsummarizedEvents(
    std::time_t last_processed_time) {
    
    // Build Elasticsearch query using nlohmann::json
    json query;
    query["size"] = config_.batch_size;
    query["sort"] = json::array({{{"timestamp", {{"order", "asc"}}}}});
    
    // Build bool query
    json must_conditions = json::array();
    must_conditions.push_back({{"term", {{"summarized", false}}}});
    must_conditions.push_back({{"exists", {{"field", "session_id"}}}});
    
    // Note: Removed timestamp filter to process all unsummarized events
    // regardless of when they were created
    
    query["query"] = {{"bool", {{"must", must_conditions}}}};
    
    try {
        // Call search with correct parameters: indexName, queryString, from, size
        auto result = es_client_->search(
            config_.es_index,
            query.dump(),
            0,
            config_.batch_size
        );
        
        if (result.events.empty()) {
            return {};
        }
        
        // Group by session_id and return first group
        std::map<std::string, std::vector<database::RawEvent>> grouped;
        for (const auto& event : result.events) {
            if (event.sessionId.has_value()) {
                grouped[*event.sessionId].push_back(event);
            }
        }
        
        // Return first session group
        if (!grouped.empty()) {
            return grouped.begin()->second;
        }
        
    } catch (const std::exception& e) {
        log("ERROR querying Elasticsearch: " + std::string(e.what()));
    }
    
    return {};
}

bool LinguaCore::processSingleEvent(const database::RawEvent& event) {
    log("Processing single event: " + event.eventId);
    
    // Check if screen_content is available
    if (!event.screenContent.has_value() || event.screenContent->empty()) {
        log("WARNING: Event has no screen_content, marking as summarized without processing");
        
        // Still mark the event as summarized in Elasticsearch
        std::vector<std::string> event_ids = {event.eventId};
        if (!updateSummarizedFlag(event_ids)) {
            log("ERROR: Failed to update summarized flag for empty content event");
            return false;
        }
        
        log("Successfully marked empty content event as summarized: " + event.eventId);
        return true;  // Return true since we successfully handled the case
    }
    
    const std::string& content = *event.screenContent;
    
    // Generate summary
    std::string summary = generateSummary(content);
    if (summary.empty()) {
        log("ERROR: Failed to generate summary");
        return false;
    }
    
    log("Generated summary (" + std::to_string(summary.length()) + " chars)");
    
    // Store in vector database
    std::string session_id = event.sessionId.value_or(event.eventId);
    if (!storeSummaryInVectorDB(session_id, summary, content)) {
        log("ERROR: Failed to store summary in vector DB");
        return false;
    }
    
    // Update Elasticsearch
    std::vector<std::string> event_ids = {event.eventId};
    if (!updateSummarizedFlag(event_ids)) {
        log("ERROR: Failed to update summarized flag");
        return false;
    }
    
    log("Successfully processed event: " + event.eventId);
    return true;
}

bool LinguaCore::processMultipleEvents(const std::vector<database::RawEvent>& events) {
    log("Processing " + std::to_string(events.size()) + " events with same session_id");
    
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
        log("WARNING: No similar_screen_content found in any event, skipping");
        return false;
    }
    
    log("Combined content length: " + std::to_string(content.length()) + " chars");
    
    // Generate summary
    std::string summary = generateSummary(content);
    if (summary.empty()) {
        log("ERROR: Failed to generate summary");
        return false;
    }
    
    log("Generated summary (" + std::to_string(summary.length()) + " chars)");
    
    // Store in vector database
    if (!storeSummaryInVectorDB(session_id, summary, content)) {
        log("ERROR: Failed to store summary in vector DB");
        return false;
    }
    
    // Update Elasticsearch for all events
    if (!updateSummarizedFlag(event_ids)) {
        log("ERROR: Failed to update summarized flags");
        return false;
    }
    
    log("Successfully processed " + std::to_string(events.size()) + " events");
    return true;
}

std::string LinguaCore::generateSummary(const std::string& content) {
    try {
        return llm_client_->summarize(content, config_.llm_max_tokens);
    } catch (const std::exception& e) {
        log("ERROR generating summary: " + std::string(e.what()));
        return "";
    }
}

bool LinguaCore::storeSummaryInVectorDB(
    const std::string& session_id,
    const std::string& summary,
    const std::string& original_content) {
    
    try {
        // Check if vector store is properly initialized
        if (!vector_store_) {
            log("ERROR: Vector store is null");
            return false;
        }
        
        // Check if embedding model is loaded
        if (vector_store_->getEmbeddingDimension() == 0) {
            log("ERROR: Embedding model is not loaded (dimension = 0)");
            log("ERROR: Embedding model error: " + 
                (vector_store_->getEmbeddingModel().has_value() 
                 ? vector_store_->getEmbeddingModel().value().get().getLastError()
                 : "Model not initialized"));
            return false;
        }
        
        log("Embedding model dimension: " + std::to_string(vector_store_->getEmbeddingDimension()));
        
        // Create metadata payload using vectordb::Payload
        vectordb::Payload metadata;
        metadata["session_id"] = session_id;
        metadata["summary"] = summary;
        metadata["timestamp"] = static_cast<int64_t>(std::time(nullptr));
        metadata["original_length"] = static_cast<int64_t>(original_content.length());
        metadata["summary_length"] = static_cast<int64_t>(summary.length());
        
        log("Storing in Qdrant - session_id: " + session_id + 
            ", summary length: " + std::to_string(summary.length()));
        
        // Store in Qdrant (text will be embedded automatically)
        bool success = vector_store_->storeText(summary, metadata);
        
        if (!success) {
            log("ERROR: VectorStore::storeText returned false");
            log("ERROR: Qdrant client error: " + vector_store_->getClient().getLastError());
            
            // Check collection exists
            if (!vector_store_->getClient().collectionExists(vector_store_->getCollectionName())) {
                log("ERROR: Collection does not exist: " + vector_store_->getCollectionName());
            } else {
                log("Collection exists: " + vector_store_->getCollectionName());
                
                // Get collection info
                auto collectionInfo = vector_store_->getClient().getCollectionInfo(vector_store_->getCollectionName());
                if (collectionInfo.has_value()) {
                    log("Collection info - Points: " + std::to_string(collectionInfo->pointsCount) +
                        ", Vector size: " + std::to_string(collectionInfo->vectorSize));
                }
            }
            return false;
        }
        
        log("Successfully stored summary in Qdrant");
        
        // Verify by checking collection point count
        auto collectionInfo = vector_store_->getClient().getCollectionInfo(vector_store_->getCollectionName());
        if (collectionInfo.has_value()) {
            log("Collection now has " + std::to_string(collectionInfo->pointsCount) + " points");
        }
        
        return true;
    } catch (const std::exception& e) {
        log("ERROR storing in vector DB (exception): " + std::string(e.what()));
        return false;
    }
}

bool LinguaCore::updateSummarizedFlag(const std::vector<std::string>& event_ids) {
    try {
        for (const auto& event_id : event_ids) {
            // Build update request using nlohmann::json
            json doc = {
                {"doc", {
                    {"summarized", true}
                }}
            };
            
            // Update the document
            bool success = es_client_->updateDocument(
                config_.es_index,
                event_id,
                doc.dump()
            );
            
            if (!success) {
                log("ERROR: Failed to update event " + event_id);
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        log("ERROR updating Elasticsearch: " + std::string(e.what()));
        return false;
    }
}

std::string LinguaCore::getStatistics() const {
    json stats;
    stats["running"] = running_.load();
    stats["total_processed"] = total_processed_.load();
    stats["total_errors"] = total_errors_.load();
    stats["last_process_time"] = last_process_time_;
    
    // Build config object
    stats["config"] = {
        {"check_interval_seconds", config_.check_interval_seconds},
        {"batch_size", config_.batch_size},
        {"es_host", config_.es_host},
        {"es_port", config_.es_port},
        {"es_index", config_.es_index}
    };
    
    return stats.dump(2);  // Pretty print with 2 spaces indentation
}

void LinguaCore::log(const std::string& message) const {
    if (!config_.verbose) return;
    
    // Get current time
    auto now = std::time(nullptr);
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                 std::localtime(&now));
    
    std::cout << "[" << timestamp << "] [LinguaCore] " << message << std::endl;
}

} // namespace linguacore
