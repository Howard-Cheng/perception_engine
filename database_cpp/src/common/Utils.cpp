#include "common/Utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <random>
#include <fstream>

// Include OpenSSL only if available
#ifndef NO_OPENSSL
#include <openssl/md5.h>
#else
// Simple fallback MD5 implementation
// This is a basic implementation for when OpenSSL is not available
// For production use, it's recommended to use OpenSSL
namespace {
    // Simple hash function as MD5 fallback (not cryptographically secure)
    std::string simpleMD5Fallback(const std::string& data) {
        // Use std::hash as a simple alternative
        std::hash<std::string> hasher;
        size_t hashValue = hasher(data);
        
        // Convert to 32-character hex string to mimic MD5 format
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(16) << hashValue;
        ss << std::setw(16) << (hashValue ^ 0xDEADBEEF); // Add some variation
        return ss.str();
    }
}
#endif

namespace perception {
namespace utils {

std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && 
           str.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}

std::string computeMD5(const std::string& data) {
#ifndef NO_OPENSSL
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(digest[i]);
    }
    return ss.str();
#else
    // Use fallback implementation
    return simpleMD5Fallback(data);
#endif
}

std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4"; // Version 4 UUID
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen); // Variant bits
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

std::string timestampToISO8601(const Timestamp& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

std::string timestampToString(const Timestamp& timestamp) {
    // Format for DuckDB TIMESTAMP: 'YYYY-MM-DD HH:MM:SS.mmm'
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

Timestamp stringToTimestamp(const std::string& str) {
    // Parse timestamp from string (handles both ISO8601 and DuckDB format)
    std::tm tm = {};
    std::istringstream ss(str);
    
    // Try YYYY-MM-DD HH:MM:SS format first
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    if (ss.fail()) {
        // Try ISO8601 format
        ss.clear();
        ss.str(str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    
    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
}

Timestamp iso8601ToTimestamp(const std::string& iso8601) {
    // Simple parser for ISO 8601 format
    std::tm tm = {};
    std::istringstream ss(iso8601);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return tp;
}

int64_t getTimestampSeconds(const Timestamp& timestamp) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
}

Timestamp now() {
    return std::chrono::system_clock::now();
}

std::string escapeJson(const std::string& str) {
    std::stringstream ss;
    for (char c : str) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:   ss << c;      break;
        }
    }
    return ss.str();
}

std::string vectorToJsonArray(const std::vector<std::string>& vec) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << escapeJson(vec[i]) << "\"";
    }
    ss << "]";
    return ss.str();
}

std::vector<std::string> extractNumbers(const std::string& text) {
    std::vector<std::string> numbers;
    std::regex pattern(R"(\$?\d{1,3}(?:,\d{3})*(?:\.\d+)?[MKB%]?)");
    std::sregex_iterator iter(text.begin(), text.end(), pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        numbers.push_back(iter->str());
    }
    return numbers;
}

std::vector<std::string> extractDates(const std::string& text) {
    std::vector<std::string> dates;
    //std::regex pattern(R"(\b\d{1,2}[/-]\d{1,2}[/-]\d{2,4}\b|"
    //                   R"(\b(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]* \d{1,2},? \d{4}\b|"
    //                   R"(\bQ[1-4] \d{4}\b)");
    //std::sregex_iterator iter(text.begin(), text.end(), pattern);
    //std::sregex_iterator end;
    //
    //for (; iter != end; ++iter) {
    //    dates.push_back(iter->str());
    //}
    return dates;
}

std::vector<std::string> extractUrls(const std::string& text) {
    std::vector<std::string> urls;
    std::regex pattern(R"(https?://[^\s<>"{}|\\^`\[\]]+)");
    std::sregex_iterator iter(text.begin(), text.end(), pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        urls.push_back(iter->str());
    }
    return urls;
}

std::vector<std::string> extractEmails(const std::string& text) {
    std::vector<std::string> emails;
    std::regex pattern(R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
    std::sregex_iterator iter(text.begin(), text.end(), pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        emails.push_back(iter->str());
    }
    return emails;
}

std::string extractDomain(const std::string& url) {
    std::regex pattern(R"((?:https?://)?(?:www\.)?([^/]+))");
    std::smatch match;
    if (std::regex_search(url, match, pattern) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

bool fileExists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

void ensureDirectoryExists(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}

int64_t getFileSize(const std::filesystem::path& path) {
    if (fileExists(path)) {
        return std::filesystem::file_size(path);
    }
    return 0;
}

} // namespace utils
} // namespace perception
