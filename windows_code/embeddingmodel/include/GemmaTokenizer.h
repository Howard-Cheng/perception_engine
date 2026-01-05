#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace embedding {

/**
 * @brief C++ Tokenizer for Gemma models using SentencePiece
 * 
 * This tokenizer provides high-performance tokenization for Gemma models
 * using the SentencePiece library. It eliminates the 7-second delay of
 * Python tokenization.
 */
class GemmaTokenizer {
public:
    /**
     * @brief Result structure for tokenization
     */
    struct EncodingResult {
        std::vector<int64_t> input_ids;        // Token IDs
        std::vector<int64_t> attention_mask;   // Attention mask (1 for real tokens, 0 for padding)
        std::vector<int64_t> token_type_ids;   // Token type IDs (all 0 for single sequence)
        size_t num_tokens;                     // Actual number of tokens (before padding)
        
        EncodingResult() : num_tokens(0) {}
    };

    /**
     * @brief Construct tokenizer with model path
     * @param model_path Path to directory containing tokenizer.model
     */
    explicit GemmaTokenizer(const std::string& model_path);
    
    /**
     * @brief Destructor
     */
    ~GemmaTokenizer();
    
    // Disable copy (tokenizer holds unique resources)
    GemmaTokenizer(const GemmaTokenizer&) = delete;
    GemmaTokenizer& operator=(const GemmaTokenizer&) = delete;
    
    // Enable move
    GemmaTokenizer(GemmaTokenizer&&) noexcept = default;
    GemmaTokenizer& operator=(GemmaTokenizer&&) noexcept = default;

    /**
     * @brief Encode text to token IDs
     * 
     * @param text Input text to tokenize
     * @param max_length Maximum sequence length (default: 512)
     * @param truncation Whether to truncate if exceeds max_length (default: true)
     * @param padding Padding strategy: "max_length" or "none" (default: "max_length")
     * @return EncodingResult containing token IDs and masks
     */
    EncodingResult encode(
        const std::string& text,
        size_t max_length = 512,
        bool truncation = true,
        const std::string& padding = "max_length"
    );

    /**
     * @brief Check if tokenizer is successfully loaded
     * @return true if loaded, false otherwise
     */
    bool isLoaded() const;

    /**
     * @brief Get vocabulary size
     * @return Size of vocabulary
     */
    size_t vocabSize() const;

    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;

private:
    class Impl;  // Forward declaration for pimpl
    std::unique_ptr<Impl> impl_;
};

} // namespace embedding
