// src/ElasticsearchClient.cpp
#include "ElasticsearchClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>

using json = nlohmann::json;

namespace elasticsearch {

// Helper: CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper: Convert time_t to ISO 8601 string
static std::string timestampToISO8601(std::time_t timestamp) {
    std::tm tm_val;
#ifdef _WIN32
    gmtime_s(&tm_val, &timestamp);
#else
    gmtime_r(&timestamp, &tm_val);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    oss << ".000Z";
    return oss.str();
}

// Helper: Convert ISO 8601 string to time_t
static std::time_t iso8601ToTimestamp(const std::string& iso8601) {
    std::tm tm_val = {};
    std::istringstream ss(iso8601);
    ss >> std::get_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm_val);
}

// Helper: Content type to string
static std::string contentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::TEXT: return "TEXT";
        case ContentType::IMAGE: return "IMAGE";
        case ContentType::VIDEO: return "VIDEO";
        case ContentType::AUDIO: return "AUDIO";
        case ContentType::CODE: return "CODE";
        case ContentType::DOCUMENT: return "DOCUMENT";
        default: return "UNKNOWN";
    }
}

// Helper: Domain to string
static std::string domainToString(Domain domain) {
    switch (domain) {
        case Domain::WORK: return "WORK";
        case Domain::ENTERTAINMENT: return "ENTERTAINMENT";
        case Domain::SOCIAL: return "SOCIAL";
        case Domain::SHOPPING: return "SHOPPING";
        case Domain::NEWS: return "NEWS";
        case Domain::EDUCATION: return "EDUCATION";
        case Domain::HEALTH: return "HEALTH";
        case Domain::OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

// Implementation class (Pimpl pattern)
class ElasticsearchClient::Impl {
public:
    std::string esUrl_;
    
    explicit Impl(const std::string& esUrl) : esUrl_(esUrl) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    
    ~Impl() {
        curl_global_cleanup();
    }
    
    bool httpRequest(const std::string& method,
                    const std::string& endpoint,
                    const std::string& body,
                    std::string& response) {
        CURL* curl = curl_easy_init();
        if (!curl) return false;
        
        std::string url = esUrl_ + endpoint;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        return (res == CURLE_OK) && (http_code >= 200 && http_code < 300);
    }
    
    std::string eventToJson(const RawEvent& event) {
        json j = {
            {"event_id", event.eventId},
            {"timestamp", timestampToISO8601(event.timestamp)},
            {"created_at", timestampToISO8601(event.createdAt)},
            {"device_id", event.deviceId},
            {"app_name", event.appName},
            {"interaction_count", event.interactionCount},
            {"dwell_time_seconds", event.dwellTimeSeconds},
            {"compressed", event.compressed}
        };
        
        // Optional fields
        if (event.windowTitle) j["window_title"] = *event.windowTitle;
        if (event.url) j["url"] = *event.url;
        if (event.screenContent) j["screen_content"] = *event.screenContent;
        if (event.screenContentHash) j["screen_content_hash"] = *event.screenContentHash;
        if (event.voiceTranscription) j["voice_transcription"] = *event.voiceTranscription;
        if (event.cameraDescription) j["camera_description"] = *event.cameraDescription;
        if (event.sessionId) j["session_id"] = *event.sessionId;
        if (event.contentType) j["content_type"] = contentTypeToString(*event.contentType);
        if (event.domain) j["domain"] = domainToString(*event.domain);
        
        // Mouse events
        if (!event.mouseEvents.empty()) {
            json mouseEventsArray = json::array();
            for (const auto& me : event.mouseEvents) {
                mouseEventsArray.push_back({
                    {"timestamp", timestampToISO8601(me.timestamp)},
                    {"event_type", me.eventType},
                    {"content", me.content},
                    {"pos_x", me.posX},
                    {"pos_y", me.posY},
                    {"element_type", me.elementType}
                });
            }
            j["mouse_events"] = mouseEventsArray;
        }
        
        // System info
        json sysInfo;
        if (event.systemInfo.batteryPercent) 
            sysInfo["battery_percent"] = *event.systemInfo.batteryPercent;
        sysInfo["is_charging"] = event.systemInfo.isCharging;
        sysInfo["network_type"] = event.systemInfo.networkType;
        if (event.systemInfo.locationLat) 
            sysInfo["location_lat"] = *event.systemInfo.locationLat;
        if (event.systemInfo.locationLon) 
            sysInfo["location_lon"] = *event.systemInfo.locationLon;
        if (event.systemInfo.cpuUsage) 
            sysInfo["cpu_usage"] = *event.systemInfo.cpuUsage;
        if (event.systemInfo.memoryUsage) 
            sysInfo["memory_usage"] = *event.systemInfo.memoryUsage;
        
        if (!sysInfo.empty()) {
            j["system_info"] = sysInfo;
        }
        
        return j.dump();
    }
    
    RawEvent jsonToEvent(const std::string& jsonStr) {
        RawEvent event;
        try {
            auto j = json::parse(jsonStr);
            
            event.eventId = j["event_id"].get<std::string>();
            event.timestamp = iso8601ToTimestamp(j["timestamp"].get<std::string>());
            event.deviceId = j["device_id"].get<std::string>();
            event.appName = j["app_name"].get<std::string>();
            event.interactionCount = j.value("interaction_count", 0);
            event.dwellTimeSeconds = j.value("dwell_time_seconds", 0);
            event.compressed = j.value("compressed", false);
            
            if (j.contains("created_at") && !j["created_at"].is_null()) {
                event.createdAt = iso8601ToTimestamp(j["created_at"].get<std::string>());
            }
            
            // Optional fields
            if (j.contains("window_title") && !j["window_title"].is_null()) {
                event.windowTitle = j["window_title"].get<std::string>();
            }
            if (j.contains("url") && !j["url"].is_null()) {
                event.url = j["url"].get<std::string>();
            }
            if (j.contains("screen_content") && !j["screen_content"].is_null()) {
                event.screenContent = j["screen_content"].get<std::string>();
            }
            
            // Mouse events, system info, etc. can be added similarly
        } catch (...) {
            // Return empty event on error
        }
        
        return event;
    }
};

// ElasticsearchClient implementation
ElasticsearchClient::ElasticsearchClient(const std::string& esUrl)
    : pImpl_(std::make_unique<Impl>(esUrl)) {
}

ElasticsearchClient::~ElasticsearchClient() = default;

bool ElasticsearchClient::initializeIndex(const std::string& indexName) {
    std::string endpoint = "/" + indexName;
    std::string response;
    
    // Check if exists
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        return true;  // Already exists
    }
    
    // Create index with mapping
    json mapping = json::parse(R"({
        "mappings": {
            "properties": {
                "event_id": {"type": "keyword"},
                "timestamp": {"type": "date"},
                "created_at": {"type": "date"},
                "device_id": {"type": "keyword"},
                "app_name": {"type": "keyword"},
                "window_title": {"type": "text"},
                "url": {"type": "keyword"},
                "screen_content": {"type": "text"},
                "interaction_count": {"type": "integer"},
                "dwell_time_seconds": {"type": "integer"},
                "compressed": {"type": "boolean"}
            }
        }
    })");
    
    return pImpl_->httpRequest("PUT", endpoint, mapping.dump(), response);
}

std::string ElasticsearchClient::indexDocument(const std::string& indexName, 
                                              const RawEvent& event) {
    std::string eventJson = pImpl_->eventToJson(event);
    std::string endpoint = "/" + indexName + "/_doc/" + event.eventId;
    std::string response;
    
    if (pImpl_->httpRequest("PUT", endpoint, eventJson, response)) {
        return event.eventId;
    }
    return "";
}

bool ElasticsearchClient::bulkIndexDocuments(const std::string& indexName,
                                            const std::vector<RawEvent>& events) {
    if (events.empty()) return true;
    
    std::ostringstream bulk;
    for (const auto& event : events) {
        json action = {
            {"index", {
                {"_index", indexName},
                {"_id", event.eventId}
            }}
        };
        bulk << action.dump() << "\n";
        bulk << pImpl_->eventToJson(event) << "\n";
    }
    
    std::string response;
    return pImpl_->httpRequest("POST", "/_bulk", bulk.str(), response);
}

SearchResult ElasticsearchClient::search(const std::string& indexName,
                                        const std::string& query,
                                        int from,
                                        int size) {
    SearchResult result;
    std::string endpoint = "/" + indexName + "/_search?from=" + 
                          std::to_string(from) + "&size=" + std::to_string(size);
    
    std::string response;
    if (!pImpl_->httpRequest("POST", endpoint, query, response)) {
        return result;
    }
    
    try {
        auto j = json::parse(response);
        if (j.contains("hits") && j["hits"].contains("total")) {
            if (j["hits"]["total"].is_object()) {
                result.totalHits = j["hits"]["total"]["value"].get<int>();
            } else {
                result.totalHits = j["hits"]["total"].get<int>();
            }
        }
        
        for (const auto& hit : j["hits"]["hits"]) {
            result.events.push_back(pImpl_->jsonToEvent(hit["_source"].dump()));
        }
    } catch (...) {
        // Return empty result on error
    }
    
    return result;
}

std::vector<RawEvent> ElasticsearchClient::getUncompressedEvents(
    const std::string& indexName, 
    int hours) {
    
    json query = json::parse(R"({
        "query": {
            "bool": {
                "must": [
                    {"term": {"compressed": false}},
                    {"range": {"timestamp": {"gte": "now-)" + std::to_string(hours) + R"(h"}}}
                ]
            }
        },
        "sort": [{"timestamp": "asc"}],
        "size": 10000
    })");
    
    SearchResult result = search(indexName, query.dump(), 0, 10000);
    return result.events;
}

bool ElasticsearchClient::updateDocument(const std::string& indexName,
                                        const std::string& docId,
                                        const std::string& updateJson) {
    std::string endpoint = "/" + indexName + "/_update/" + docId;
    std::string response;
    return pImpl_->httpRequest("POST", endpoint, updateJson, response);
}

bool ElasticsearchClient::markEventsAsCompressed(const std::string& indexName,
                                                const std::vector<std::string>& eventIds,
                                                const std::string& sessionId) {
    if (eventIds.empty()) return true;
    
    std::ostringstream bulk;
    for (const auto& eventId : eventIds) {
        json action = {
            {"update", {
                {"_index", indexName},
                {"_id", eventId}
            }}
        };
        bulk << action.dump() << "\n";
        
        json doc = {
            {"doc", {
                {"compressed", true},
                {"session_id", sessionId}
            }}
        };
        bulk << doc.dump() << "\n";
    }
    
    std::string response;
    return pImpl_->httpRequest("POST", "/_bulk", bulk.str(), response);
}

int ElasticsearchClient::deleteOlderThan(const std::string& indexName,
                                        std::time_t cutoffTime) {
    std::string cutoffStr = timestampToISO8601(cutoffTime);
    
    json query = {
        {"query", {
            {"range", {
                {"timestamp", {
                    {"lt", cutoffStr}
                }}
            }}
        }}
    };
    
    std::string endpoint = "/" + indexName + "/_delete_by_query";
    std::string response;
    
    if (!pImpl_->httpRequest("POST", endpoint, query.dump(), response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j.value("deleted", 0);
    } catch (...) {
        return 0;
    }
}

bool ElasticsearchClient::refreshIndex(const std::string& indexName) {
    std::string endpoint = "/" + indexName + "/_refresh";
    std::string response;
    return pImpl_->httpRequest("POST", endpoint, "", response);
}

IndexStats ElasticsearchClient::getIndexStats(const std::string& indexName) {
    IndexStats stats;
    stats.indexName = indexName;
    
    stats.documentCount = getDocumentCount(indexName);
    stats.uncompressedCount = getUncompressedCount(indexName);
    stats.compressedCount = stats.documentCount - stats.uncompressedCount;
    
    // Get size
    std::string endpoint = "/" + indexName + "/_stats";
    std::string response;
    
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("indices") && j["indices"].contains(indexName)) {
                auto idx = j["indices"][indexName];
                if (idx.contains("total") && idx["total"].contains("store")) {
                    stats.sizeInBytes = idx["total"]["store"]["size_in_bytes"].get<long long>();
                }
            }
        } catch (...) {}
    }
    
    return stats;
}

int ElasticsearchClient::getDocumentCount(const std::string& indexName) {
    std::string endpoint = "/" + indexName + "/_count";
    std::string response;
    
    if (!pImpl_->httpRequest("GET", endpoint, "", response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j.value("count", 0);
    } catch (...) {
        return 0;
    }
}

int ElasticsearchClient::getUncompressedCount(const std::string& indexName) {
    json query = {
        {"query", {
            {"term", {{"compressed", false}}}
        }}
    };
    
    std::string endpoint = "/" + indexName + "/_count";
    std::string response;
    
    if (!pImpl_->httpRequest("POST", endpoint, query.dump(), response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j.value("count", 0);
    } catch (...) {
        return 0;
    }
}

bool ElasticsearchClient::deleteIndex(const std::string& indexName) {
    std::string endpoint = "/" + indexName;
    std::string response;
    return pImpl_->httpRequest("DELETE", endpoint, "", response);
}

bool ElasticsearchClient::indexExists(const std::string& indexName) {
    std::string endpoint = "/" + indexName;
    std::string response;
    return pImpl_->httpRequest("GET", endpoint, "", response);
}

bool ElasticsearchClient::testConnection() {
    std::string response;
    return pImpl_->httpRequest("GET", "/", "", response);
}

std::string ElasticsearchClient::getClusterInfo() {
    std::string response;
    if (pImpl_->httpRequest("GET", "/", "", response)) {
        return response;
    }
    return "{}";
}

} // namespace elasticsearch
