#include "layer0/DataIngestion.h"
#include "layer0/SchemaManager.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

namespace perception {
namespace layer0 {

RawEvent::RawEvent()
    : interactionCount(0)
    , dwellTimeSeconds(0)
    , compressed(false)
    , createdAt(utils::now()) {
}

DataIngestion::DataIngestion(const std::string& dbPath, const DeviceId& deviceId)
    : db_(nullptr)
    , deviceId_(deviceId) {
    
    // Open SQLite database
    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = "Failed to open SQLite database: ";
        error += sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error(error);
    }
    
    // Initialize schema if needed
    if (!SchemaManager::isSqliteSchemaInitialized(db_)) {
        SchemaManager::initializeSqliteSchema(db_);
    }
    
    LOG_INFO("Data ingestion initialized for device: " + deviceId);
}

DataIngestion::~DataIngestion() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

EventId DataIngestion::ingestEvent(const RawEvent& event) {
    EventId eventId = generateEventId(event);
    
    // Convert mouse events to JSON
    std::string mouseEventsJson = mouseEventsToJson(event.mouseEvents);
    
    // Prepare SQL statement
    const char* sql = R"(
        INSERT OR REPLACE INTO raw_events (
            event_id, timestamp, device_id, app_name, window_title, url,
            screen_content, screen_content_hash, mouse_events, interaction_count,
            dwell_time_seconds, voice_transcription, camera_description,
            battery_percent, is_charging, network_type, location_lat, location_lon,
            cpu_usage, memory_usage, content_type, domain, session_id, compressed, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare SQL statement: " + 
                               std::string(sqlite3_errmsg(db_)));
    }
    
    // Bind parameters
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, eventId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, utils::timestampToISO8601(event.timestamp).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, deviceId_.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, event.appName.c_str(), -1, SQLITE_TRANSIENT);
    
    if (event.windowTitle) {
        sqlite3_bind_text(stmt, idx++, event.windowTitle->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.url) {
        sqlite3_bind_text(stmt, idx++, event.url->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.screenContent) {
        sqlite3_bind_text(stmt, idx++, event.screenContent->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.screenContentHash) {
        sqlite3_bind_text(stmt, idx++, event.screenContentHash->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    sqlite3_bind_text(stmt, idx++, mouseEventsJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, event.interactionCount);
    sqlite3_bind_int(stmt, idx++, event.dwellTimeSeconds);
    
    if (event.voiceTranscription) {
        sqlite3_bind_text(stmt, idx++, event.voiceTranscription->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.cameraDescription) {
        sqlite3_bind_text(stmt, idx++, event.cameraDescription->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    // System info
    if (event.systemInfo.batteryPercent) {
        sqlite3_bind_int(stmt, idx++, *event.systemInfo.batteryPercent);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    sqlite3_bind_int(stmt, idx++, event.systemInfo.isCharging ? 1 : 0);
    sqlite3_bind_text(stmt, idx++, event.systemInfo.networkType.c_str(), -1, SQLITE_TRANSIENT);
    
    if (event.systemInfo.locationLat) {
        sqlite3_bind_double(stmt, idx++, *event.systemInfo.locationLat);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.systemInfo.locationLon) {
        sqlite3_bind_double(stmt, idx++, *event.systemInfo.locationLon);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.systemInfo.cpuUsage) {
        sqlite3_bind_double(stmt, idx++, *event.systemInfo.cpuUsage);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.systemInfo.memoryUsage) {
        sqlite3_bind_double(stmt, idx++, *event.systemInfo.memoryUsage);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    // Classification
    if (event.contentType) {
        sqlite3_bind_text(stmt, idx++, ContentTypeToString(*event.contentType).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.domain) {
        sqlite3_bind_text(stmt, idx++, DomainToString(*event.domain).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    if (event.sessionId) {
        sqlite3_bind_text(stmt, idx++, event.sessionId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }
    
    sqlite3_bind_int(stmt, idx++, event.compressed ? 1 : 0);
    sqlite3_bind_text(stmt, idx++, utils::timestampToISO8601(event.createdAt).c_str(), -1, SQLITE_TRANSIENT);
    
    // Execute
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = "Failed to insert event: " + std::string(sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        throw std::runtime_error(error);
    }
    
    sqlite3_finalize(stmt);
    
    LOG_DEBUG("Ingested event: " + eventId);
    return eventId;
}

std::vector<EventId> DataIngestion::ingestEvents(const std::vector<RawEvent>& events) {
    std::vector<EventId> eventIds;
    eventIds.reserve(events.size());
    
    // Begin transaction for batch insert
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = "Failed to begin transaction: ";
        if (errMsg) {
            error += errMsg;
            sqlite3_free(errMsg);
        }
        throw std::runtime_error(error);
    }
    
    try {
        for (const auto& event : events) {
            eventIds.push_back(ingestEvent(event));
        }
        
        // Commit transaction
        if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "Failed to commit transaction: ";
            if (errMsg) {
                error += errMsg;
                sqlite3_free(errMsg);
            }
            throw std::runtime_error(error);
        }
    } catch (...) {
        // Rollback on error
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
    
    LOG_INFO("Batch ingested " + std::to_string(events.size()) + " events");
    return eventIds;
}

int DataIngestion::getEventCount() const {
    const char* sql = "SELECT COUNT(*) FROM raw_events";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int DataIngestion::getUncompressedEventCount() const {
    const char* sql = "SELECT COUNT(*) FROM raw_events WHERE compressed = 0";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

int DataIngestion::getTodayEventCount() const {
    const char* sql = "SELECT COUNT(*) FROM raw_events WHERE DATE(timestamp) = DATE('now')";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return count;
}

EventId DataIngestion::generateEventId(const RawEvent& event) const {
    std::stringstream ss;
    ss << utils::timestampToISO8601(event.timestamp)
       << "_" << event.appName;
    if (event.windowTitle) {
        ss << "_" << *event.windowTitle;
    }
    return utils::computeMD5(ss.str());
}

std::string DataIngestion::mouseEventsToJson(const std::vector<MouseEvent>& events) const {
    json j = json::array();
    
    for (const auto& event : events) {
        json eventJson;
        eventJson["timestamp"] = utils::timestampToISO8601(event.timestamp);
        eventJson["eventType"] = event.eventType;
        eventJson["position"]["x"] = event.posX;
        eventJson["position"]["y"] = event.posY;
        eventJson["content"] = event.content;
        eventJson["elementType"] = event.elementType;
        j.push_back(eventJson);
    }
    
    return j.dump();
}

} // namespace layer0
} // namespace perception
