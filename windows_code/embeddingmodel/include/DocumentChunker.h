#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace embedding {

/**
 * @brief Tokenized chunk data structure
 */
struct ChunkData {
    std::vector<int64_t> input_ids;
    std::vector<int64_t> attention_mask;
    std::vector<int64_t> token_type_ids;
    int actual_length;  // Actual number of tokens (before padding)
};

/**
 * @brief Document chunker for embedding comparison
 * 
 * Chunks documents with sliding window approach and tokenizes using GemmaTokenizer.
 * This replaces the Python chunk_document.py script for better performance.
 */
class DocumentChunker {
public:
    /**
     * @brief Constructor
     * @param tokenizer_path Path to tokenizer directory
     * @param max_seq_length Maximum sequence length (default: 512)
     */
    explicit DocumentChunker(const std::string& tokenizer_path, int max_seq_length = 512);
    
    /**
     * @brief Destructor
     */
    ~DocumentChunker();
    
    /**
     * @brief Check if chunker is ready
     */
    bool isReady() const { return is_ready_; }
    
    /**
     * @brief Get last error message
     */
    std::string getLastError() const { return last_error_; }
    
    /**
     * @brief Chunk a document with sliding window
     * 
     * @param text Input text (can be plain text or JSON)
     * @param chunk_size Size of each chunk in tokens (default: 450)
     * @param overlap Overlap between chunks in tokens (default: 50)
     * @param chunks_out Output vector of tokenized chunks
     * @return true on success, false on error
     * 
     * This function:
     * 1. Tokenizes the full document
     * 2. Splits into overlapping chunks using sliding window
     * 3. Pads chunks to max_seq_length
     * 4. Returns ready-to-use tokenized data
     */
    bool chunkDocument(
        const std::string& text,
        int chunk_size,
        int overlap,
        std::vector<ChunkData>& chunks_out
    );
    
    /**
     * @brief Extract text content from input
     * 
     * If input is JSON (Elasticsearch format), extracts only content fields.
     * If input is plain text, returns as-is.
     * 
     * @param text Input text
     * @return Extracted content text
     */
    std::string extractContent(const std::string& text);
    
private:
    class GemmaTokenizer* tokenizer_;
    int max_seq_length_;
    bool is_ready_;
    std::string last_error_;
    
    // Helper: Extract content fields from JSON object (for Elasticsearch documents)
    void extractContentFields(const std::string& json, std::vector<std::string>& content_parts);
};

} // namespace embedding
