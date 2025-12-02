#include "FastVLMTokenizer.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

// Stub implementation for FastVLMTokenizer
// This is a minimal implementation to satisfy linker requirements
// The actual tokenization is handled by the Python camera client

bool FastVLMTokenizer::LoadVocab(const std::string& vocabPath) {
    PE_WARN("FastVLMTokenizer::LoadVocab called - using stub implementation");
    LOG_INFO_FMT("Vocab path: %s (not actually loaded - Python handles tokenization)", vocabPath.c_str());
    return true;  // Return true to avoid errors
}

std::string FastVLMTokenizer::Decode(const std::vector<int64_t>& tokens) const {
    PE_WARN("FastVLMTokenizer::Decode called - using stub implementation");

    // Return a placeholder message
    std::ostringstream oss;
    oss << "[" << tokens.size() << " tokens - decoded by Python client]";
    return oss.str();
}

std::vector<int64_t> FastVLMTokenizer::GetPromptTokens() {
    PE_WARN("FastVLMTokenizer::GetPromptTokens called - using stub implementation");

    // Return empty vector - Python client handles prompts
    return std::vector<int64_t>();
}

const int64_t FastVLMTokenizer::EOS_TOKEN_ID = 2;  // Standard EOS token ID
