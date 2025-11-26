#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <optional>

namespace perception {

// Forward declarations
class RawEvent;
class Session;
class CompressedContent;

// Type aliases
using Timestamp = std::chrono::system_clock::time_point;
using EventId = std::string;
using SessionId = std::string;
using ContentId = std::string;
using DeviceId = std::string;

// Content type enumeration
enum class ContentType {
    EMAIL,
    CHAT,
    WEB_ARTICLE,
    WEB_PAGE,
    CODE,
    DOCUMENT,
    MEETING,
    VIDEO,
    SOCIAL,
    RESEARCH_PAPER,
    UNKNOWN
};

// Domain enumeration
enum class Domain {
    SYSTEM,
    WORK,
    ENTERTAINMENT,
    LIFE,
    INTERACTION
};

// Mouse event structure
struct MouseEvent {
    Timestamp timestamp;
    std::string eventType;      // "LeftClick", "Copy", "TextSelection", etc.
    int posX;
    int posY;
    std::string content;        // Selected/copied text
    std::string elementType;    // "Button", "Text", "Link", etc.
};

// System information structure
struct SystemInfo {
    std::optional<int> batteryPercent;
    bool isCharging;
    std::string networkType;
    std::optional<double> locationLat;
    std::optional<double> locationLon;
    std::optional<double> cpuUsage;
    std::optional<double> memoryUsage;
};

// Engagement metrics structure
struct EngagementMetrics {
    double engagementScore;     // 0.0 to 1.0
    int interactionCount;
    int totalDwellTime;         // seconds
    bool hasCopied;
    bool hasSelected;
    int copiedCount;
    int selectionCount;
};

// High attention content structure
struct HighAttentionContent {
    std::vector<std::string> copiedContent;
    std::vector<std::string> selectedText;
    std::vector<std::string> clickedElements;
};

// Extracted entities structure
struct ExtractedEntities {
    std::vector<std::string> numbers;
    std::vector<std::string> dates;
    std::vector<std::string> urls;
    std::vector<std::string> emails;
};

// Content metadata structure
struct ContentMetadata {
    std::optional<std::string> sender;
    std::optional<std::string> subject;
    std::optional<std::string> fileName;
    std::optional<std::string> filePath;
    std::optional<std::string> projectName;
    std::optional<std::string> repoUrl;
    std::optional<std::string> docType;
    std::optional<std::string> meetingTitle;
    std::optional<std::string> url;
    std::optional<std::string> title;
    std::map<std::string, std::string> additionalFields;
};

// Compressed summary structure
struct CompressedSummary {
    std::string summary;
    std::vector<std::string> keyPoints;
    int tokenCount;
};

// Utility functions for enum conversion
std::string ContentTypeToString(ContentType type);
ContentType StringToContentType(const std::string& str);
std::string DomainToString(Domain domain);
Domain StringToDomain(const std::string& str);

} // namespace perception
