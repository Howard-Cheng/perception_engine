#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <set>
#include "common/Types.h"

namespace perception {
namespace utils {

// String utilities
std::string toLower(const std::string& str);
std::string toUpper(const std::string& str);
std::string trim(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);
bool contains(const std::string& str, const std::string& substring);

// Hash utilities
std::string computeMD5(const std::string& data);
std::string generateUUID();

// Time utilities
std::string timestampToISO8601(const Timestamp& timestamp);
std::string timestampToString(const Timestamp& timestamp);  // For DuckDB TIMESTAMP format
Timestamp iso8601ToTimestamp(const std::string& iso8601);
Timestamp stringToTimestamp(const std::string& str);  // Parse timestamp from string
int64_t getTimestampSeconds(const Timestamp& timestamp);
Timestamp now();

// JSON utilities
std::string escapeJson(const std::string& str);
std::string vectorToJsonArray(const std::vector<std::string>& vec);

// Regex pattern matching
std::vector<std::string> extractNumbers(const std::string& text);
std::vector<std::string> extractDates(const std::string& text);
std::vector<std::string> extractUrls(const std::string& text);
std::vector<std::string> extractEmails(const std::string& text);

// URL utilities
std::string extractDomain(const std::string& url);

// File utilities
bool fileExists(const std::filesystem::path& path);
void ensureDirectoryExists(const std::filesystem::path& path);
int64_t getFileSize(const std::filesystem::path& path);

// Deduplication utility
template<typename T>
std::vector<T> deduplicate(const std::vector<T>& vec) {
    std::vector<T> result;
    std::set<T> seen;
    for (const auto& item : vec) {
        if (seen.find(item) == seen.end()) {
            seen.insert(item);
            result.push_back(item);
        }
    }
    return result;
}

} // namespace utils
} // namespace perception
