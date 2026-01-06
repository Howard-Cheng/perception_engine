// include/DatabaseTypes.h
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <ctime>

// DLL export/import macros
#ifdef _WIN32
    #ifdef DB_CLIENT_EXPORTS
        #define DB_CLIENT_API __declspec(dllexport)
    #else
        #define DB_CLIENT_API __declspec(dllimport)
    #endif
#else
    #define DB_CLIENT_API
#endif

// Suppress C4251 warning for STL types in exported classes
// This is safe for structures with STL members used across DLL boundaries
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace database {

// Type aliases
using EventId = std::string;
using DeviceId = std::string;
using SessionId = std::string;
using Timestamp = std::time_t;

// Mouse event structure
struct DB_CLIENT_API MouseEvent {
    std::time_t timestamp;
    std::string eventType;      // "LeftClick", "Copy", "TextSelection", etc.
    std::string content;         // Selected/copied text
    int posX;
    int posY;
    std::string elementType;
    
    MouseEvent() : timestamp(0), posX(0), posY(0) {}
};

// System information structure
struct DB_CLIENT_API SystemInfo {
    std::optional<int> batteryPercent;
    bool isCharging;
    std::string networkType;
    std::optional<double> locationLat;
    std::optional<double> locationLon;
    std::optional<double> cpuUsage;
    std::optional<double> memoryUsage;
    
    SystemInfo() : isCharging(false) {}
};

// Content type enumeration
enum class ContentType {
    TEXT,
    IMAGE,
    VIDEO,
    AUDIO,
    CODE,
    DOCUMENT,
    MEETING,
    UNKNOWN
};

// Domain enumeration
enum class Domain {
    WORK,
    ENTERTAINMENT,
    SOCIAL,
    SHOPPING,
    NEWS,
    EDUCATION,
    HEALTH,
    OTHER,
    UNKNOWN
};

// Raw event structure
struct DB_CLIENT_API RawEvent {
    EventId eventId;
    std::time_t timestamp;
    std::time_t createdAt;
    DeviceId deviceId;
    
    // App context
    std::string appName;
    std::optional<std::string> windowTitle;
    std::optional<std::string> url;
    
    // Content
    std::optional<std::string> screenContent;
    std::optional<std::string> screenContentHash;
    std::optional<std::string> similarScreenContent;  // NEW: Similar screen content for comparison
    
    // Multimodal data
    std::optional<std::string> voiceTranscription;
    std::optional<std::string> cameraDescription;
    
    // Interaction signals
    std::vector<MouseEvent> mouseEvents;
    int interactionCount;
    int dwellTimeSeconds;
    
    // Metadata
    std::optional<ContentType> contentType;
    std::optional<Domain> domain;
    
    // Session linking
    std::optional<SessionId> sessionId;
    
    // Status
    bool compressed;
    bool summarized;  // NEW: Indicates if the event has been summarized
    
    // System info
    SystemInfo systemInfo;
    
    RawEvent() 
        : timestamp(0)
        , createdAt(0)
        , interactionCount(0)
        , dwellTimeSeconds(0)
        , compressed(false)
        , summarized(false) 
    {}
};

// Search result structure
struct DB_CLIENT_API SearchResult {
    std::vector<RawEvent> events;
    int totalHits;
    double searchTimeMs;
    
    SearchResult() : totalHits(0), searchTimeMs(0.0) {}
};

// Index/Collection statistics
struct DB_CLIENT_API CollectionStats {
    std::string collectionName;
    int documentCount;
    int uncompressedCount;
    int compressedCount;
    long long sizeInBytes;
    
    CollectionStats() 
        : documentCount(0)
        , uncompressedCount(0)
        , compressedCount(0)
        , sizeInBytes(0) 
    {}
};

// Database type enumeration
enum class DatabaseType {
    ELASTICSEARCH,
    MONGODB,
    POSTGRESQL,
    SQLITE,
    MEILISEARCH,
    UNKNOWN
};

} // namespace database

#ifdef _MSC_VER
#pragma warning(pop)
#endif
