#include "GemmaTokenizer.h"
#include "pe_base/logger.h"

#ifndef NO_CPP_TOKENIZER
#include <sentencepiece_processor.h>
#endif

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace embedding {

class GemmaTokenizer::Impl {
public:
#ifndef NO_CPP_TOKENIZER
    sentencepiece::SentencePieceProcessor processor;
#endif
    bool loaded;
    std::string lastError;
    std::string modelPath;
    
    // Special tokens for Gemma
    static constexpr int64_t PAD_TOKEN_ID = 0;
    static constexpr int64_t BOS_TOKEN_ID = 2;  // Beginning of sequence
    static constexpr int64_t EOS_TOKEN_ID = 1;  // End of sequence
    
    Impl() : loaded(false) {}
};

GemmaTokenizer::GemmaTokenizer(const std::string& model_path)
    : impl_(std::make_unique<Impl>()) {
    
    impl_->modelPath = model_path;
    
#ifdef NO_CPP_TOKENIZER
    impl_->lastError = "C++ tokenizer disabled at compile time (SentencePiece not available)";
    PE_WARN("[GemmaTokenizer] " + impl_->lastError);
    PE_WARN("[GemmaTokenizer] Please run: powershell -File scripts/build_sentencepiece.ps1");
    return;
#else
    
    try {
        // Try to find tokenizer model file
        std::filesystem::path tokenizer_dir(model_path);
        std::filesystem::path model_file;
        
        // Look for common tokenizer file names
        std::vector<std::string> possible_names = {
            "tokenizer.model",
            "spiece.model",
            "sentencepiece.model"
        };
        
        bool found = false;
        for (const auto& name : possible_names) {
            std::filesystem::path candidate = tokenizer_dir / name;
            if (std::filesystem::exists(candidate)) {
                model_file = candidate;
                found = true;
                PE_INFO("[GemmaTokenizer] Found tokenizer model: " + candidate.string());
                break;
            }
        }
        
        if (!found) {
            impl_->lastError = "Tokenizer model file not found in: " + model_path;
            PE_ERROR("[GemmaTokenizer] " + impl_->lastError);
            PE_ERROR("[GemmaTokenizer] Looked for: tokenizer.model, spiece.model, sentencepiece.model");
            return;
        }
        
        // Load SentencePiece model
        auto start = std::chrono::high_resolution_clock::now();
        
        const auto status = impl_->processor.Load(model_file.string());
        
        if (!status.ok()) {
            impl_->lastError = "Failed to load SentencePiece model: " + status.ToString();
            PE_ERROR("[GemmaTokenizer] " + impl_->lastError);
            return;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        impl_->loaded = true;
        impl_->lastError.clear();
        
        PE_INFO("[GemmaTokenizer] ? Tokenizer loaded in " + std::to_string(ms) + "ms");
        PE_INFO("[GemmaTokenizer] Vocabulary size: " + std::to_string(impl_->processor.GetPieceSize()));
        
    } catch (const std::exception& e) {
        impl_->lastError = std::string("Exception loading tokenizer: ") + e.what();
        PE_ERROR("[GemmaTokenizer] " + impl_->lastError);
        impl_->loaded = false;
    }
#endif
}

GemmaTokenizer::~GemmaTokenizer() = default;

GemmaTokenizer::EncodingResult GemmaTokenizer::encode(
    const std::string& text,
    size_t max_length,
    bool truncation,
    const std::string& padding) {
    
    EncodingResult result;
    
#ifdef NO_CPP_TOKENIZER
    PE_ERROR("[GemmaTokenizer] Cannot encode - tokenizer not available (compile with SentencePiece)");
    return result;
#else
    
    if (!impl_->loaded) {
        PE_ERROR("[GemmaTokenizer] Tokenizer not loaded");
        return result;
    }
    
    try {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Encode text to token IDs
        std::vector<int> token_ids;
        const auto status = impl_->processor.Encode(text, &token_ids);
        
        if (!status.ok()) {
            PE_ERROR("[GemmaTokenizer] Encoding failed: " + status.ToString());
            return result;
        }
        
        // Add BOS token at beginning
        result.input_ids.push_back(Impl::BOS_TOKEN_ID);
        
        // Convert int to int64_t and add tokens
        for (int id : token_ids) {
            result.input_ids.push_back(static_cast<int64_t>(id));
        }
        
        // Add EOS token at end
        result.input_ids.push_back(Impl::EOS_TOKEN_ID);
        
        // Truncate if needed
        if (truncation && result.input_ids.size() > max_length) {
            result.input_ids.resize(max_length);
            // Ensure EOS token at end after truncation
            result.input_ids[max_length - 1] = Impl::EOS_TOKEN_ID;
        }
        
        // Store actual number of tokens (before padding)
        result.num_tokens = result.input_ids.size();
        
        // Padding to max_length
        if (padding == "max_length") {
            size_t current_length = result.input_ids.size();
            
            // Pad input_ids
            result.input_ids.resize(max_length, Impl::PAD_TOKEN_ID);
            
            // Create attention mask (1 for real tokens, 0 for padding)
            result.attention_mask.resize(max_length, 0);
            for (size_t i = 0; i < current_length; ++i) {
                result.attention_mask[i] = 1;
            }
            
            // Token type IDs (all zeros for single sequence)
            result.token_type_ids.resize(max_length, 0);
        } else {
            // No padding - attention mask all 1s
            result.attention_mask.resize(result.input_ids.size(), 1);
            result.token_type_ids.resize(result.input_ids.size(), 0);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        PE_INFO("[GemmaTokenizer] ? Tokenized in " + std::to_string(us / 1000.0) + "ms " +
               "(" + std::to_string(result.num_tokens) + " tokens)");
        
        return result;
        
    } catch (const std::exception& e) {
        PE_ERROR("[GemmaTokenizer] Exception during encoding: " + std::string(e.what()));
        return result;
    }
#endif
}

bool GemmaTokenizer::isLoaded() const {
    return impl_->loaded;
}

size_t GemmaTokenizer::vocabSize() const {
#ifdef NO_CPP_TOKENIZER
    return 0;
#else
    if (!impl_->loaded) {
        return 0;
    }
    return static_cast<size_t>(impl_->processor.GetPieceSize());
#endif
}

std::string GemmaTokenizer::getLastError() const {
    return impl_->lastError;
}

} // namespace embedding
