#include "FastVLMTokenizer.h"
#include <iostream>

bool FastVLMTokenizer::LoadVocab(const std::string& vocabPath) {
    std::ifstream file(vocabPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open vocab file: " << vocabPath << std::endl;
        return false;
    }

    // Read entire file
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();

    // Simple JSON parsing (vocab.json is {"token": id, ...})
    vocab_.clear();

    size_t pos = json.find('{');
    if (pos == std::string::npos) {
        std::cerr << "Invalid JSON format" << std::endl;
        return false;
    }
    pos++;

    int entries = 0;
    while (pos < json.length()) {
        // Skip whitespace
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
            pos++;
        }

        // Check for end of object
        if (json[pos] == '}') break;

        // Skip comma
        if (json[pos] == ',') {
            pos++;
            continue;
        }

        // Parse token string (key)
        if (json[pos] != '"') {
            std::cerr << "Expected quote at position " << pos << std::endl;
            return false;
        }
        std::string token = ParseJsonString(json, pos);

        // Skip whitespace and colon
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':')) {
            pos++;
        }

        // Parse token ID (value)
        int64_t tokenId = ParseJsonNumber(json, pos);

        // Store in reverse map (ID -> token)
        vocab_[tokenId] = token;
        entries++;
    }

    std::cout << "Loaded " << entries << " tokens from vocabulary" << std::endl;
    return entries > 0;
}

std::string FastVLMTokenizer::Decode(const std::vector<int64_t>& tokens) const {
    std::string result;
    result.reserve(tokens.size() * 4); // Rough estimate

    for (int64_t tokenId : tokens) {
        // Stop at EOS token
        if (tokenId == EOS_TOKEN_ID) {
            break;
        }

        auto it = vocab_.find(tokenId);
        if (it != vocab_.end()) {
            result += it->second;
        } else {
            // Unknown token - skip
            std::cerr << "Warning: Unknown token ID " << tokenId << std::endl;
        }
    }

    return result;
}

std::string FastVLMTokenizer::ParseJsonString(const std::string& json, size_t& pos) {
    std::string result;
    pos++; // Skip opening quote

    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\') {
            // Handle escape sequences
            pos++;
            if (pos >= json.length()) break;

            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                case 'u': {
                    // Unicode escape \uXXXX
                    if (pos + 4 < json.length()) {
                        std::string hex = json.substr(pos + 1, 4);
                        int codepoint = std::stoi(hex, nullptr, 16);
                        if (codepoint < 128) {
                            result += static_cast<char>(codepoint);
                        } else {
                            // For simplicity, skip non-ASCII unicode
                            result += "?";
                        }
                        pos += 4;
                    }
                    break;
                }
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }

    pos++; // Skip closing quote
    return result;
}

int64_t FastVLMTokenizer::ParseJsonNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '-')) {
        pos++;
    }
    return std::stoll(json.substr(start, pos - start));
}
