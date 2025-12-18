#include "QdrantClient.h"
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <cmath>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace vectordb
{

    // CURL write callback
    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        ((std::string *)userp)->append((char *)contents, size * nmemb);
        return size * nmemb;
    }

    // Forward declaration of implementation class
    class QdrantClientImpl
    {
    public:
        QdrantClient::Config config;
        std::string lastError;

        QdrantClientImpl(const QdrantClient::Config &cfg) : config(cfg)
        {
            lastError.clear();
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }

        ~QdrantClientImpl()
        {
            curl_global_cleanup();
        }

        // HTTP request helper
        bool httpRequest(
            const std::string &method,
            const std::string &endpoint,
            const std::string &body,
            std::string &response,
            long &httpCode)
        {
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                lastError = "Failed to initialize CURL";
                return false;
            }

            std::string url = config.url;
            if (!url.empty() && url.back() == '/')
            {
                url.pop_back();
            }
            url += endpoint;

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

            if (!body.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }

            struct curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            if (config.apiKey.has_value())
            {
                std::string apiKeyHeader = "api-key: " + config.apiKey.value();
                headers = curl_slist_append(headers, apiKeyHeader.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config.timeout));
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK)
            {
                lastError = "CURL error: " + std::string(curl_easy_strerror(res));
                return false;
            }

            if (httpCode >= 200 && httpCode < 300)
            {
                return true;
            }
            else
            {
                lastError = "HTTP error: " + std::to_string(httpCode) + " - " + response;
                return false;
            }
        }

        // Helper: Convert DistanceMetric to Qdrant string
        std::string distanceToString(DistanceMetric distance)
        {
            switch (distance)
            {
            case DistanceMetric::COSINE:
                return "Cosine";
            case DistanceMetric::EUCLID:
                return "Euclid";
            case DistanceMetric::DOT:
                return "Dot";
            default:
                return "Cosine";
            }
        }

        // Helper: Convert Qdrant string to DistanceMetric
        DistanceMetric stringToDistance(const std::string &distStr)
        {
            if (distStr == "Cosine")
                return DistanceMetric::COSINE;
            if (distStr == "Euclid")
                return DistanceMetric::EUCLID;
            if (distStr == "Dot")
                return DistanceMetric::DOT;
            return DistanceMetric::COSINE;
        }

        // Helper: Serialize PointId to JSON
        json serializePointId(const PointId &id)
        {
            if (std::holds_alternative<std::string>(id))
            {
                return json(std::get<std::string>(id));
            }
            else
            {
                return json(std::get<uint64_t>(id));
            }
        }

        // Helper: Parse PointId from JSON
        PointId parsePointId(const json &idJson)
        {
            if (idJson.is_string())
            {
                return idJson.get<std::string>();
            }
            else if (idJson.is_number_unsigned())
            {
                return idJson.get<uint64_t>();
            }
            else
            {
                throw std::runtime_error("Invalid point ID format");
            }
        }

        // Helper: Serialize Payload to JSON
        json serializePayload(const Payload &payload)
        {
            json payloadJson = json::object();
            for (const auto &[key, value] : payload)
            {
                if (std::holds_alternative<std::string>(value))
                {
                    payloadJson[key] = std::get<std::string>(value);
                }
                else if (std::holds_alternative<int64_t>(value))
                {
                    payloadJson[key] = std::get<int64_t>(value);
                }
                else if (std::holds_alternative<double>(value))
                {
                    payloadJson[key] = std::get<double>(value);
                }
                else if (std::holds_alternative<bool>(value))
                {
                    payloadJson[key] = std::get<bool>(value);
                }
                else if (std::holds_alternative<std::vector<std::string>>(value))
                {
                    payloadJson[key] = std::get<std::vector<std::string>>(value);
                }
                else if (std::holds_alternative<std::vector<int64_t>>(value))
                {
                    payloadJson[key] = std::get<std::vector<int64_t>>(value);
                }
                else if (std::holds_alternative<std::vector<double>>(value))
                {
                    payloadJson[key] = std::get<std::vector<double>>(value);
                }
            }
            return payloadJson;
        }

        // Helper: Parse Payload from JSON
        Payload parsePayload(const json &payloadJson)
        {
            Payload payload;
            if (!payloadJson.is_object())
            {
                return payload;
            }

            for (const auto &[key, value] : payloadJson.items())
            {
                if (value.is_string())
                {
                    payload[key] = value.get<std::string>();
                }
                else if (value.is_number_integer())
                {
                    payload[key] = value.get<int64_t>();
                }
                else if (value.is_number_float())
                {
                    payload[key] = value.get<double>();
                }
                else if (value.is_boolean())
                {
                    payload[key] = value.get<bool>();
                }
                else if (value.is_array())
                {
                    if (!value.empty())
                    {
                        if (value[0].is_string())
                        {
                            payload[key] = value.get<std::vector<std::string>>();
                        }
                        else if (value[0].is_number_integer())
                        {
                            payload[key] = value.get<std::vector<int64_t>>();
                        }
                        else if (value[0].is_number_float())
                        {
                            payload[key] = value.get<std::vector<double>>();
                        }
                    }
                }
            }
            return payload;
        }

        // Helper: Serialize Filter to Qdrant format
        json serializeFilter(const Filter &filter)
        {
            json filterJson = json::object();

            if (!filter.must.empty())
            {
                filterJson["must"] = json::array();
                for (const auto &cond : filter.must)
                {
                    filterJson["must"].push_back(serializeFilterCondition(cond));
                }
            }

            if (!filter.mustNot.empty())
            {
                filterJson["must_not"] = json::array();
                for (const auto &cond : filter.mustNot)
                {
                    filterJson["must_not"].push_back(serializeFilterCondition(cond));
                }
            }

            if (!filter.should.empty())
            {
                filterJson["should"] = json::array();
                for (const auto &cond : filter.should)
                {
                    filterJson["should"].push_back(serializeFilterCondition(cond));
                }
            }

            return filterJson.empty() ? json() : filterJson;
        }

        // Helper: Serialize FilterCondition to Qdrant format
        json serializeFilterCondition(const FilterCondition &cond)
        {
            json condJson = json::object();

            switch (cond.type)
            {
            case FilterConditionType::MATCH:
            {
                if (cond.matchValue.has_value())
                {
                    const auto &value = cond.matchValue.value();
                    json matchJson = json::object();
                    matchJson["key"] = cond.key;
                    if (std::holds_alternative<std::string>(value))
                    {
                        matchJson["match"] = json::object();
                        matchJson["match"]["value"] = std::get<std::string>(value);
                    }
                    else if (std::holds_alternative<int64_t>(value))
                    {
                        matchJson["match"] = json::object();
                        matchJson["match"]["value"] = std::get<int64_t>(value);
                    }
                    else if (std::holds_alternative<double>(value))
                    {
                        matchJson["match"] = json::object();
                        matchJson["match"]["value"] = std::get<double>(value);
                    }
                    else if (std::holds_alternative<bool>(value))
                    {
                        matchJson["match"] = json::object();
                        matchJson["match"]["value"] = std::get<bool>(value);
                    }
                    return matchJson;
                }
                break;
            }
            case FilterConditionType::MATCH_TEXT:
            {
                if (cond.matchText.has_value())
                {
                    json matchJson = json::object();
                    matchJson["key"] = cond.key;
                    matchJson["match"] = json::object();
                    matchJson["match"]["text"] = cond.matchText.value();
                    return matchJson;
                }
                break;
            }
            case FilterConditionType::RANGE:
            {
                json rangeJson = json::object();
                rangeJson["key"] = cond.key;
                json range = json::object();
                if (cond.rangeGt.has_value())
                {
                    range["gt"] = cond.rangeGt.value();
                }
                if (cond.rangeGte.has_value())
                {
                    range["gte"] = cond.rangeGte.value();
                }
                if (cond.rangeLt.has_value())
                {
                    range["lt"] = cond.rangeLt.value();
                }
                if (cond.rangeLte.has_value())
                {
                    range["lte"] = cond.rangeLte.value();
                }
                rangeJson["range"] = range;
                return rangeJson;
            }
            }

            return condJson;
        }
    };

    QdrantClient::QdrantClient(const Config &config)
        : impl_(std::make_unique<QdrantClientImpl>(config))
    {
    }

    QdrantClient::~QdrantClient() = default;

    QdrantClient::QdrantClient(QdrantClient &&) noexcept = default;
    QdrantClient &QdrantClient::operator=(QdrantClient &&) noexcept = default;

    // ========================================================================
    // COLLECTION MANAGEMENT
    // ========================================================================

    bool QdrantClient::createCollection(
        const std::string &collectionName,
        size_t vectorSize,
        DistanceMetric distance,
        bool recreate)
    {
        try
        {
            if (recreate && collectionExists(collectionName))
            {
                deleteCollection(collectionName);
            }

            // Build request body
            json requestBody = json::object();
            requestBody["vectors"] = json::object();
            requestBody["vectors"]["size"] = vectorSize;
            requestBody["vectors"]["distance"] = impl_->distanceToString(distance);

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName;
            if (!impl_->httpRequest("PUT", endpoint, body, response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to create collection: " + std::string(e.what());
            return false;
        }
    }

    bool QdrantClient::deleteCollection(const std::string &collectionName)
    {
        try
        {
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName;
            if (!impl_->httpRequest("DELETE", endpoint, "", response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to delete collection: " + std::string(e.what());
            return false;
        }
    }

    bool QdrantClient::collectionExists(const std::string &collectionName)
    {
        try
        {
            auto collections = listCollections();
            return std::find(collections.begin(), collections.end(), collectionName) != collections.end();
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to check collection existence: " + std::string(e.what());
            return false;
        }
    }

    std::vector<std::string> QdrantClient::listCollections()
    {
        try
        {
            std::string response;
            long httpCode = 0;

            if (!impl_->httpRequest("GET", "/collections", "", response, httpCode))
            {
                return {};
            }

            json result = json::parse(response);
            std::vector<std::string> collections;

            if (result.contains("result") && result["result"].contains("collections"))
            {
                for (const auto &collection : result["result"]["collections"])
                {
                    if (collection.contains("name"))
                    {
                        collections.push_back(collection["name"].get<std::string>());
                    }
                }
            }

            impl_->lastError.clear();
            return collections;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to list collections: " + std::string(e.what());
            return {};
        }
    }

    std::optional<CollectionInfo> QdrantClient::getCollectionInfo(const std::string &collectionName)
    {
        try
        {
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName;
            if (!impl_->httpRequest("GET", endpoint, "", response, httpCode))
            {
                return std::nullopt;
            }

            json result = json::parse(response);
            if (!result.contains("result"))
            {
                impl_->lastError = "Invalid response format";
                return std::nullopt;
            }

            json collectionInfo = result["result"];
            CollectionInfo info;
            info.name = collectionName;

            if (collectionInfo.contains("vectors_count"))
            {
                info.vectorsCount = collectionInfo["vectors_count"].get<uint64_t>();
            }
            if (collectionInfo.contains("points_count"))
            {
                info.pointsCount = collectionInfo["points_count"].get<uint64_t>();
            }
            if (collectionInfo.contains("config") && collectionInfo["config"].contains("params"))
            {
                auto params = collectionInfo["config"]["params"];
                if (params.contains("vectors") && params["vectors"].contains("size"))
                {
                    info.vectorSize = params["vectors"]["size"].get<size_t>();
                }
                if (params.contains("vectors") && params["vectors"].contains("distance"))
                {
                    std::string distStr = params["vectors"]["distance"].get<std::string>();
                    info.distance = impl_->stringToDistance(distStr);
                }
            }
            if (collectionInfo.contains("status"))
            {
                info.status = collectionInfo["status"].get<std::string>();
            }
            else
            {
                info.status = "green";
            }

            impl_->lastError.clear();
            return info;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to get collection info: " + std::string(e.what());
            return std::nullopt;
        }
    }

    // ========================================================================
    // VECTOR OPERATIONS
    // ========================================================================

    bool QdrantClient::upsert(
        const std::string &collectionName,
        const std::vector<VectorPoint> &points)
    {
        try
        {
            if (points.empty())
            {
                return true;
            }

            // Build request body
            json requestBody = json::object();
            json pointsJson = json::array();

            for (const auto &point : points)
            {
                json pointJson = json::object();
                pointJson["id"] = impl_->serializePointId(point.id);
                pointJson["vector"] = point.vector;
                if (!point.payload.empty())
                {
                    pointJson["payload"] = impl_->serializePayload(point.payload);
                }
                pointsJson.push_back(pointJson);
            }

            requestBody["points"] = pointsJson;

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            // Add wait=true parameter to ensure points are written before returning
            std::string endpoint = "/collections/" + collectionName + "/points?wait=true";
            if (!impl_->httpRequest("PUT", endpoint, body, response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to upsert points: " + std::string(e.what());
            return false;
        }
    }

    bool QdrantClient::upsert(
        const std::string &collectionName,
        const VectorPoint &point)
    {
        return upsert(collectionName, std::vector<VectorPoint>{point});
    }

    std::vector<SearchResult> QdrantClient::search(
        const std::string &collectionName,
        const std::vector<float> &queryVector,
        size_t limit,
        std::optional<float> scoreThreshold,
        const std::optional<Filter> &filter,
        bool withPayload,
        bool withVectors)
    {
        try
        {
            // Build request body
            json requestBody = json::object();
            requestBody["vector"] = queryVector;
            requestBody["limit"] = limit;
            requestBody["with_payload"] = withPayload;
            requestBody["with_vector"] = withVectors;

            if (scoreThreshold.has_value())
            {
                requestBody["score_threshold"] = scoreThreshold.value();
            }

            if (filter.has_value())
            {
                json filterJson = impl_->serializeFilter(filter.value());
                if (!filterJson.is_null())
                {
                    requestBody["filter"] = filterJson;
                }
            }

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName + "/points/search";
            if (!impl_->httpRequest("POST", endpoint, body, response, httpCode))
            {
                return {};
            }

            json result = json::parse(response);
            std::vector<SearchResult> results;

            if (result.contains("result"))
            {
                for (const auto &item : result["result"])
                {
                    PointId id = impl_->parsePointId(item["id"]);
                    float score = item.contains("score") ? item["score"].get<float>() : 0.0f;

                    SearchResult searchResult(id, score);

                    if (withPayload && item.contains("payload"))
                    {
                        searchResult.payload = impl_->parsePayload(item["payload"]);
                    }

                    if (withVectors && item.contains("vector"))
                    {
                        searchResult.vector = item["vector"].get<std::vector<float>>();
                    }

                    results.push_back(searchResult);
                }
            }

            impl_->lastError.clear();
            return results;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to search: " + std::string(e.what());
            return {};
        }
    }

    bool QdrantClient::deletePoints(
        const std::string &collectionName,
        const std::vector<PointId> &pointIds)
    {
        try
        {
            if (pointIds.empty())
            {
                return true;
            }

            // Build request body
            json requestBody = json::object();
            json idsJson = json::array();

            for (const auto &id : pointIds)
            {
                idsJson.push_back(impl_->serializePointId(id));
            }

            requestBody["points"] = idsJson;

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName + "/points/delete";
            if (!impl_->httpRequest("POST", endpoint, body, response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to delete points: " + std::string(e.what());
            return false;
        }
    }

    bool QdrantClient::deletePointsByFilter(
        const std::string &collectionName,
        const Filter &filter)
    {
        try
        {
            // Build request body
            json requestBody = json::object();
            json filterJson = impl_->serializeFilter(filter);
            if (!filterJson.is_null())
            {
                requestBody["filter"] = filterJson;
            }

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName + "/points/delete";
            if (!impl_->httpRequest("POST", endpoint, body, response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to delete points by filter: " + std::string(e.what());
            return false;
        }
    }

    std::vector<VectorPoint> QdrantClient::retrieve(
        const std::string &collectionName,
        const std::vector<PointId> &pointIds,
        bool withPayload,
        bool withVectors)
    {
        try
        {
            if (pointIds.empty())
            {
                return {};
            }

            // Build request body
            json requestBody = json::object();
            json idsJson = json::array();

            for (const auto &id : pointIds)
            {
                idsJson.push_back(impl_->serializePointId(id));
            }

            requestBody["ids"] = idsJson;
            requestBody["with_payload"] = withPayload;
            requestBody["with_vector"] = withVectors;

            std::string body = requestBody.dump();
            std::string response;
            long httpCode = 0;

            std::string endpoint = "/collections/" + collectionName + "/points";
            if (!impl_->httpRequest("POST", endpoint, body, response, httpCode))
            {
                return {};
            }

            json result = json::parse(response);
            std::vector<VectorPoint> points;

            if (result.contains("result"))
            {
                for (const auto &item : result["result"])
                {
                    VectorPoint point;
                    point.id = impl_->parsePointId(item["id"]);

                    if (withVectors && item.contains("vector"))
                    {
                        point.vector = item["vector"].get<std::vector<float>>();
                    }

                    if (withPayload && item.contains("payload"))
                    {
                        point.payload = impl_->parsePayload(item["payload"]);
                    }

                    points.push_back(point);
                }
            }

            impl_->lastError.clear();
            return points;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Failed to retrieve points: " + std::string(e.what());
            return {};
        }
    }

    // ========================================================================
    // UTILITY METHODS
    // ========================================================================

    bool QdrantClient::testConnection()
    {
        try
        {
            std::string response;
            long httpCode = 0;

            if (!impl_->httpRequest("GET", "/", "", response, httpCode))
            {
                return false;
            }

            impl_->lastError.clear();
            return true;
        }
        catch (const std::exception &e)
        {
            impl_->lastError = "Connection test failed: " + std::string(e.what());
            return false;
        }
    }

    std::string QdrantClient::getLastError() const
    {
        return impl_->lastError;
    }

} // namespace vectordb
