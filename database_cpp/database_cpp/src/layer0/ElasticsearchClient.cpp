// src/layer0/ElasticsearchClient.cpp
#include "layer0/ElasticsearchClient.h"
#include "common/Logger.h"
#include "common/Utils.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>  // for std::min

using json = nlohmann::json;

namespace perception {
namespace layer0 {

ElasticsearchClient::ElasticsearchClient(const std::string& esUrl)
    : esUrl_(esUrl)
    , indexName_("perception_raw_events") {
    
    LOG_INFO("Initializing Elasticsearch client: " + esUrl);
    
    // Initialize index
    if (!initializeIndex(indexName_)) {
        throw std::runtime_error("Failed to initialize Elasticsearch index");
    }
}

ElasticsearchClient::~ElasticsearchClient() = default;

bool ElasticsearchClient::initializeIndex(const std::string& indexName) {
    const std::string endpoint = "/" + indexName;
    
    // Check if index already exists using GET request (HEAD seems to timeout)
    std::string checkResponse;
    if (httpRequest("GET", endpoint, "", checkResponse)) {
        LOG_INFO("Index already exists: " + indexName);
        return true;  // Index exists, that's fine
    }
    
    // Create index with mapping
    json mapping = {
        {"mappings", {
            {"properties", {
                {"event_id", {{"type", "keyword"}}},
                {"timestamp", {{"type", "date"}}},
                {"device_id", {{"type", "keyword"}}},
                {"app_name", {{"type", "keyword"}}},
                {"window_title", {{"type", "text"}}},
                {"url", {{"type", "keyword"}}},
                {"screen_content", {
                    {"type", "text"},
                    {"analyzer", "standard"}  // 全文搜索
                }},
                {"screen_content_hash", {{"type", "keyword"}}},
                {"interaction_count", {{"type", "integer"}}},
                {"dwell_time_seconds", {{"type", "integer"}}},
                {"compressed", {{"type", "boolean"}}},
                {"session_id", {{"type", "keyword"}}},
                {"content_type", {{"type", "keyword"}}},
                {"domain", {{"type", "keyword"}}},
                
                // System info
                {"system_info", {
                    {"properties", {
                        {"battery_percent", {{"type", "integer"}}},
                        {"is_charging", {{"type", "boolean"}}},
                        {"network_type", {{"type", "keyword"}}},
                        {"location_lat", {{"type", "geo_point"}}},
                        {"location_lon", {{"type", "geo_point"}}},
                        {"cpu_usage", {{"type", "float"}}},
                        {"memory_usage", {{"type", "float"}}}
                    }}
                }},
                
                // Mouse events (nested)
                {"mouse_events", {
                    {"type", "nested"},
                    {"properties", {
                        {"timestamp", {{"type", "date"}}},
                        {"event_type", {{"type", "keyword"}}},
                        {"content", {{"type", "text"}}},
                        {"pos_x", {{"type", "integer"}}},
                        {"pos_y", {{"type", "integer"}}},
                        {"element_type", {{"type", "keyword"}}}
                    }}
                }},
                
                // Voice and camera
                {"voice_transcription", {{"type", "text"}}},
                {"camera_description", {{"type", "text"}}},
                
                // Timestamps
                {"created_at", {{"type", "date"}}}
            }}
        }},
        {"settings", {
            {"number_of_shards", 1},
            {"number_of_replicas", 0},  // 单机部署
            {"refresh_interval", "1s"}   // 1秒刷新
        }}
    };
    
    std::string response;
    
    // PUT request to create index
    bool success = httpRequest("PUT", endpoint, mapping.dump(), response);
    
    if (!success) {
        LOG_ERROR("Failed to create index: " + indexName);
        try {
            auto errorJson = json::parse(response);
            LOG_ERROR("Elasticsearch error: " + errorJson["error"]["reason"].get<std::string>());
        } catch (...) {
            LOG_ERROR("Response: " + response);
        }
        return false;
    }
    
    LOG_INFO("Created Elasticsearch index: " + indexName);
    return true;
}

std::string ElasticsearchClient::indexDocument(
    const std::string& indexName,
    const RawEvent& event) {
    
    std::string eventJson = eventToJson(event);
    LOG_INFO("Indexing document with ID: " + event.eventId);
    LOG_DEBUG("Document JSON: " + eventJson);
    
    std::string endpoint = "/" + indexName + "/_doc/" + event.eventId;
    
    std::string response;
    if (httpRequest("PUT", endpoint, eventJson, response)) {
        LOG_INFO("Successfully indexed document: " + event.eventId);
        return event.eventId;
    }
    
    LOG_ERROR("Failed to index document: " + event.eventId);
    return "";
}

bool ElasticsearchClient::bulkIndexDocuments(
    const std::string& indexName,
    const std::vector<RawEvent>& events) {
    
    if (events.empty()) {
        return true;
    }
    
    // Build bulk request
    std::ostringstream bulk;
    for (const auto& event : events) {
        // Action line
        json action = {
            {"index", {
                {"_index", indexName},
                {"_id", event.eventId}
            }}
        };
        bulk << action.dump() << "\n";
        
        // Document line
        bulk << eventToJson(event) << "\n";
    }
    
    std::string response;
    bool success = httpRequest("POST", "/_bulk", bulk.str(), response);
    
    if (success) {
        LOG_INFO("Bulk indexed " + std::to_string(events.size()) + " events");
    }
    
    return success;
}

std::vector<RawEvent> ElasticsearchClient::getUncompressedEvents(int hours) {
    // Build query
    json query = {
        {"query", {
            {"bool", {
                {"must", {
                    {{"term", {{"compressed", false}}}},
                    {{"range", {
                        {"timestamp", {
                            {"gte", "now-" + std::to_string(hours) + "h"}
                        }}
                    }}}
                }}
            }}
        }},
        {"sort", {{{"timestamp", "asc"}}}},
        {"size", 10000}  // Maximum
    };
    
    std::string response;
    if (!httpRequest("POST", "/" + indexName_ + "/_search", 
                    query.dump(), response)) {
        return {};
    }
    
    // Parse response
    try {
        auto j = json::parse(response);
        std::vector<RawEvent> events;
        
        for (const auto& hit : j["hits"]["hits"]) {
            RawEvent event = jsonToEvent(hit["_source"].dump());
            events.push_back(event);
        }
        
        return events;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse ES response: " + std::string(e.what()));
        return {};
    }
}

bool ElasticsearchClient::markEventsAsCompressed(
    const std::vector<std::string>& eventIds,
    const std::string& sessionId) {
    
    if (eventIds.empty()) {
        return true;
    }
    
    // Build bulk update request
    std::ostringstream bulk;
    for (const auto& eventId : eventIds) {
        json action = {
            {"update", {
                {"_index", indexName_},
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
    bool success = httpRequest("POST", "/_bulk", bulk.str(), response);
    
    if (success) {
        LOG_DEBUG("Marked " + std::to_string(eventIds.size()) + 
                 " events as compressed with session_id: " + sessionId);
    }
    
    return success;
}

int ElasticsearchClient::getDocumentCount(const std::string& indexName) {
    std::string response;
    if (!httpRequest("GET", "/" + indexName + "/_count", "", response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j["count"].get<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get document count: " + std::string(e.what()));
        return 0;
    }
}

int ElasticsearchClient::getUncompressedCount() {
    json query = {
        {"query", {
            {"term", {{"compressed", false}}}
        }}
    };
    
    std::string response;
    if (!httpRequest("POST", "/" + indexName_ + "/_count", 
                    query.dump(), response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j["count"].get<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get uncompressed count: " + std::string(e.what()));
        return 0;
    }
}

int ElasticsearchClient::deleteOlderThan(const Timestamp& cutoffTime) {
    std::string cutoffStr = utils::timestampToString(cutoffTime);
    
    json query = {
        {"query", {
            {"range", {
                {"timestamp", {
                    {"lt", cutoffStr}
                }}
            }}
        }}
    };
    
    std::string response;
    if (!httpRequest("POST", "/" + indexName_ + "/_delete_by_query", 
                    query.dump(), response)) {
        return 0;
    }
    
    try {
        auto j = json::parse(response);
        return j["deleted"].get<int>();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to delete old documents: " + std::string(e.what()));
        return 0;
    }
}

bool ElasticsearchClient::refreshIndex(const std::string& indexName) {
    std::string response;
    std::string endpoint = "/" + indexName + "/_refresh";
    
    if (httpRequest("POST", endpoint, "", response)) {
        LOG_DEBUG("Refreshed index: " + indexName);
        return true;
    }
    
    LOG_ERROR("Failed to refresh index: " + indexName);
    return false;
}

std::vector<RawEvent> ElasticsearchClient::search(
    const std::string& indexName,
    const std::string& query,
    int from,
    int size) {
    
    std::string endpoint = "/" + indexName + "/_search?from=" + 
                          std::to_string(from) + "&size=" + std::to_string(size);
    
    LOG_DEBUG("Elasticsearch search endpoint: " + esUrl_ + endpoint);
    LOG_DEBUG("Search query: " + query);
    
    std::string response;
    if (!httpRequest("POST", endpoint, query, response)) {
        LOG_ERROR("Search HTTP request failed");
        return {};
    }
    
    LOG_DEBUG("Search response: " + response);
    
    try {
        auto j = json::parse(response);
        
        // Log search results details
        if (j.contains("hits") && j["hits"].contains("total")) {
            if (j["hits"]["total"].is_object()) {
                int totalHits = j["hits"]["total"]["value"].get<int>();
                LOG_INFO("Search found " + std::to_string(totalHits) + " total hits");
            } else {
                int totalHits = j["hits"]["total"].get<int>();
                LOG_INFO("Search found " + std::to_string(totalHits) + " total hits");
            }
        }
        
        std::vector<RawEvent> events;
        
        for (const auto& hit : j["hits"]["hits"]) {
            RawEvent event = jsonToEvent(hit["_source"].dump());
            events.push_back(event);
        }
        
        return events;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse search results: " + std::string(e.what()));
        LOG_ERROR("Response was: " + response);
        return {};
    }
}

bool ElasticsearchClient::updateDocument(
    const std::string& indexName,
    const std::string& docId,
    const std::string& updateScript) {
    
    std::string endpoint = "/" + indexName + "/_update/" + docId;
    
    std::string response;
    return httpRequest("POST", endpoint, updateScript, response);
}

bool ElasticsearchClient::httpRequest(
    const std::string& method,
    const std::string& endpoint,
    const std::string& body,
    std::string& response) {
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        return false;
    }
    
    std::string url = esUrl_ + endpoint;
    
    LOG_DEBUG("HTTP " + method + " " + url);
    if (!body.empty()) {
        LOG_DEBUG("Request body: " + body);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Special handling for HEAD requests
    if (method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // Don't download body
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }
    }
    
    // Headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
        +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
            ((std::string*)userp)->append((char*)contents, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);  // Connection timeout
    
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    bool success = (res == CURLE_OK);
    
    if (!success) {
        LOG_ERROR("HTTP request failed: " + std::string(curl_easy_strerror(res)));
    } else {
        LOG_DEBUG("HTTP response code: " + std::to_string(http_code));
        if (!response.empty()) {
            size_t maxLen = response.length() < 500 ? response.length() : 500;
            LOG_DEBUG("HTTP response: " + response.substr(0, maxLen)); 
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return success && (http_code >= 200 && http_code < 300);
}

std::string ElasticsearchClient::eventToJson(const RawEvent& event) const {
    json j = {
        {"event_id", event.eventId},
        {"timestamp", utils::timestampToISO8601(event.timestamp)},  // Use ISO8601 format
        {"device_id", event.deviceId},
        {"app_name", event.appName},
        {"interaction_count", event.interactionCount},
        {"dwell_time_seconds", event.dwellTimeSeconds},
        {"compressed", event.compressed},
        {"created_at", utils::timestampToISO8601(event.createdAt)}  // Use ISO8601 format
    };
    
    // Optional fields
    if (event.windowTitle.has_value()) {
        j["window_title"] = event.windowTitle.value();
    }
    if (event.url.has_value()) {
        j["url"] = event.url.value();
    }
    if (event.screenContent.has_value()) {
        j["screen_content"] = event.screenContent.value();
    }
    if (event.screenContentHash.has_value()) {
        j["screen_content_hash"] = event.screenContentHash.value();
    }
    if (event.voiceTranscription.has_value()) {
        j["voice_transcription"] = event.voiceTranscription.value();
    }
    if (event.cameraDescription.has_value()) {
        j["camera_description"] = event.cameraDescription.value();
    }
    if (event.sessionId.has_value()) {
        j["session_id"] = event.sessionId.value();
    }
    if (event.contentType.has_value()) {
        j["content_type"] = ContentTypeToString(event.contentType.value());
    }
    if (event.domain.has_value()) {
        j["domain"] = DomainToString(event.domain.value());
    }
    
    // Mouse events
    if (!event.mouseEvents.empty()) {
        json mouseEventsArray = json::array();
        for (const auto& me : event.mouseEvents) {
            mouseEventsArray.push_back({
                {"timestamp", utils::timestampToISO8601(me.timestamp)},  // Use ISO8601
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
    if (event.systemInfo.batteryPercent.has_value()) {
        sysInfo["battery_percent"] = event.systemInfo.batteryPercent.value();
    }
    sysInfo["is_charging"] = event.systemInfo.isCharging;
    if (!event.systemInfo.networkType.empty()) {
        sysInfo["network_type"] = event.systemInfo.networkType;
    }
    if (event.systemInfo.locationLat.has_value()) {
        sysInfo["location_lat"] = event.systemInfo.locationLat.value();
    }
    if (event.systemInfo.locationLon.has_value()) {
        sysInfo["location_lon"] = event.systemInfo.locationLon.value();
    }
    if (event.systemInfo.cpuUsage.has_value()) {
        sysInfo["cpu_usage"] = event.systemInfo.cpuUsage.value();
    }
    if (event.systemInfo.memoryUsage.has_value()) {
        sysInfo["memory_usage"] = event.systemInfo.memoryUsage.value();
    }
    
    if (!sysInfo.empty()) {
        j["system_info"] = sysInfo;
    }
    
    return j.dump();
}

RawEvent ElasticsearchClient::jsonToEvent(const std::string& jsonStr) const {
    RawEvent event;
    
    try {
        auto j = json::parse(jsonStr);
        
        // Required fields
        event.eventId = j["event_id"].get<std::string>();
        event.timestamp = utils::stringToTimestamp(j["timestamp"].get<std::string>());
        event.deviceId = j["device_id"].get<std::string>();
        event.appName = j["app_name"].get<std::string>();
        event.interactionCount = j.value("interaction_count", 0);
        event.dwellTimeSeconds = j.value("dwell_time_seconds", 0);
        event.compressed = j.value("compressed", false);
        
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
        if (j.contains("created_at") && !j["created_at"].is_null()) {
            event.createdAt = utils::stringToTimestamp(j["created_at"].get<std::string>());
        } else {
            event.createdAt = utils::now();
        }
        
        // Mouse events
        if (j.contains("mouse_events") && j["mouse_events"].is_array()) {
            for (const auto& me : j["mouse_events"]) {
                MouseEvent mouseEvent;
                mouseEvent.timestamp = utils::stringToTimestamp(me["timestamp"].get<std::string>());
                mouseEvent.eventType = me.value("event_type", "");
                mouseEvent.content = me.value("content", "");
                mouseEvent.posX = me.value("pos_x", 0);
                mouseEvent.posY = me.value("pos_y", 0);
                mouseEvent.elementType = me.value("element_type", "");
                event.mouseEvents.push_back(mouseEvent);
            }
        }
        
        // System info
        if (j.contains("system_info") && j["system_info"].is_object()) {
            const auto& sysInfo = j["system_info"];
            if (sysInfo.contains("battery_percent")) {
                event.systemInfo.batteryPercent = sysInfo["battery_percent"].get<int>();
            }
            if (sysInfo.contains("is_charging")) {
                event.systemInfo.isCharging = sysInfo["is_charging"].get<bool>();
            }
            if (sysInfo.contains("network_type")) {
                event.systemInfo.networkType = sysInfo["network_type"].get<std::string>();
            }
            if (sysInfo.contains("location_lat")) {
                event.systemInfo.locationLat = sysInfo["location_lat"].get<double>();
            }
            if (sysInfo.contains("location_lon")) {
                event.systemInfo.locationLon = sysInfo["location_lon"].get<double>();
            }
            if (sysInfo.contains("cpu_usage")) {
                event.systemInfo.cpuUsage = sysInfo["cpu_usage"].get<double>();
            }
            if (sysInfo.contains("memory_usage")) {
                event.systemInfo.memoryUsage = sysInfo["memory_usage"].get<double>();
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse JSON to RawEvent: " + std::string(e.what()));
    }
    
    return event;
}

} // namespace layer0
} // namespace perception