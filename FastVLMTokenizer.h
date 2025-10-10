#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>

/**
 * @brief FastVLM Tokenizer - Handles token decoding
 *
 * Loads vocab.json at runtime and provides token->text decoding
 * for FastVLM model outputs.
 */
class FastVLMTokenizer {
public:
    FastVLMTokenizer() = default;

    /**
     * @brief Load vocabulary from vocab.json
     * @param vocabPath Path to vocab.json file
     * @return true if loaded successfully
     */
    bool LoadVocab(const std::string& vocabPath);

    /**
     * @brief Decode token IDs to text
     * @param tokens Vector of token IDs
     * @return Decoded text string
     */
    std::string Decode(const std::vector<int64_t>& tokens) const;

    /**
     * @brief Get pre-tokenized prompt tokens
     * Prompt: "<image>Briefly, what is this?"
     */
    static std::vector<int64_t> GetPromptTokens() {
        return {151646, 85984, 398, 11, 1128, 374, 419, 30};
    }

    // Special token IDs
    static constexpr int64_t IMAGE_TOKEN_ID = 151646;
    static constexpr int64_t EOS_TOKEN_ID = 151645;

private:
    std::unordered_map<int64_t, std::string> vocab_;

    /**
     * @brief Simple JSON string parser (extracts quoted strings)
     */
    std::string ParseJsonString(const std::string& json, size_t& pos);

    /**
     * @brief Simple JSON number parser
     */
    int64_t ParseJsonNumber(const std::string& json, size_t& pos);
};
