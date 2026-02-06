// src/ElasticsearchClient.cpp
#include "ElasticsearchClient.h"
#include <curl/cURL.h>
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

// Helper: Sanitize UTF-8 strings for JSON
static std::string sanitizeUtf8ForJson(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        
        // Single-byte character (ASCII: 0x00-0x7F)
        if (c <= 0x7F) {
            // Filter out control characters except newline, tab, and carriage return
            if (c >= 0x20 || c == '\n' || c == '\r' || c == '\t') {
                output.push_back(input[i]);
            }
            i++;
        }
        // Two-byte character (0xC0-0xDF)
        else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                if ((c2 & 0xC0) == 0x80) {
                    output.push_back(input[i]);
                    output.push_back(input[i + 1]);
                    i += 2;
                    continue;
                }
            }
            // Invalid sequence, skip
            i++;
        }
        // Three-byte character (0xE0-0xEF)
        else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                unsigned char c3 = static_cast<unsigned char>(input[i + 2]);
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                    output.push_back(input[i]);
                    output.push_back(input[i + 1]);
                    output.push_back(input[i + 2]);
                    i += 3;
                    continue;
                }
            }
            // Invalid sequence, skip
            i++;
        }
        // Four-byte character (0xF0-0xF7)
        else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < input.size()) {
                unsigned char c2 = static_cast<unsigned char>(input[i + 1]);
                unsigned char c3 = static_cast<unsigned char>(input[i + 2]);
                unsigned char c4 = static_cast<unsigned char>(input[i + 3]);
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
                    output.push_back(input[i]);
                    output.push_back(input[i + 1]);
                    output.push_back(input[i + 2]);
                    output.push_back(input[i + 3]);
                    i += 4;
                    continue;
                }
            }
            // Invalid sequence, skip
            i++;
        }
        // Invalid UTF-8 start byte, skip
        else {
            i++;
        }
    }
    
    return output;
}

// Helper: Convert time_t to ISO 8601 string (using local time)
static std::string timestampToISO8601(std::time_t timestamp) {
    std::tm tm_val;
#ifdef _WIN32
    localtime_s(&tm_val, &timestamp);  // Use local time
#else
    localtime_r(&timestamp, &tm_val);  // Use local time
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    oss << ".000";  // Remove Z suffix to indicate local time
    return oss.str();
}

// Helper: Convert ISO 8601 string to time_t (parse as local time)
static std::time_t iso8601ToTimestamp(const std::string& iso8601) {
    std::tm tm_val = {};
    std::istringstream ss(iso8601);
    ss >> std::get_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
    
    // mktime treats input as local time
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
        
        if ((res == CURLE_OK) && (http_code >= 200 && http_code < 300)) {
            return true;
        }
        else {
            // Enhanced error logging
            std::cerr << "Elasticsearch request failed:" << std::endl;
            std::cerr << "  Method: " << method << std::endl;
            std::cerr << "  URL: " << url << std::endl;
            std::cerr << "  HTTP Code: " << http_code << std::endl;
            std::cerr << "  CURL Code: " << res << std::endl;
            if (!body.empty()) {
                std::cerr << "  Request Body (first 500 chars): " 
                         << body.substr(0, 500) << std::endl;
            }
            if (!response.empty()) {
                std::cerr << "  Response: " << response << std::endl;
            }
            return false;
        }
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
            {"compressed", event.compressed},
            {"summarized", event.summarized}
        };
        
        // Optional fields - sanitize text fields to prevent UTF-8 errors
        if (event.windowTitle) j["window_title"] = sanitizeUtf8ForJson(*event.windowTitle);
        if (event.url) j["url"] = *event.url;
        if (event.screenContent) j["screen_content"] = sanitizeUtf8ForJson(*event.screenContent);
        if (event.screenContentHash) j["screen_content_hash"] = *event.screenContentHash;
        if (event.similarScreenContent) j["similar_screen_content"] = sanitizeUtf8ForJson(*event.similarScreenContent);
        if (event.voiceTranscription) j["voice_transcription"] = sanitizeUtf8ForJson(*event.voiceTranscription);
        if (event.cameraDescription) j["camera_description"] = sanitizeUtf8ForJson(*event.cameraDescription);
        if (event.sessionId) j["session_id"] = *event.sessionId;
        if (event.contentType) j["content_type"] = contentTypeToString(*event.contentType);
        if (event.domain) j["domain"] = domainToString(*event.domain);
        
        // Mouse events
        if (!event.mouseEvents.empty()) {
            json mouseEventsArray = json::array();
            for (const auto& me : event.mouseEvents) {
                mouseEventsArray.push_back({
                    {"timestamp", timestampToISO8601(me.timestamp)},
                    //{"event_type", me.eventType},
                    {"content", sanitizeUtf8ForJson(me.content)}/*,
                    {"pos_x", me.posX},
                    {"pos_y", me.posY},
                    {"element_type", me.elementType}*/
                });
            }
            j["mouse_events"] = mouseEventsArray;
        }
        
        // System info - FIXED: Proper geo_point format for Elasticsearch
        json sysInfo;
        if (event.systemInfo.batteryPercent) 
            sysInfo["battery_percent"] = *event.systemInfo.batteryPercent;
        sysInfo["is_charging"] = event.systemInfo.isCharging;
        sysInfo["network_type"] = event.systemInfo.networkType;
        
        // FIX: Elasticsearch geo_point must be a single object with lat/lon, not separate fields
        if (event.systemInfo.locationLat && event.systemInfo.locationLon) {
            sysInfo["location"] = {
                {"lat", *event.systemInfo.locationLat},
                {"lon", *event.systemInfo.locationLon}
            };
        }
        
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
            event.summarized = j.value("summarized", false);
            
            if (j.contains("created_at") && !j["created_at"].is_null()) {
                event.createdAt = iso8601ToTimestamp(j["created_at"].get<std::string>());
            }
            
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
            if (j.contains("similar_screen_content") && !j["similar_screen_content"].is_null()) {
                event.similarScreenContent = j["similar_screen_content"].get<std::string>();
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
            
            // Content type
            if (j.contains("content_type") && !j["content_type"].is_null()) {
                std::string ctStr = j["content_type"].get<std::string>();
                if (ctStr == "TEXT") event.contentType = ContentType::TEXT;
                else if (ctStr == "IMAGE") event.contentType = ContentType::IMAGE;
                else if (ctStr == "VIDEO") event.contentType = ContentType::VIDEO;
                else if (ctStr == "AUDIO" ) event.contentType = ContentType::AUDIO;
                else if (ctStr == "CODE") event.contentType = ContentType::CODE;
                else if (ctStr == "DOCUMENT") event.contentType = ContentType::DOCUMENT;
                else event.contentType = ContentType::UNKNOWN;
            }
            
            // Domain
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
                else event.domain = Domain::UNKNOWN;
            }
            
            // Mouse events
            if (j.contains("mouse_events") && j["mouse_events"].is_array()) {
                for (const auto& meJson : j["mouse_events"]) {
                    MouseEvent me;
                    if (meJson.contains("timestamp") && !meJson["timestamp"].is_null()) {
                        me.timestamp = iso8601ToTimestamp(meJson["timestamp"].get<std::string>());
                    }
                    /*if (meJson.contains("event_type") && !meJson["event_type"].is_null()) {
                        me.eventType = meJson["event_type"].get<std::string>();
                    }
                    if (meJson.contains("content") && !meJson["content"].is_null()) {
                        me.content = meJson["content"].get<std::string>();
                    }
                    if (meJson.contains("pos_x") && !meJson["pos_x"].is_null()) {
                        me.posX = meJson["pos_x"].get<int>();
                    }
                    if (meJson.contains("pos_y") && !meJson["pos_y"].is_null()) {
                        me.posY = meJson["pos_y"].get<int>();
                    }
                    if (meJson.contains("element_type") && !meJson["element_type"].is_null()) {
                        me.elementType = meJson["element_type"].get<std::string>();
                    }*/
                    event.mouseEvents.push_back(me);
                }
            }
            
            // System info
            if (j.contains("system_info") && j["system_info"].is_object()) {
                const auto& sysInfo = j["system_info"];
                
                if (sysInfo.contains("battery_percent") && !sysInfo["battery_percent"].is_null()) {
                    event.systemInfo.batteryPercent = sysInfo["battery_percent"].get<int>();
                }
                if (sysInfo.contains("is_charging") && !sysInfo["is_charging"].is_null()) {
                    event.systemInfo.isCharging = sysInfo["is_charging"].get<bool>();
                }
                if (sysInfo.contains("network_type") && !sysInfo["network_type"].is_null()) {
                    event.systemInfo.networkType = sysInfo["network_type"].get<std::string>();
                }
                
                // FIX: Parse geo_point location object
                if (sysInfo.contains("location") && sysInfo["location"].is_object()) {
                    const auto& location = sysInfo["location"];
                    if (location.contains("lat") && !location["lat"].is_null()) {
                        event.systemInfo.locationLat = location["lat"].get<double>();
                    }
                    if (location.contains("lon") && !location["lon"].is_null()) {
                        event.systemInfo.locationLon = location["lon"].get<double>();
                    }
                }
                // Support legacy format (separate lat/lon fields)
                else {
                    if (sysInfo.contains("location_lat") && !sysInfo["location_lat"].is_null()) {
                        event.systemInfo.locationLat = sysInfo["location_lat"].get<double>();
                    }
                    if (sysInfo.contains("location_lon") && !sysInfo["location_lon"].is_null()) {
                        event.systemInfo.locationLon = sysInfo["location_lon"].get<double>();
                    }
                }
                
                if (sysInfo.contains("cpu_usage") && !sysInfo["cpu_usage"].is_null()) {
                    event.systemInfo.cpuUsage = sysInfo["cpu_usage"].get<double>();
                }
                if (sysInfo.contains("memory_usage") && !sysInfo["memory_usage"].is_null()) {
                    event.systemInfo.memoryUsage = sysInfo["memory_usage"].get<double>();
                }
            }
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

DatabaseType ElasticsearchClient::getType() const {
    return DatabaseType::ELASTICSEARCH;
}

bool ElasticsearchClient::initializeCollection(const std::string& collectionName) {
    std::string endpoint = "/" + collectionName;
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
                "screen_content_hash": {"type": "keyword"},
                "similar_screen_content": {"type": "text"},
                "voice_transcription": {"type": "text"},
                "camera_description": {"type": "text"},
                "session_id": {"type": "keyword"},
                "content_type": {"type": "keyword"},
                "domain": {"type": "keyword"},
                "interaction_count": {"type": "integer"},
                "dwell_time_seconds": {"type": "integer"},
                "compressed": {"type": "boolean"},
                "summarized": {"type": "boolean"},
                "mouse_events": {
                    "type": "nested",
                    "properties": {
                        "timestamp": {"type": "date"},
                        "event_type": {"type": "keyword"},
                        "content": {"type": "text"},
                        "pos_x": {"type": "integer"},
                        "pos_y": {"type": "integer"},
                        "element_type": {"type": "keyword"}
                    }
                },
                "system_info": {
                    "type": "object",
                    "properties": {
                        "battery_percent": {"type": "integer"},
                        "is_charging": {"type": "boolean"},
                        "network_type": {"type": "keyword"},
                        "location": {"type": "geo_point"},
                        "cpu_usage": {"type": "float"},
                        "memory_usage": {"type": "float"}
                    }
                }
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
    int max_count) {
    
    json query = {
        {"query", {
            {"term", {{"compressed", false}}}
        }},
        {"sort", {{"timestamp", "asc"}}},
        {"size", max_count}
    };
    
    SearchResult result = search(indexName, query.dump(), 0, max_count);
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
    bool httpSuccess = pImpl_->httpRequest("POST", "/_bulk", bulk.str(), response);
    
    if (!httpSuccess) {
        std::cerr << "[ElasticsearchClient] HTTP request failed for bulk update" << std::endl;
        return false;
    }
    
    // Parse bulk response to check for individual operation failures
    try {
        auto j = json::parse(response);
        
        if (j.contains("errors") && j["errors"].get<bool>()) {
            // Some operations failed
            std::vector<std::string> failedIds;
            
            if (j.contains("items") && j["items"].is_array()) {
                size_t idx = 0;
                for (const auto& item : j["items"]) {
                    if (item.contains("update")) {
                        const auto& updateResult = item["update"];
                        
                        if (updateResult.contains("error")) {
                            if (idx < eventIds.size()) {
                                failedIds.push_back(eventIds[idx]);
                                std::cerr << "[ElasticsearchClient] Failed to update event " 
                                         << eventIds[idx] << ": ";
                                if (updateResult["error"].contains("reason")) {
                                    std::cerr << updateResult["error"]["reason"].get<std::string>();
                                }
                                std::cerr << std::endl;
                            }
                        }
                    }
                    idx++;
                }
            }
            
            int successCount = eventIds.size() - failedIds.size();
            std::cerr << "[ElasticsearchClient] Bulk update partial failure: " 
                     << successCount << " succeeded, " 
                     << failedIds.size() << " failed" << std::endl;
            
            return failedIds.empty();
        }
        
        std::cout << "[ElasticsearchClient] Successfully updated " << eventIds.size() 
                  << " events with session_id=" << sessionId << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ElasticsearchClient] Failed to parse bulk response: " << e.what() << std::endl;
        return true;
    }
}

bool ElasticsearchClient::markEventsAsCompressedWithSimilarity(
    const std::string& indexName,
    const std::vector<std::string>& eventIds,
    const std::string& sessionId,
    const std::string& similarScreenContent) {
    
    if (eventIds.empty()) return true;
    
    // Sanitize the similarity content before serialization
    std::string sanitizedContent = sanitizeUtf8ForJson(similarScreenContent);
    
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
                {"session_id", sessionId},
                {"similar_screen_content", sanitizedContent}
            }}
        };
        bulk << doc.dump() << "\n";
    }
    
    std::string response;
    bool httpSuccess = pImpl_->httpRequest("POST", "/_bulk", bulk.str(), response);
    
    if (!httpSuccess) {
        std::cerr << "[ElasticsearchClient] HTTP request failed for bulk update" << std::endl;
        return false;
    }
    
    // Parse bulk response to check for individual operation failures
    try {
        auto j = json::parse(response);
        
        if (j.contains("errors") && j["errors"].get<bool>()) {
            // Some operations failed
            std::vector<std::string> failedIds;
            
            if (j.contains("items") && j["items"].is_array()) {
                size_t idx = 0;
                for (const auto& item : j["items"]) {
                    if (item.contains("update")) {
                        const auto& updateResult = item["update"];
                        
                        if (updateResult.contains("error")) {
                            if (idx < eventIds.size()) {
                                failedIds.push_back(eventIds[idx]);
                                std::cerr << "[ElasticsearchClient] Failed to update event " 
                                         << eventIds[idx] << ": ";
                                if (updateResult["error"].contains("reason")) {
                                    std::cerr << updateResult["error"]["reason"].get<std::string>();
                                }
                                std::cerr << std::endl;
                            }
                        }
                    }
                    idx++;
                }
            }
            
            int successCount = eventIds.size() - failedIds.size();
            std::cerr << "[ElasticsearchClient] Bulk update partial failure: " 
                     << successCount << " succeeded, " 
                     << failedIds.size() << " failed" << std::endl;
            
            return failedIds.empty();
        }
        
        std::cout << "[ElasticsearchClient] Successfully updated " << eventIds.size() 
                  << " events with session_id=" << sessionId << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ElasticsearchClient] Failed to parse bulk response: " << e.what() << std::endl;
        return true;
    }
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

// Interface implementations - use new method names

bool ElasticsearchClient::refreshCollection(const std::string& collectionName) {
    std::string endpoint = "/" + collectionName + "/_refresh";
    std::string response;
    return pImpl_->httpRequest("POST", endpoint, "", response);
}

CollectionStats ElasticsearchClient::getCollectionStats(const std::string& collectionName) {
    CollectionStats stats;
    stats.collectionName = collectionName;
    
    stats.documentCount = getDocumentCount(collectionName);
    stats.uncompressedCount = getUncompressedCount(collectionName);
    stats.compressedCount = stats.documentCount - stats.uncompressedCount;
    
    // Get size
    std::string endpoint = "/" + collectionName + "/_stats";
    std::string response;
    
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("indices") && j["indices"].contains(collectionName)) {
                auto idx = j["indices"][collectionName];
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

bool ElasticsearchClient::deleteCollection(const std::string& collectionName) {
    std::string endpoint = "/" + collectionName;
    std::string response;
    return pImpl_->httpRequest("DELETE", endpoint, "", response);
}

bool ElasticsearchClient::testConnection() {
    std::string response;
    return pImpl_->httpRequest("GET", "/", "", response);
}

std::string ElasticsearchClient::getServerInfo() {
    std::string response;
    if (pImpl_->httpRequest("GET", "/", "", response)) {
        return response;
    }
    return "{}";
}

bool ElasticsearchClient::collectionExists(const std::string& collectionName) {
    std::string endpoint = "/" + collectionName;
    std::string response;
    return pImpl_->httpRequest("GET", endpoint, "", response);
}

RawEvent ElasticsearchClient::getDocumentByAppName(const std::string& indexName, 
                                                    const std::string& appName) {
    std::string endpoint = "/" + indexName + "/_doc/" + appName;
    std::string response;
    
    if (pImpl_->httpRequest("GET", endpoint, "", response)) {
        try {
            auto j = json::parse(response);
            if (j.contains("_source") && !j["_source"].is_null()) {
                return pImpl_->jsonToEvent(j["_source"].dump());
            }
        } catch (...) {
            // Return empty event on error
        }
    }
    
    return RawEvent();
}

bool ElasticsearchClient::deleteDocumentByAppName(const std::string& indexName, 
                                                   const std::string& appName) {
    std::string endpoint = "/" + indexName + "/_doc/" + appName;
    std::string response;
    return pImpl_->httpRequest("DELETE", endpoint, "", response);
}

} // namespace database
