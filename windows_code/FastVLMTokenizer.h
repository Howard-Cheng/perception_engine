#pragma once

#include <string>
#include <vector>
#include <cstdint>

/**
 * FastVLMTokenizer - Stub implementation for tokenizer
 *
 * NOTE: This is a minimal stub implementation to satisfy linker requirements.
 * The actual vision-language model tokenization is handled by the Python camera client
 * (win_camera_fastvlm_pytorch.py) which uses Hugging Face transformers.
 *
 * The C++ CameraVisionEngine was an experimental ONNX-based implementation
 * that is not currently used in production.
 */
class FastVLMTokenizer {
public:
    /**
     * Load vocabulary file (stub - returns true without loading)
     */
    bool LoadVocab(const std::string& vocabPath);

    /**
     * Decode token IDs to text (stub - returns placeholder)
     */
    std::string Decode(const std::vector<int64_t>& tokens) const;

    /**
     * Get prompt tokens (stub - returns empty vector)
     */
    static std::vector<int64_t> GetPromptTokens();

    /**
     * End-of-sequence token ID
     */
    static const int64_t EOS_TOKEN_ID;
};
