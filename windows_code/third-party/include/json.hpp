// This is a simplified JSON implementation for C++14 compatibility
// In a real project, you would use nlohmann/json library
#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>

class Json {
private:
    std::map<std::string, std::string> data;
    std::map<std::string, bool> isRawValue; // Track which values are raw JSON (no quotes)
    
public:
    Json() = default;
    
    Json(std::initializer_list<std::pair<std::string, std::string>> init) {
        for (const auto& pair : init) {
            data[pair.first] = pair.second;
            isRawValue[pair.first] = false;
        }
    }
    
    void set(const std::string& key, const std::string& value) {
        data[key] = value;
        isRawValue[key] = false;
    }
    
    void set(const std::string& key, int value) {
        data[key] = std::to_string(value);
        isRawValue[key] = true; // Numbers don't need quotes
    }
    
    void set(const std::string& key, bool value) {
        data[key] = value ? "true" : "false";
        isRawValue[key] = true; // Booleans don't need quotes
    }
    
    // New method for setting raw JSON values (arrays, objects, null)
    void setRaw(const std::string& key, const std::string& rawJsonValue) {
        data[key] = rawJsonValue;
        isRawValue[key] = true;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : defaultValue;
    }

    int getInt(const std::string& key, int defaultValue = 0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    double getDouble(const std::string& key, double defaultValue = 0.0) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::stod(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    bool getBool(const std::string& key, bool defaultValue = false) const {
        auto it = data.find(key);
        if (it != data.end()) {
            return (it->second == "true" || it->second == "1");
        }
        return defaultValue;
    }

    // Helper function to escape JSON strings
    static std::string escapeJsonString(const std::string& input) {
        std::ostringstream oss;
        for (char c : input) {
            switch (c) {
                case '\"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        // Control characters - escape as \uXXXX
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    } else {
                        oss << c;
                    }
            }
        }
        return oss.str();
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& pair : data) {
            if (!first) oss << ",";
            oss << "\"" << pair.first << "\":";

            // Check if this value should be quoted or raw
            auto rawIt = isRawValue.find(pair.first);
            bool isRaw = (rawIt != isRawValue.end()) && rawIt->second;

            if (isRaw) {
                oss << pair.second; // Raw JSON value (no quotes)
            } else {
                oss << "\"" << escapeJsonString(pair.second) << "\""; // Quoted and escaped string value
            }
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};