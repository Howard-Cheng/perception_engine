#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <variant>

namespace vectordb {

// Forward declarations
class QdrantClientImpl;

// Distance metric for vector similarity
enum class DistanceMetric {
    COSINE,    // Cosine similarity
    EUCLID,    // Euclidean distance
    DOT        // Dot product
};

// Point ID type (can be string or integer)
using PointId = std::variant<std::string, uint64_t>;

// Payload (metadata) type
using PayloadValue = std::variant<
    std::string,
    int64_t,
    double,
    bool,
    std::vector<std::string>,
    std::vector<int64_t>,
    std::vector<double>
>;
using Payload = std::map<std::string, PayloadValue>;

// Vector point structure
struct VectorPoint {
    PointId id;
    std::vector<float> vector;
    Payload payload;
    
    VectorPoint() = default;
    VectorPoint(PointId id, const std::vector<float>& vec, const Payload& payload = {})
        : id(std::move(id)), vector(vec), payload(payload) {}
};

// Search result structure
struct SearchResult {
    PointId id;
    float score;                    // Similarity score
    std::optional<Payload> payload; // Optional payload
    std::optional<std::vector<float>> vector; // Optional vector
    
    SearchResult(PointId id, float score)
        : id(std::move(id)), score(score) {}
};

// Collection information
struct CollectionInfo {
    std::string name;
    uint64_t pointsCount;
    uint64_t vectorsCount;
    size_t vectorSize;
    DistanceMetric distance;
    std::string status;
};

// Filter condition types
enum class FilterConditionType {
    MATCH,      // Exact match
    MATCH_TEXT, // Text match (substring)
    RANGE       // Range match
};

// Filter condition
struct FilterCondition {
    FilterConditionType type;
    std::string key;
    
    // For MATCH
    std::optional<PayloadValue> matchValue;
    
    // For MATCH_TEXT
    std::optional<std::string> matchText;
    
    // For RANGE
    std::optional<double> rangeGt;
    std::optional<double> rangeGte;
    std::optional<double> rangeLt;
    std::optional<double> rangeLte;
    
    // Constructor for MATCH
    static FilterCondition createMatch(const std::string& key, const PayloadValue& value) {
        FilterCondition cond;
        cond.type = FilterConditionType::MATCH;
        cond.key = key;
        cond.matchValue = value;
        return cond;
    }
    
    // Constructor for MATCH_TEXT
    static FilterCondition createMatchText(const std::string& key, const std::string& text) {
        FilterCondition cond;
        cond.type = FilterConditionType::MATCH_TEXT;
        cond.key = key;
        cond.matchText = text;
        return cond;
    }
    
    // Constructor for RANGE
    static FilterCondition createRange(
        const std::string& key,
        std::optional<double> gt = {},
        std::optional<double> gte = {},
        std::optional<double> lt = {},
        std::optional<double> lte = {}
    ) {
        FilterCondition cond;
        cond.type = FilterConditionType::RANGE;
        cond.key = key;
        cond.rangeGt = gt;
        cond.rangeGte = gte;
        cond.rangeLt = lt;
        cond.rangeLte = lte;
        return cond;
    }
};

// Filter structure (supports must, must_not, should)
struct Filter {
    std::vector<FilterCondition> must;      // All must be satisfied (AND)
    std::vector<FilterCondition> mustNot;   // All must not be satisfied (NOT)
    std::vector<FilterCondition> should;     // At least one must be satisfied (OR)
    
    Filter() = default;
    
    // Helper to create a simple filter with must conditions
    static Filter createMust(const std::vector<FilterCondition>& conditions) {
        Filter filter;
        filter.must = conditions;
        return filter;
    }
    
    // Helper to create a simple filter with must_not conditions
    static Filter createMustNot(const std::vector<FilterCondition>& conditions) {
        Filter filter;
        filter.mustNot = conditions;
        return filter;
    }
    
    // Helper to create a simple filter with should conditions
    static Filter createShould(const std::vector<FilterCondition>& conditions) {
        Filter filter;
        filter.should = conditions;
        return filter;
    }
};

/**
 * Qdrant Client for vector database operations.
 * 
 * Provides interface for collection management, vector storage, and search operations.
 */
class QdrantClient {
public:
    // Connection configuration
    struct Config {
        std::string url = "http://localhost:6333";  // Qdrant server URL
        std::optional<std::string> apiKey;         // Optional API key for authentication
        float timeout = 30.0f;                     // Request timeout in seconds
        
        // Create config for Qdrant server connection
        static Config remote(const std::string& url, 
                            const std::optional<std::string>& apiKey = {}) {
            Config config;
            config.url = url;
            config.apiKey = apiKey;
            return config;
        }
    };
    
    /**
     * Constructor.
     * 
     * @param config Connection configuration
     */
    explicit QdrantClient(const Config& config);
    
    /**
     * Destructor.
     */
    ~QdrantClient();
    
    // Delete copy constructor and assignment
    QdrantClient(const QdrantClient&) = delete;
    QdrantClient& operator=(const QdrantClient&) = delete;
    
    // Move constructor and assignment
    QdrantClient(QdrantClient&&) noexcept;
    QdrantClient& operator=(QdrantClient&&) noexcept;
    
    // ========================================================================
    // COLLECTION MANAGEMENT
    // ========================================================================
    
    /**
     * Create a new collection.
     * 
     * @param collectionName Name of the collection
     * @param vectorSize Dimension of vectors in this collection
     * @param distance Distance metric (default: COSINE)
     * @param recreate If true, delete existing collection before creating
     * @return true if collection was created successfully
     */
    bool createCollection(
        const std::string& collectionName,
        size_t vectorSize,
        DistanceMetric distance = DistanceMetric::COSINE,
        bool recreate = false
    );
    
    /**
     * Delete a collection.
     * 
     * @param collectionName Name of the collection to delete
     * @return true if collection was deleted successfully
     */
    bool deleteCollection(const std::string& collectionName);
    
    /**
     * Check if a collection exists.
     * 
     * @param collectionName Name of the collection to check
     * @return true if collection exists
     */
    bool collectionExists(const std::string& collectionName);
    
    /**
     * List all collection names.
     * 
     * @return Vector of collection names
     */
    std::vector<std::string> listCollections();
    
    /**
     * Get information about a collection.
     * 
     * @param collectionName Name of the collection
     * @return Collection information, or nullopt if not found
     */
    std::optional<CollectionInfo> getCollectionInfo(const std::string& collectionName);
    
    // ========================================================================
    // VECTOR OPERATIONS
    // ========================================================================
    
    /**
     * Insert or update points in a collection.
     * 
     * @param collectionName Name of the collection
     * @param points Vector of points to upsert
     * @return true if operation succeeded
     */
    bool upsert(
        const std::string& collectionName,
        const std::vector<VectorPoint>& points
    );
    
    /**
     * Insert or update a single point in a collection.
     * 
     * @param collectionName Name of the collection
     * @param point Point to upsert
     * @return true if operation succeeded
     */
    bool upsert(
        const std::string& collectionName,
        const VectorPoint& point
    );
    
    /**
     * Search for similar vectors in a collection.
     * 
     * @param collectionName Name of the collection
     * @param queryVector Query vector to search for
     * @param limit Maximum number of results to return (default: 10)
     * @param scoreThreshold Minimum similarity score threshold (optional)
     * @param filter Optional metadata filter
     * @param withPayload Whether to include payload in results (default: true)
     * @param withVectors Whether to include vectors in results (default: false)
     * @return Vector of search results
     */
    std::vector<SearchResult> search(
        const std::string& collectionName,
        const std::vector<float>& queryVector,
        size_t limit = 10,
        std::optional<float> scoreThreshold = {},
        const std::optional<Filter>& filter = {},
        bool withPayload = true,
        bool withVectors = false
    );
    
    /**
     * Delete points from a collection by IDs.
     * 
     * @param collectionName Name of the collection
     * @param pointIds Vector of point IDs to delete
     * @return true if operation succeeded
     */
    bool deletePoints(
        const std::string& collectionName,
        const std::vector<PointId>& pointIds
    );
    
    /**
     * Delete points from a collection by metadata filter.
     * 
     * @param collectionName Name of the collection
     * @param filter Metadata filter to match points for deletion
     * @return true if operation succeeded
     */
    bool deletePointsByFilter(
        const std::string& collectionName,
        const Filter& filter
    );
    
    /**
     * Retrieve points by IDs.
     * 
     * @param collectionName Name of the collection
     * @param pointIds Vector of point IDs to retrieve
     * @param withPayload Whether to include payload (default: true)
     * @param withVectors Whether to include vectors (default: false)
     * @return Vector of retrieved points
     */
    std::vector<VectorPoint> retrieve(
        const std::string& collectionName,
        const std::vector<PointId>& pointIds,
        bool withPayload = true,
        bool withVectors = false
    );
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * Test connection to Qdrant server.
     * 
     * @return true if connection is successful
     */
    bool testConnection();
    
    /**
     * Get the last error message (if any).
     * 
     * @return Error message string
     */
    std::string getLastError() const;

private:
    std::unique_ptr<QdrantClientImpl> impl_;
};

} // namespace vectordb

