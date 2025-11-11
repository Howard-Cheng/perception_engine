#include "layer1/CompressionPipeline.h"
#include "layer1/SessionDetector.h"
#include "layer1/EngagementCalculator.h"
#include "layer1/ContentExtractor.h"
#include "layer1/ContentClassifier.h"
#include "layer1/DuckDBManager.h"
#include "layer0/DataIngestion.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace perception {
namespace layer1 {

CompressionPipeline::CompressionPipeline(
    const std::string& sqlitePath,
    const std::string& duckdbPath,
    const SessionConfig& config)
    : sqlitePath_(sqlitePath)
    , duckdbPath_(duckdbPath)
    , config_(config)
    , useElasticsearch_(false) {  // Default to SQLite
    
    LOG_INFO("Initializing Compression Pipeline");
    LOG_INFO("SQLite DB: " + sqlitePath);
    LOG_INFO("DuckDB: " + duckdbPath);
    
    try {
        // Initialize components
        detector_ = std::make_unique<SessionDetector>(config);
        calculator_ = std::make_unique<EngagementCalculator>();
        extractor_ = std::make_unique<ContentExtractor>();
        classifier_ = std::make_unique<ContentClassifier>();
        duckdb_ = std::make_unique<DuckDBManager>(duckdbPath);
        
        LOG_INFO("Compression Pipeline initialized successfully (using SQLite)");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize Compression Pipeline: " + std::string(e.what()));
        throw;
    }
}

#ifdef ELASTICSEARCH_ENABLED
CompressionPipeline::CompressionPipeline(
    const std::string& sqlitePath,
    const std::string& duckdbPath,
    const SessionConfig& config,
    const std::string& esUrl)
    : sqlitePath_(sqlitePath)
    , duckdbPath_(duckdbPath)
    , config_(config)
    , useElasticsearch_(true) {  // Use Elasticsearch
    
    LOG_INFO("Initializing Compression Pipeline with Elasticsearch");
    LOG_INFO("Elasticsearch URL: " + esUrl);
    LOG_INFO("DuckDB: " + duckdbPath);
    
    try {
        // Initialize components
        detector_ = std::make_unique<SessionDetector>(config);
        calculator_ = std::make_unique<EngagementCalculator>();
        extractor_ = std::make_unique<ContentExtractor>();
        classifier_ = std::make_unique<ContentClassifier>();
        duckdb_ = std::make_unique<DuckDBManager>(duckdbPath);
        
        // Initialize Elasticsearch client
        esClient_ = std::make_unique<layer0::ElasticsearchClient>(esUrl);
        
        LOG_INFO("Compression Pipeline initialized successfully (using Elasticsearch)");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize Compression Pipeline: " + std::string(e.what()));
        throw;
    }
}
#endif

CompressionPipeline::~CompressionPipeline() = default;

int CompressionPipeline::processUncompressedEvents() {
    try {
        LOG_INFO("Starting compression pipeline...");
        
        // Step 1: Fetch uncompressed events from SQLite
        auto rawEvents = fetchUncompressedEvents();
        
        if (rawEvents.empty()) {
            LOG_INFO("No uncompressed events found");
            return 0;
        }
        
        LOG_INFO("Found " + std::to_string(rawEvents.size()) + " uncompressed events");
        
        // Step 2: Detect sessions
        auto sessions = detector_->detectInteractionSessions(rawEvents);
        LOG_INFO("Detected " + std::to_string(sessions.size()) + " interaction sessions");
        
        int compressedCount = 0;
        
        // Step 3: Process each session
        for (const auto& sessionEvents : sessions) {
            if (sessionEvents.empty()) continue;
            
            try {
                // Compress the session
                auto compressedSession = compressSession(sessionEvents);
                
                // Store to DuckDB
                auto sessionId = duckdb_->insertCompressedSession(compressedSession);
                
                // Mark events as compressed in SQLite
                markEventsAsCompressed(sessionEvents);
                
                compressedCount++;
                
                LOG_INFO("? Compressed session: " + sessionId + 
                        " (" + compressedSession.appName + ", " +
                        std::to_string(sessionEvents.size()) + " events, " +
                        "engagement: " + std::to_string(compressedSession.engagementScore) + ")");
                
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to compress session: " + std::string(e.what()));
                continue;
            }
        }
        
        LOG_INFO("Compression completed: " + std::to_string(compressedCount) + " sessions compressed");
        
        // Update statistics
        stats_.sessionsCompressed += compressedCount;
        stats_.eventsProcessed += static_cast<int>(rawEvents.size());
        
        return compressedCount;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Compression pipeline failed: " + std::string(e.what()));
        return 0;
    }
}

std::vector<layer0::RawEvent> CompressionPipeline::fetchUncompressedEvents() {
#ifdef ELASTICSEARCH_ENABLED
    if (useElasticsearch_) {
        // Use Elasticsearch
        LOG_DEBUG("Fetching uncompressed events from Elasticsearch");
        return esClient_->getUncompressedEvents(240);  // 240 hours = 10 days
    }
#endif
    
    // Use SQLite (original implementation)
    std::vector<layer0::RawEvent> events;
    
    try {
        // Open SQLite database
        sqlite3* db = nullptr;
        int rc = sqlite3_open(sqlitePath_.c_str(), &db);
        
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db)));
            return events;
        }
        
        // Query uncompressed events from last 240 hours
        const char* sql = R"(
            SELECT 
                event_id, timestamp, device_id, app_name, window_title, url,
                screen_content, screen_content_hash, mouse_events,
                interaction_count, dwell_time_seconds,
                voice_transcription, camera_description,
                battery_percent, is_charging, network_type,
                location_lat, location_lon, cpu_usage, memory_usage,
                content_type, domain, session_id
            FROM raw_events 
            WHERE compressed = 0 
            AND datetime(timestamp) >= datetime('now', '-240 hours')
            ORDER BY timestamp ASC
        )";
        
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare SQL: " + std::string(sqlite3_errmsg(db)));
            sqlite3_close(db);
            return events;
        }
        
        // Fetch results
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            layer0::RawEvent event;
            
            // Parse basic fields
            event.eventId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            event.timestamp = utils::stringToTimestamp(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))
            );
            event.deviceId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            event.appName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            // Parse optional fields
            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                event.windowTitle = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            }
            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                event.url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            }
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                event.screenContent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            }
            if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
                event.screenContentHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            }
            
            // Parse mouse events (JSON)
            if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
                try {
                    std::string mouseEventsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
                    auto j = json::parse(mouseEventsJson);
                    for (const auto& mouseEventJson : j) {
                        MouseEvent mouseEvent;
                        mouseEvent.timestamp = utils::stringToTimestamp(mouseEventJson["timestamp"].get<std::string>());
                        mouseEvent.eventType = mouseEventJson["eventType"].get<std::string>();
                        if (mouseEventJson.contains("content")) {
                            mouseEvent.content = mouseEventJson["content"].get<std::string>();
                        }
                        if (mouseEventJson.contains("pos_x")) {
                            mouseEvent.posX = mouseEventJson["pos_x"].get<int>();
                        }
                        if (mouseEventJson.contains("pos_y")) {
                            mouseEvent.posY = mouseEventJson["pos_y"].get<int>();
                        }
                        event.mouseEvents.push_back(mouseEvent);
                    }
                } catch (const std::exception& e) {
                    LOG_WARNING("Failed to parse mouse events: " + std::string(e.what()));
                }
            }
            
            // Parse metrics
            event.interactionCount = sqlite3_column_int(stmt, 9);
            event.dwellTimeSeconds = sqlite3_column_int(stmt, 10);
            
            // Parse system info
            if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
                event.systemInfo.batteryPercent = sqlite3_column_int(stmt, 13);
            }
            if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
                event.systemInfo.isCharging = sqlite3_column_int(stmt, 14) != 0;
            }
            if (sqlite3_column_type(stmt, 18) != SQLITE_NULL) {
                event.systemInfo.cpuUsage = sqlite3_column_double(stmt, 18);
            }
            if (sqlite3_column_type(stmt, 19) != SQLITE_NULL) {
                event.systemInfo.memoryUsage = sqlite3_column_double(stmt, 19);
            }
            
            events.push_back(event);
        }
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        
        LOG_INFO("Fetched " + std::to_string(events.size()) + " uncompressed events from SQLite");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to fetch uncompressed events: " + std::string(e.what()));
    }
    
    return events;
}

CompressedSession CompressionPipeline::compressSession(
    const std::vector<layer0::RawEvent>& sessionEvents) {
    
    CompressedSession compressed;
    
    if (sessionEvents.empty()) {
        throw std::runtime_error("Cannot compress empty session");
    }
    
    // Generate session ID
    std::stringstream idStream;
    const auto& firstEvent = sessionEvents.front();
    auto timestamp = utils::getTimestampSeconds(firstEvent.timestamp);
    idStream << firstEvent.deviceId << "_"
             << timestamp << "_"
             << firstEvent.appName << "_"
             << sessionEvents.size();
    compressed.sessionId = utils::computeMD5(idStream.str());
    
    // IMPORTANT: Set session_id on all events in this session
    // This is needed for markEventsAsCompressed to work properly
    for (auto& event : const_cast<std::vector<layer0::RawEvent>&>(sessionEvents)) {
        event.sessionId = compressed.sessionId;
    }
    
    // Basic session info
    compressed.deviceId = sessionEvents[0].deviceId;
    compressed.startTime = sessionEvents.front().timestamp;
    compressed.endTime = sessionEvents.back().timestamp;
    compressed.appName = sessionEvents[0].appName;
    compressed.windowTitle = sessionEvents[0].windowTitle;
    compressed.url = sessionEvents[0].url;
    
    // Calculate engagement metrics
    auto engagement = calculator_->calculate(sessionEvents);
    compressed.engagementScore = engagement.engagementScore;
    compressed.interactionCount = engagement.interactionCount;
    compressed.totalDwellTime = engagement.totalDwellTime;
    compressed.hasCopied = engagement.hasCopied;
    compressed.hasSelected = engagement.hasSelected;
    
    // Classify content (use first event's app info)
    auto classification = classifier_->classify(
        sessionEvents[0].appName,
        sessionEvents[0].url,
        sessionEvents[0].windowTitle
    );
    compressed.contentType = classification.first;
    compressed.domain = classification.second;
    
    // Extract high attention content
    auto highAttention = extractor_->extractHighAttentionContent(sessionEvents);
    compressed.copiedContent = highAttention.copiedContent;
    compressed.selectedText = highAttention.selectedText;
    
    // Extract entities
    auto entities = extractor_->extractEntities(sessionEvents, highAttention);
    compressed.numbers = entities.numbers;
    compressed.dates = entities.dates;
    compressed.urls = entities.urls;
    compressed.emails = entities.emails;
    
    // Extract metadata
    auto metadata = extractor_->extractContentSpecificMetadata(compressed.contentType, sessionEvents);
    compressed.metadataJson = metadataToJson(metadata);
    
    // Calculate system info averages
    double totalCpu = 0, totalMem = 0;
    int cpuCount = 0, memCount = 0, batterySum = 0, batteryCount = 0;
    
    for (const auto& event : sessionEvents) {
        if (event.systemInfo.cpuUsage.has_value()) {
            totalCpu += event.systemInfo.cpuUsage.value();
            cpuCount++;
        }
        if (event.systemInfo.memoryUsage.has_value()) {
            totalMem += event.systemInfo.memoryUsage.value();
            memCount++;
        }
        if (event.systemInfo.batteryPercent.has_value()) {
            batterySum += event.systemInfo.batteryPercent.value();
            batteryCount++;
        }
    }
    
    if (cpuCount > 0) compressed.avgCpuUsage = totalCpu / cpuCount;
    if (memCount > 0) compressed.avgMemoryUsage = totalMem / memCount;
    if (batteryCount > 0) compressed.avgBatteryPercent = batterySum / batteryCount;
    
    // Compress content using LLM (placeholder - to be implemented)
    compressed.compressedSummary = compressContentWithLLM(sessionEvents, compressed.engagementScore);
    compressed.keyPoints = extractKeyPoints(sessionEvents);
    compressed.summaryTokenCount = static_cast<int>(compressed.compressedSummary.length() / 4); // Rough estimate
    
    compressed.createdAt = utils::now();
    compressed.compressedAt = utils::now();
    
    return compressed;
}

std::string CompressionPipeline::compressContentWithLLM(
    const std::vector<layer0::RawEvent>& events,
    double engagementScore) {
    
    // TODO: Implement LLM compression using llama.cpp or similar
    // For now, create a simple summary
    
    std::ostringstream summary;
    summary << "Session with " << events.size() << " events. ";
    summary << "Engagement score: " << std::fixed << std::setprecision(2) << engagementScore << ". ";
    
    if (!events.empty()) {
        summary << "Application: " << events[0].appName << ". ";
        if (events[0].windowTitle.has_value()) {
            summary << "Window: " << events[0].windowTitle.value() << ". ";
        }
    }
    
    // Placeholder for LLM-generated summary
    summary << "[LLM compression not yet implemented]";
    
    return summary.str();
}

std::vector<std::string> CompressionPipeline::extractKeyPoints(
    const std::vector<layer0::RawEvent>& events) {
    
    std::vector<std::string> keyPoints;
    
    // Extract key points from high attention content
    for (const auto& event : events) {
        for (const auto& mouseEvent : event.mouseEvents) {
            if (mouseEvent.eventType == "Copy" && !mouseEvent.content.empty()) {
                keyPoints.push_back(mouseEvent.content);
            }
        }
        
        if (keyPoints.size() >= 5) break; // Limit to top 5 key points
    }
    
    return keyPoints;
}

std::string CompressionPipeline::metadataToJson(const ContentMetadata& metadata) const {
    json j;
    
    if (metadata.sender.has_value()) j["sender"] = metadata.sender.value();
    if (metadata.subject.has_value()) j["subject"] = metadata.subject.value();
    if (metadata.fileName.has_value()) j["fileName"] = metadata.fileName.value();
    if (metadata.filePath.has_value()) j["filePath"] = metadata.filePath.value();
    if (metadata.projectName.has_value()) j["projectName"] = metadata.projectName.value();
    if (metadata.repoUrl.has_value()) j["repoUrl"] = metadata.repoUrl.value();
    if (metadata.docType.has_value()) j["docType"] = metadata.docType.value();
    if (metadata.meetingTitle.has_value()) j["meetingTitle"] = metadata.meetingTitle.value();
    if (metadata.url.has_value()) j["url"] = metadata.url.value();
    if (metadata.title.has_value()) j["title"] = metadata.title.value();
    
    for (const auto& [key, value] : metadata.additionalFields) {
        j[key] = value;
    }
    
    return j.dump();
}

void CompressionPipeline::markEventsAsCompressed(const std::vector<layer0::RawEvent>& events) {
    try {
        if (events.empty()) {
            return;
        }
        
#ifdef ELASTICSEARCH_ENABLED
        if (useElasticsearch_) {
            // Use Elasticsearch
            std::string sessionId;
            if (events[0].sessionId.has_value()) {
                sessionId = events[0].sessionId.value();
            } else {
                LOG_WARNING("Events do not have session_id set");
                return;
            }
            
            std::vector<std::string> eventIds;
            for (const auto& event : events) {
                eventIds.push_back(event.eventId);
            }
            
            if (esClient_->markEventsAsCompressed(eventIds, sessionId)) {
                LOG_DEBUG("Marked " + std::to_string(events.size()) + 
                         " events as compressed in Elasticsearch with session_id: " + sessionId);
            }
            return;
        }
#endif
        
        // Use SQLite (original implementation)
        sqlite3* db = nullptr;
        int rc = sqlite3_open(sqlitePath_.c_str(), &db);
        
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db)));
            return;
        }
        
        // Begin transaction
        sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
        
        // Prepare update statement - update both compressed and session_id
        const char* sql = "UPDATE raw_events SET compressed = 1, session_id = ? WHERE event_id = ?";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        
        // Get session_id from first event (all events in this batch belong to same session)
        std::string sessionId;
        if (events[0].sessionId.has_value()) {
            sessionId = events[0].sessionId.value();
        } else {
            LOG_WARNING("Events do not have session_id set");
            // Fallback to old behavior if session_id is not set
            sqlite3_finalize(stmt);
            const char* fallbackSql = "UPDATE raw_events SET compressed = 1 WHERE event_id = ?";
            sqlite3_prepare_v2(db, fallbackSql, -1, &stmt, nullptr);
            
            for (const auto& event : events) {
                sqlite3_bind_text(stmt, 1, event.eventId.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            
            LOG_DEBUG("Marked " + std::to_string(events.size()) + " events as compressed (no session_id)");
            return;
        }
        
        // Update each event with session_id and compressed flag
        for (const auto& event : events) {
            sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);  // session_id
            sqlite3_bind_text(stmt, 2, event.eventId.c_str(), -1, SQLITE_TRANSIENT);  // event_id
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        
        sqlite3_finalize(stmt);
        
        // Commit transaction
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
        
        sqlite3_close(db);
        
        LOG_DEBUG("Marked " + std::to_string(events.size()) + 
                 " events as compressed with session_id: " + sessionId);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to mark events as compressed: " + std::string(e.what()));
    }
}

int CompressionPipeline::cleanupOldCompressedSessions(int retentionDays) {
    try {
        auto cutoffTime = utils::now() - std::chrono::hours(24 * retentionDays);
        return duckdb_->deleteSessionsOlderThan(cutoffTime);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to cleanup old sessions: " + std::string(e.what()));
        return 0;
    }
}

CompressionStatistics CompressionPipeline::getStatistics() const {
    return stats_;
}

} // namespace layer1
} // namespace perception
