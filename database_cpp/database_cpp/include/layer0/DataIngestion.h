#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/DatabaseConfig.h"

// Forward declaration for SQLite
struct sqlite3;

namespace perception {
namespace layer0 {

// Raw event structure for Layer 0
class RawEvent {
public:
    EventId eventId;
    Timestamp timestamp;
    DeviceId deviceId;
    
    // App context
    std::string appName;
    std::optional<std::string> windowTitle;
    std::optional<std::string> url;
    
    // Content
    std::optional<std::string> screenContent;
    std::optional<std::string> screenContentHash;
    
    // Interaction signals
    std::vector<MouseEvent> mouseEvents;
    int interactionCount;
    int dwellTimeSeconds;
    
    // Audio/Camera
    std::optional<std::string> voiceTranscription;
    std::optional<std::string> cameraDescription;
    
    // System info
    SystemInfo systemInfo;
    
    // Classification (assigned during compression)
    std::optional<ContentType> contentType;
    std::optional<Domain> domain;
    
    // Session linking
    std::optional<SessionId> sessionId;
    
    // Status
    bool compressed;
    Timestamp createdAt;
    
    RawEvent();
};

// Data ingestion class for Layer 0
class DataIngestion {
public:
    explicit DataIngestion(const std::string& dbPath, const DeviceId& deviceId);
    ~DataIngestion();
    
    // Delete copy constructor and assignment
    DataIngestion(const DataIngestion&) = delete;
    DataIngestion& operator=(const DataIngestion&) = delete;
    
    // Ingest a raw event
    EventId ingestEvent(const RawEvent& event);
    
    // Batch ingest
    std::vector<EventId> ingestEvents(const std::vector<RawEvent>& events);
    
    // Get statistics
    int getEventCount() const;
    int getUncompressedEventCount() const;
    int getTodayEventCount() const;
    
private:
    EventId generateEventId(const RawEvent& event) const;
    std::string mouseEventsToJson(const std::vector<MouseEvent>& events) const;
    
    sqlite3* db_;
    DeviceId deviceId_;
};

} // namespace layer0
} // namespace perception
