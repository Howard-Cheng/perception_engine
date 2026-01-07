#include "DocumentChunker.h"
#include "GemmaTokenizer.h"
#include "pe_base/logger.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <set>

using json = nlohmann::json;

namespace embedding {

DocumentChunker::DocumentChunker(const std::string& tokenizer_path, int max_seq_length)
    : tokenizer_(nullptr)
    , max_seq_length_(max_seq_length)
    , is_ready_(false)
    , last_error_("") {
    
    try {
        // Initialize tokenizer
        tokenizer_ = new GemmaTokenizer(tokenizer_path);
        
        if (!tokenizer_->isLoaded()) {
            last_error_ = "Failed to load tokenizer: " + tokenizer_->getLastError();
            PE_ERROR("[DocumentChunker] " + last_error_);
            delete tokenizer_;
            tokenizer_ = nullptr;
            return;
        }
        
        is_ready_ = true;
        PE_INFO("[DocumentChunker] Initialized successfully with tokenizer at: " + tokenizer_path);
        
    } catch (const std::exception& e) {
        last_error_ = std::string("Exception initializing DocumentChunker: ") + e.what();
        PE_ERROR("[DocumentChunker] " + last_error_);
        if (tokenizer_) {
            delete tokenizer_;
            tokenizer_ = nullptr;
        }
    }
}

DocumentChunker::~DocumentChunker() {
    if (tokenizer_) {
        delete tokenizer_;
        tokenizer_ = nullptr;
    }
}

// Helper: Trim whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Helper: Check if string looks like JSON
static bool isJSON(const std::string& text) {
    std::string trimmed = trim(text);
    return (!trimmed.empty() && (trimmed[0] == '{' || trimmed[0] == '['));
}

// Helper: Recursively extract content fields from JSON
static void extractContentFieldsRecursive(const json& j, std::vector<std::string>& content_parts) {
    if (j.is_object()) {
        // Check for screen_content field
        if (j.contains("screen_content") && j["screen_content"].is_string()) {
            std::string content = j["screen_content"].get<std::string>();
            content = trim(content);
            if (!content.empty()) {
                content_parts.push_back(content);
            }
        }
        
        // Check for window_title field
        if (j.contains("window_title") && j["window_title"].is_string()) {
            std::string content = j["window_title"].get<std::string>();
            content = trim(content);
            if (!content.empty()) {
                content_parts.push_back(content);
            }
        }
        
        // Recursively process all nested objects
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.value().is_object() || it.value().is_array()) {
                extractContentFieldsRecursive(it.value(), content_parts);
            }
        }
    } else if (j.is_array()) {
        // Process all array elements
        for (const auto& item : j) {
            if (item.is_object() || item.is_array()) {
                extractContentFieldsRecursive(item, content_parts);
            }
        }
    }
}

void DocumentChunker::extractContentFields(const std::string& json_str, std::vector<std::string>& content_parts) {
    try {
        // Parse JSON using nlohmann/json library
        auto j = json::parse(json_str);
        
        // Recursively extract content fields
        extractContentFieldsRecursive(j, content_parts);
        
    } catch (const json::parse_error& e) {
        PE_ERROR("[DocumentChunker] JSON parse error: " + std::string(e.what()));
        // If parsing fails, treat as plain text (no content parts)
    } catch (const std::exception& e) {
        PE_ERROR("[DocumentChunker] Exception extracting content fields: " + std::string(e.what()));
    }
}

std::string DocumentChunker::extractContent(const std::string& text) {
    std::string trimmed = trim(text);
    
    // Check if input is JSON
    if (!isJSON(trimmed)) {
        // Plain text, return as-is
        return trimmed;
    }
    
    // Extract content fields from JSON
    std::vector<std::string> content_parts;
    extractContentFields(trimmed, content_parts);
    
    if (content_parts.empty()) {
        // No content fields found, return original
        PE_WARN("[DocumentChunker] No content fields found in JSON, using original text");
        return trimmed;
    }
    
    // Remove duplicates while preserving order
    std::vector<std::string> unique_content;
    std::set<std::string> seen;
    for (const auto& part : content_parts) {
        if (seen.find(part) == seen.end()) {
            seen.insert(part);
            unique_content.push_back(part);
        }
    }
    
    // Join with space
    std::ostringstream oss;
    for (size_t i = 0; i < unique_content.size(); ++i) {
        if (i > 0) oss << " ";
        oss << unique_content[i];
    }
    
    std::string result = oss.str();
    PE_INFO("[DocumentChunker] Extracted " + std::to_string(unique_content.size()) + 
            " content fields from JSON (" + std::to_string(result.length()) + " chars)");
    
    return result;
}

bool DocumentChunker::chunkDocument(
    const std::string& text,
    int chunk_size,
    int overlap,
    std::vector<ChunkData>& chunks_out) {
    
    if (!is_ready_) {
        last_error_ = "DocumentChunker not ready (tokenizer not loaded)";
        PE_ERROR("[DocumentChunker] " + last_error_);
        return false;
    }
    
    if (chunk_size <= 0 || chunk_size > max_seq_length_) {
        last_error_ = "Invalid chunk_size: " + std::to_string(chunk_size);
        PE_ERROR("[DocumentChunker] " + last_error_);
        return false;
    }
    
    if (overlap < 0 || overlap >= chunk_size) {
        last_error_ = "Invalid overlap: " + std::to_string(overlap);
        PE_ERROR("[DocumentChunker] " + last_error_);
        return false;
    }
    
    try {
        // Clear output
        chunks_out.clear();
        
        // Extract content (handles JSON automatically)
        std::string content = extractContent(text);
        
        if (content.empty()) {
            last_error_ = "Empty content after extraction";
            PE_WARN("[DocumentChunker] " + last_error_);
            return false;
        }
        
        PE_INFO("[DocumentChunker] Chunking document: " + std::to_string(content.length()) + 
                " chars, chunk_size=" + std::to_string(chunk_size) + 
                ", overlap=" + std::to_string(overlap));
        
        // Tokenize full document (no truncation)
        auto full_encoding = tokenizer_->encode(
            content,
            0,      // max_length = 0 means no truncation
            false,  // truncation = false
            "no_padding"  // no padding for full document
        );
        
        if (full_encoding.input_ids.empty()) {
            last_error_ = "Tokenization failed: empty result";
            PE_ERROR("[DocumentChunker] " + last_error_);
            return false;
        }
        
        int total_tokens = static_cast<int>(full_encoding.input_ids.size());
        int stride = chunk_size - overlap;
        
        PE_INFO("[DocumentChunker] Total tokens: " + std::to_string(total_tokens) + 
                ", stride: " + std::to_string(stride));
        
        // Sliding window chunking
        for (int start = 0; start < total_tokens; start += stride) {
            int end = std::min(start + chunk_size, total_tokens);
            int chunk_length = end - start;
            
            ChunkData chunk;
            chunk.actual_length = chunk_length;
            
            // Extract chunk tokens
            chunk.input_ids.assign(
                full_encoding.input_ids.begin() + start,
                full_encoding.input_ids.begin() + end
            );
            
            chunk.attention_mask.assign(
                full_encoding.attention_mask.begin() + start,
                full_encoding.attention_mask.begin() + end
            );
            
            // Pad to max_seq_length if needed
            int pad_length = max_seq_length_ - chunk_length;
            if (pad_length > 0) {
                chunk.input_ids.resize(max_seq_length_, 0);
                chunk.attention_mask.resize(max_seq_length_, 0);
            }
            
            // Token type IDs (all zeros for embeddinggemma)
            chunk.token_type_ids.assign(max_seq_length_, 0);
            
            chunks_out.push_back(std::move(chunk));
            
            // If we've covered the entire document, break
            if (end >= total_tokens) {
                break;
            }
        }
        
        PE_INFO("[DocumentChunker] Created " + std::to_string(chunks_out.size()) + " chunks");
        
        return true;
        
    } catch (const std::exception& e) {
        last_error_ = std::string("Exception during chunking: ") + e.what();
        PE_ERROR("[DocumentChunker] " + last_error_);
        return false;
    }
}

} // namespace embedding
