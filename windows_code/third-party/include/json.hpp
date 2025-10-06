// This is a simplified JSON implementation for C++14 compatibility
// In a real project, you would use nlohmann/json library
#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>

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
                oss << "\"" << pair.second << "\""; // Quoted string value
            }
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};