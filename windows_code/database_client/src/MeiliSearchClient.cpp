// src/MeiliSearchClient.cpp
#include "MeiliSearchClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <iostream>

using json = nlohmann::json;

namespace database {

// Helper: CURL write callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper: Convert time_t to timestamp string
static std::string timestampToString(std::time_t timestamp) {
    return std::to_string(timestamp);
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
class MeiliSearchClient::Impl {
public:
    std::string meiliUrl_;
    std::string apiKey_;

    explicit Impl(const std::string& meiliUrl, const std::string& apiKey)
        : meiliUrl_(meiliUrl), apiKey_(apiKey) {
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
        
        std::string url = meiliUrl_;
        if (!endpoint.empty() && endpoint[0] != '/') {
            url += "/";
        }
        url += endpoint;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        if (!apiKey_.empty()) {
            std::string auth = "Authorization: Bearer " + apiKey_;
            headers = curl_slist_append(headers, auth.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            return true;
        } else {
            // Log error for debugging
            std::cerr << "MeiliSearch request failed:" << std::endl;
            std::cerr << "  Method: " << method << std::endl;
            std::cerr << "  URL: " << url << std::endl;
            std::cerr << "  HTTP Code: " << http_code << std::endl;
            std::cerr << "  CURL Code: " << res << std::endl;
            if (!response.empty()) {
                std::cerr << "  Response: " << response << std::endl;
            }
            return false;
        }
    }

    // Convert RawEvent to MeiliSearch JSON
    json eventToJson(const RawEvent& event) {
        json j = {
            {"event_id", event.eventId},
            {"timestamp", event.timestamp},
            {"created_at", event.createdAt},
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
        
        // Mouse events (if needed)
        if (!event.mouseEvents.empty()) {
            json mouseEventsArray = json::array();
            for (const auto& me : event.mouseEvents) {
                mouseEventsArray.push_back({
                    {"timestamp", me.timestamp},
                    {"event_type", me.eventType},
                    {"content", me.content},
                    {"pos_x", me.posX},
                    {"pos_y", me.posY},
                    {"element_type", me.elementType}
                });
            }
            j["mouse_events"] = mouseEventsArray;
        }
        
        // System info (simplified)
        json sysInfo;
        if (event.systemInfo.batteryPercent) 
            sysInfo["battery_percent"] = *event.systemInfo.batteryPercent;
        sysInfo["is_charging"] = event.systemInfo.isCharging;
        sysInfo["network_type"] = event.systemInfo.networkType;
        
        if (event.systemInfo.locationLat && event.systemInfo.locationLon) {
            sysInfo["location_lat"] = *event.systemInfo.locationLat;
            sysInfo["location_lon"] = *event.systemInfo.locationLon;
        }
        
        if (event.systemInfo.cpuUsage) 
            sysInfo["cpu_usage"] = *event.systemInfo.cpuUsage;
        if (event.systemInfo.memoryUsage) 
            sysInfo["memory_usage"] = *event.systemInfo.memoryUsage;
        
        if (!sysInfo.empty()) {
            j["system_info"] = sysInfo;
        }
        
        return j;
    }
    
    // Convert MeiliSearch JSON to RawEvent
    RawEvent jsonToEvent(const json& j) {
        RawEvent event;
        
        event.eventId = j.value("event_id", "");
        event.timestamp = j.value("timestamp", (std::time_t)0);
        event.createdAt = j.value("created_at", (std::time_t)0);
        event.deviceId = j.value("device_id", "");
        event.appName = j.value("app_name", "");
        event.interactionCount = j.value("interaction_count", 0);
        event.dwellTimeSeconds = j.value("dwell_time_seconds", 0);
        event.compressed = j.value("compressed", false);
        
        // Optional string fields
        if (j.contains("window_title") && !j["window_title"].is_null()) {
            event.windowTitle = j["window_title"].get<std::string>();
        }
        if (j.contains("url") && !j["url"].is_null()) {
            event.url = j["url"].get<std::string>();
        }
        if (j.contains("screen_content") && !j["screen_content"].is_null()) {
            event.screenContent = j["screen_content"].get<std::string>();
        }
        if (j.contains("screen_content_hash") && !j["screen_content_hash"].is_null()) {
            event.screenContentHash = j["screen_content_hash"].get<std::string>();
        }
        if (j.contains("voice_transcription") && !j["voice_transcription"].is_null()) {
            event.voiceTranscription = j["voice_transcription"].get<std::string>();
        }
        if (j.contains("camera_description") && !j["camera_description"].is_null()) {
            event.cameraDescription = j["camera_description"].get<std::string>();
        }
        if (j.contains("session_id") && !j["session_id"].is_null()) {
            event.sessionId = j["session_id"].get<std::string>();
        }
        
        // Content type (can be extended)
        if (j.contains("content_type") && !j["content_type"].is_null()) {
            std::string ctStr = j["content_type"].get<std::string>();
            if (ctStr == "TEXT") event.contentType = ContentType::TEXT;
            else if (ctStr == "IMAGE") event.contentType = ContentType::IMAGE;
            else if (ctStr == "VIDEO") event.contentType = ContentType::VIDEO;
            else if (ctStr == "AUDIO") event.contentType = ContentType::AUDIO;
            else if (ctStr == "CODE") event.contentType = ContentType::CODE;
            else if (ctStr == "DOCUMENT") event.contentType = ContentType::DOCUMENT;
        }
        
        // Domain (can be extended)
        if (j.contains("domain") && !j["domain"].is_null()) {
            std::string domainStr = j["domain"].get<std::string>();
            if (domainStr == "WORK") event.domain = Domain::WORK;
            else if (domainStr == "ENTERTAINMENT") event.domain = Domain::ENTERTAINMENT;
            else if (domainStr == "SOCIAL") event.domain = Domain::SOCIAL;
            else if (domainStr == "SHOPPING") event.domain = Domain::SHOPPING;
            else if (domainStr == "NEWS") event.domain = Domain::NEWS;
            else if (domainStr == "EDUCATION") event.domain = Domain::EDUCATION;
            else if (domainStr == "HEALTH") event.domain = Domain::HEALTH;
            else if (domainStr == "OTHER") event.domain = Domain::OTHER;
        }
        
        return event;
    }
};

// MeiliSearchClient implementation
MeiliSearchClient::MeiliSearchClient(const std::string& meiliUrl, const std::string& apiKey)
    : pImpl_(std::make_unique<Impl>(meiliUrl, apiKey)) {
}

MeiliSearchClient::~MeiliSearchClient() = default;

DatabaseType MeiliSearchClient::getType() const {
    return DatabaseType::MEILISEARCH;
}

bool MeiliSearchClient::initializeCollection(const std::string& collectionName) {
    std::string response;
    json body = {
        {"uid", collectionName},
        {"primaryKey", "event_id"}
    };
    
    // Try to create index, if it already exists, just return true
    if (pImpl_->httpRequest("POST", "/indexes", body.dump(), response)) {
        return true;
    }
    
    // Check if it already exists
    return pImpl_->httpRequest("GET", "/indexes/" + collectionName, "", response);
}

std::string MeiliSearchClient::indexDocument(const std::string& collectionName, const RawEvent& event) {
    std::string response;
    json arr = json::array();
    arr.push_back(pImpl_->eventToJson(event));
    
    std::string endpoint = "/indexes/" + collectionName + "/documents";
    
    if (pImpl_->httpRequest("POST", endpoint, arr.dump(), response)) {
        return event.eventId;
    }
    
    return "";
}

bool MeiliSearchClient::bulkIndexDocuments(const std::string& collectionName, const std::vector<RawEvent>& events) {
    if (events.empty()) return true;
    
    std::string response;
    json arr = json::array();
    
    for (const auto& e : events) {
        arr.push_back(pImpl_->eventToJson(e));
    }
    
    std::string endpoint = "/indexes/" + collectionName + "/documents";
    return pImpl_->httpRequest("POST", endpoint, arr.dump(), response);
}

SearchResult MeiliSearchClient::search(const std::string& collectionName, const std::string& query, int from, int size) {
    SearchResult result;
    std::string response;
    
    json searchBody;
    
    // Try to parse query as JSON first
    try {
        searchBody = json::parse(query);
    } catch (...) {
        // Simple string query
        searchBody = {
            {"q", query},
            {"limit", size},
            {"offset", from}
        };
    }
    
    std::string endpoint = "/indexes/" + collectionName + "/search";
    
    if (pImpl_->httpRequest("POST", endpoint, searchBody.dump(), response)) {
        try {
            auto j = json::parse(response);
            result.totalHits = j.value("estimatedTotalHits", 0);
            
            if (j.contains("hits") && j["hits"].is_array()) {
                for (const auto& hit : j["hits"]) {
                    result.events.push_back(pImpl_->jsonToEvent(hit));
                }
            }
        } catch (...) {
            // Parse error, return empty result
        }
    }
    
    return result;
}

std::vector<RawEvent> MeiliSearchClient::getUncompressedEvents(const std::string& collectionName, int hours) {
    std::vector<RawEvent> events;
    std::string response;
    
    std::time_t cutoff = std::time(nullptr) - hours * 3600;
    
    json searchBody = {
        {"q", ""},
        {"filter", "compressed = false AND timestamp >= " + std::to_string(cutoff)},
        {"limit", 10000}
    };
    
    std::string endpoint = "/indexes/" + collectionName + "/search";
    
    if (pImpl_->httpRequest("POST", endpoint, searchBody.dump(), response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("hits") && j["hits"].is_array()) {
                for (const auto& hit : j["hits"]) {
                    events.push_back(pImpl_->jsonToEvent(hit));
                }
            }
        } catch (...) {
            // Parse error
        }
    }
    
    return events;
}

bool MeiliSearchClient::updateDocument(const std::string& collectionName, const std::string& docId, const std::string& updateData) {
    std::string response;
    json updateJson;
    
    try {
        updateJson = json::parse(updateData);
    } catch (...) {
        return false;
    }
    
    updateJson["event_id"] = docId;
    json arr = json::array();
    arr.push_back(updateJson);
    
    std::string endpoint = "/indexes/" + collectionName + "/documents";
    return pImpl_->httpRequest("PUT", endpoint, arr.dump(), response);
}

bool MeiliSearchClient::markEventsAsCompressed(const std::string& collectionName, 
                                              const std::vector<std::string>& eventIds, 
                                              const std::string& sessionId) {
    if (eventIds.empty()) return true;
    
    std::string response;
    json arr = json::array();
    
    for (const auto& id : eventIds) {
        json doc = {
            {"event_id", id},
            {"compressed", true},
            {"session_id", sessionId}
        };
        arr.push_back(doc);
    }
    
    std::string endpoint = "/indexes/" + collectionName + "/documents";
    return pImpl_->httpRequest("PUT", endpoint, arr.dump(), response);
}

int MeiliSearchClient::deleteOlderThan(const std::string& collectionName, std::time_t cutoffTime) {
    // MeiliSearch does not support delete by query directly
    // We need to fetch documents first, then delete by IDs
    std::vector<RawEvent> oldEvents;
    std::string response;
    
    json searchBody = {
        {"q", ""},
        {"filter", "timestamp < " + std::to_string(cutoffTime)},
        {"limit", 10000}
    };
    
    std::string endpoint = "/indexes/" + collectionName + "/search";
    
    if (pImpl_->httpRequest("POST", endpoint, searchBody.dump(), response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("hits") && j["hits"].is_array()) {
                for (const auto& hit : j["hits"]) {
                    oldEvents.push_back(pImpl_->jsonToEvent(hit));
                }
            }
        } catch (...) {
            // Parse error
        }
    }
    
    if (oldEvents.empty()) return 0;
    
    // Delete by IDs
    json ids = json::array();
    for (const auto& e : oldEvents) {
        ids.push_back(e.eventId);
    }
    
    std::string delEndpoint = "/indexes/" + collectionName + "/documents/delete-batch";
    std::string delResp;
    
    if (pImpl_->httpRequest("POST", delEndpoint, ids.dump(), delResp)) {
        return static_cast<int>(oldEvents.size());
    }
    
    return 0;
}

bool MeiliSearchClient::refreshCollection(const std::string& collectionName) {
    // MeiliSearch refreshes automatically, no explicit refresh needed
    return true;
}

CollectionStats MeiliSearchClient::getCollectionStats(const std::string& collectionName) {
    CollectionStats stats;
    stats.collectionName = collectionName;
    
    std::string response;
    std::string endpoint = "/indexes/" + collectionName + "/stats";
    
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        try {
            auto j = json::parse(response);
            stats.documentCount = j.value("numberOfDocuments", 0);
            stats.sizeInBytes = j.value("databaseSize", 0LL);
            
            // MeiliSearch doesn't track compressed/uncompressed separately
            stats.uncompressedCount = 0;
            stats.compressedCount = 0;
        } catch (...) {
            // Parse error
        }
    }
    
    return stats;
}

int MeiliSearchClient::getDocumentCount(const std::string& collectionName) {
    std::string response;
    std::string endpoint = "/indexes/" + collectionName + "/stats";
    
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        try {
            auto j = json::parse(response);
            return j.value("numberOfDocuments", 0);
        } catch (...) {
            // Parse error
        }
    }
    
    return 0;
}

int MeiliSearchClient::getUncompressedCount(const std::string& collectionName) {
    // Not directly supported, do a filtered search
    std::string response;
    
    json searchBody = {
        {"q", ""},
        {"filter", "compressed = false"},
        {"limit", 0}
    };
    
    std::string endpoint = "/indexes/" + collectionName + "/search";
    
    if (pImpl_->httpRequest("POST", endpoint, searchBody.dump(), response)) {
        try {
            auto j = json::parse(response);
            return j.value("estimatedTotalHits", 0);
        } catch (...) {
            // Parse error
        }
    }
    
    return 0;
}

bool MeiliSearchClient::deleteCollection(const std::string& collectionName) {
    std::string response;
    std::string endpoint = "/indexes/" + collectionName;
    return pImpl_->httpRequest("DELETE", endpoint, "", response);
}

bool MeiliSearchClient::collectionExists(const std::string& collectionName) {
    std::string response;
    std::string endpoint = "/indexes/" + collectionName;
    return pImpl_->httpRequest("GET", endpoint, "", response);
}

bool MeiliSearchClient::testConnection() {
    std::string response;
    return pImpl_->httpRequest("GET", "/health", "", response);
}

std::string MeiliSearchClient::getServerInfo() {
    std::string response;
    
    if (pImpl_->httpRequest("GET", "/version", "", response)) {
        return response;
    }
    
    return "{}";
}

RawEvent MeiliSearchClient::getDocumentByAppName(const std::string& collectionName, const std::string& appName) {
    std::string response;
    
    json searchBody = {
        {"q", ""},
        {"filter", "app_name = '" + appName + "'"},
        {"limit", 1}
    };
    
    std::string endpoint = "/indexes/" + collectionName + "/search";
    
    if (pImpl_->httpRequest("POST", endpoint, searchBody.dump(), response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("hits") && j["hits"].is_array() && !j["hits"].empty()) {
                return pImpl_->jsonToEvent(j["hits"][0]);
            }
        } catch (...) {
            // Parse error
        }
    }
    
    return RawEvent();
}

bool MeiliSearchClient::deleteDocumentByAppName(const std::string& collectionName, const std::string& appName) {
    RawEvent event = getDocumentByAppName(collectionName, appName);
    
    if (event.eventId.empty()) {
        return false;
    }
    
    std::string response;
    std::string endpoint = "/indexes/" + collectionName + "/documents/" + event.eventId;
    return pImpl_->httpRequest("DELETE", endpoint, "", response);
}

} // namespace database
