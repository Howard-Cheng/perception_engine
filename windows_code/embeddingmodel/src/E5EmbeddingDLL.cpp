#include "E5EmbeddingDLL.h"
#include "GemmaTokenizer.h"  // ADD: Include C++ tokenizer
#include "pe_base/config_manager.h"
#include "pe_base/logger.h"

#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <windows.h>

// Fix for Windows min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef min
#undef max

// Global state (initialized once)
namespace {
    std::unique_ptr<Ort::Env> g_env;
    std::unique_ptr<Ort::Session> g_session;
    std::unique_ptr<embedding::GemmaTokenizer> g_tokenizer;  // C++ tokenizer
    std::mutex g_mutex;  // Protects initialization
    bool g_initialized = false;
    std::string g_last_error;
    std::wstring g_tokenizer_path;  // Store tokenizer path

    const int EMBEDDING_DIM = 256;  // google/embeddinggemma-300m uses 256-dim embeddings
    const int MAX_SEQ_LENGTH = 512;
    
    // ============================================================================
    // Comparison Results Storage
    // ============================================================================
    struct ChunkComparisonResult {
        int chunk_index_A;
        int chunk_index_B;
        float similarity_score;
        std::string text_A;
        std::string text_B;
    };
    
    std::vector<ChunkComparisonResult> g_last_comparison_results;
    std::string g_last_doc_A_text;
    std::string g_last_doc_B_text;
    std::mutex g_comparison_mutex;  // Protects comparison results
}

// Helper: Set error message
void SetError(const std::string& error) {
    g_last_error = error;
}

// Helper: Average pooling
void AveragePool(
    const float* hidden_states,
    const int64_t* attention_mask,
    int seq_length,
    float* output) {

    std::fill(output, output + EMBEDDING_DIM, 0.0f);
    int token_count = 0;

    // ? DEBUG: Log pooling details
    PE_INFO("[AveragePool] seq_length: " + std::to_string(seq_length) + ", EMBEDDING_DIM: " + std::to_string(EMBEDDING_DIM));

    for (int i = 0; i < seq_length; i++) {
        if (attention_mask[i] == 1) {
            for (int j = 0; j < EMBEDDING_DIM; j++) {
                output[j] += hidden_states[i * EMBEDDING_DIM + j];
            }
            token_count++;
        }
    }

    PE_INFO("[AveragePool] token_count: " + std::to_string(token_count));

    if (token_count > 0) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            output[j] /= token_count;
        }
        
        // ? DEBUG: Check if sum before division is all zeros
        float sum_check = 0.0f;
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            sum_check += std::abs(output[j]);
        }
        
        if (sum_check < 1e-10f) {
            PE_ERROR("[AveragePool] ERROR: Pooled values are all zeros BEFORE normalization!");
            PE_ERROR("[AveragePool] This means hidden_states from ONNX are all zeros!");
        } else {
            // ? DEBUG: Log first few pooled values after division
            std::ostringstream oss;
            oss << "[AveragePool] After averaging, first 5 values: ";
            for (int i = 0; i < std::min(5, EMBEDDING_DIM); ++i) {
                oss << output[i] << " ";
            }
            PE_INFO(oss.str());
        }
    } else {
        PE_ERROR("[AveragePool] FATAL ERROR: token_count is 0! All attention_mask values are 0!");
        PE_ERROR("[AveragePool] This will result in all-zero embeddings!");
    }
}

// Helper: L2 normalization
void Normalize(float* embedding) {
    float norm = 0.0f;
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        norm += embedding[i] * embedding[i];
    }
    norm = std::sqrt(norm);

    if (norm > 1e-12f) {
        for (int i = 0; i < EMBEDDING_DIM; i++) {
            embedding[i] /= norm;
        }
    }
}

// Helper: Dot product
float DotProduct(const float* a, const float* b) {
    float result = 0.0f;
    for (int i = 0; i < EMBEDDING_DIM; i++) {
        result += a[i] * b[i];
    }
    return result;
}

// API Implementation

std::string GetExePath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exePathStr(exePath);
    size_t lastSlash = exePathStr.find_last_of("\\/");
    std::string exe_dir = exePathStr.substr(0, lastSlash);
    return exe_dir;
}

E5_API int E5_Initialize(const wchar_t* model_path) {
    std::lock_guard<std::mutex> lock(g_mutex);    
    std::filesystem::path log_path = "";
    if (auto* p_appdata = getenv("APPDATA")) {
        log_path =
            std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
    }
    pe_base::LogWriter::SetLogFilePrefix(
        (log_path / "e5_embedding").generic_string());

    if (g_initialized) {
        SetError("Already initialized");
        return 0;
    }

    // =========================================
    // Load Configuration
    // =========================================
    PE_INFO("Loading configuration from config.ini...");
    std::string config_path = GetExePath() + "\\config.ini";
    if (!pe_base::ConfigManager::GetInstance().LoadConfig(config_path)) {
        PE_WARN("Failed to load config.ini, using default values");
        PE_WARN(std::string("Error: ") + pe_base::ConfigManager::GetInstance().GetLastError());
    }
    else {
        PE_INFO("Configuration loaded successfully");
    }

    // Validate configuration
    if (!pe_base::ConfigManager::GetInstance().ValidateConfiguration()) {
        PE_ERROR("Configuration validation failed:");
        PE_ERROR(pe_base::ConfigManager::GetInstance().GetLastError());
        PE_WARN("Continuing with best-effort configuration...");
    }
    else {
        PE_INFO("Configuration validated successfully");
    }

    try {
        // Initialize ONNX Runtime
        g_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "E5Embedding");

        // Create session options
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(4);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Load model
        g_session = std::make_unique<Ort::Session>(*g_env, model_path, session_options);

        // Extract tokenizer path from model path
        // Assume tokenizer is in same directory as model with name "tokenizer"
        std::wstring model_path_str(model_path);
        size_t last_slash = model_path_str.find_last_of(L"\\/");
        if (last_slash != std::wstring::npos) {
            g_tokenizer_path = model_path_str.substr(0, last_slash + 1) + L"tokenizer";
        } else {
            g_tokenizer_path = L"tokenizer";
        }
        
        // Initialize C++ tokenizer
        PE_INFO("[E5_Initialize] Initializing C++ tokenizer...");
        std::string tokenizer_path_utf8 = pe_base::ConfigManager::GetInstance().GetTokenizerPath();
        
        try {
            g_tokenizer = std::make_unique<embedding::GemmaTokenizer>(tokenizer_path_utf8);
            
            if (g_tokenizer->isLoaded()) {
                PE_INFO("[E5_Initialize] ? C++ tokenizer loaded successfully");
                PE_INFO("[E5_Initialize] Vocabulary size: " + std::to_string(g_tokenizer->vocabSize()));
            } else {
                PE_ERROR("[E5_Initialize] Failed to load C++ tokenizer: " + g_tokenizer->getLastError());
                PE_WARN("[E5_Initialize] Text-based embedding functions will not work");
                g_tokenizer.reset();  // Release the failed tokenizer
            }
        } catch (const std::exception& e) {
            PE_ERROR("[E5_Initialize] Exception loading tokenizer: " + std::string(e.what()));
            g_tokenizer.reset();
        }

        g_initialized = true;
        g_last_error.clear();
        return 0;

    } catch (const Ort::Exception& e) {
        SetError(std::string("ONNX Runtime error: ") + e.what());
        return -1;
    } catch (const std::exception& e) {
        SetError(std::string("Initialization error: ") + e.what());
        return -1;
    }
}

E5_API int E5_ComputeEmbedding(
    const int64_t* input_ids,
    const int64_t* attention_mask,
    const int64_t* token_type_ids,
    int length,
    float* embedding_out) {

    if (!g_initialized) {
        SetError("Not initialized. Call E5_Initialize first.");
        return -1;
    }

    if (length > MAX_SEQ_LENGTH) {
        SetError("Sequence length exceeds maximum (512)");
        return -1;
    }

    // ? FIX: Clear old error messages before starting
    g_last_error.clear();
    
    // ? DEBUG: Log computation start
    PE_INFO("[E5_ComputeEmbedding] Starting computation with length: " + std::to_string(length));

    try {
        // ? DEBUG: Count valid tokens in attention mask BEFORE doing anything else
        int valid_tokens = 0;
        for (int i = 0; i < length; ++i) {
            if (attention_mask[i] == 1) {
                valid_tokens++;
            }
        }
        PE_INFO("[E5_ComputeEmbedding] Valid tokens in attention_mask: " + std::to_string(valid_tokens) + " / " + std::to_string(length));
        
        if (valid_tokens == 0) {
            PE_ERROR("[E5_ComputeEmbedding] FATAL ERROR: attention_mask has NO valid tokens (all zeros)!");
            PE_ERROR("[E5_ComputeEmbedding] This will cause AveragePool to return all zeros!");
            PE_ERROR("[E5_ComputeEmbedding] Check tokenization process!");
            SetError("attention_mask has no valid tokens (all zeros)");
            return -1;
        }
        
        // Prepare input tensors (batch size = 1)
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> input_shape = {1, length};

        // We need to copy data because ONNX Runtime expects non-const pointers
        std::vector<int64_t> input_ids_copy(input_ids, input_ids + length);
        std::vector<int64_t> attention_mask_copy(attention_mask, attention_mask + length);
        std::vector<int64_t> token_type_ids_copy(token_type_ids, token_type_ids + length);
        
        // ? DEBUG: Log first few token IDs
        std::ostringstream oss;
        oss << "[E5_ComputeEmbedding] First 10 token IDs: ";
        for (int i = 0; i < std::min(10, length); ++i) {
            oss << input_ids[i] << " ";
        }
        PE_INFO(oss.str());
        
        // ? DEBUG: Log attention mask
        std::ostringstream oss_mask;
        oss_mask << "[E5_ComputeEmbedding] First 10 attention_mask values: ";
        for (int i = 0; i < std::min(10, length); ++i) {
            oss_mask << attention_mask[i] << " ";
        }
        PE_INFO(oss_mask.str());

        Ort::Value input_ids_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, input_ids_copy.data(), length, input_shape.data(), input_shape.size());

        Ort::Value attention_mask_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, attention_mask_copy.data(), length, input_shape.data(), input_shape.size());

        // NOTE: embeddinggemma only uses input_ids and attention_mask (no token_type_ids)
        // Run inference
        const char* input_names[] = {"input_ids", "attention_mask"};
        const char* output_names[] = {"last_hidden_state"};

        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(std::move(input_ids_tensor));
        input_tensors.push_back(std::move(attention_mask_tensor));
        
        PE_INFO("[E5_ComputeEmbedding] Running ONNX inference...");

        auto output_tensors = g_session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            input_tensors.data(),
            2,  // Only 2 inputs now
            output_names,
            1
        );
        
        PE_INFO("[E5_ComputeEmbedding] ONNX inference complete");

        // Extract output
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        
        // ? DEBUG: Check if output_data is valid
        if (!output_data) {
            PE_ERROR("[E5_ComputeEmbedding] ERROR: output_data is nullptr!");
            SetError("ONNX inference returned null data");
            return -1;
        }
        
        // ? DEBUG: Check if ONNX output is all zeros (model problem!)
        bool onnx_output_all_zero = true;
        for (int i = 0; i < std::min(length * EMBEDDING_DIM, 1000); ++i) {
            if (output_data[i] != 0.0f) {
                onnx_output_all_zero = false;
                break;
            }
        }
        
        if (onnx_output_all_zero) {
            PE_ERROR("[E5_ComputeEmbedding] ERROR: ONNX model returned all zeros!");
            PE_ERROR("[E5_ComputeEmbedding] This indicates a model loading or inference problem!");
            PE_ERROR("[E5_ComputeEmbedding] Check model file and ONNX Runtime version!");
        }
        
        // ? DEBUG: Print first few output values
        std::ostringstream oss2;
        oss2 << "[E5_ComputeEmbedding] First 5 hidden state values: ";
        for (int i = 0; i < std::min(5, EMBEDDING_DIM); ++i) {
            oss2 << output_data[i] << " ";
        }
        PE_INFO(oss2.str());

        // Average pool
        PE_INFO("[E5_ComputeEmbedding] Performing average pooling...");
        AveragePool(output_data, attention_mask, length, embedding_out);
        
        // ? DEBUG: Print embedding after pooling
        std::ostringstream oss3;
        oss3 << "[E5_ComputeEmbedding] After pooling, first 5 values: ";
        for (int i = 0; i < std::min(5, EMBEDDING_DIM); ++i) {
            oss3 << embedding_out[i] << " ";
        }
        PE_INFO(oss3.str());

        // Normalize
        PE_INFO("[E5_ComputeEmbedding] Normalizing...");
        Normalize(embedding_out);
        
        // ? DEBUG: Print embedding after normalization
        std::ostringstream oss4;
        oss4 << "[E5_ComputeEmbedding] After normalization, first 5 values: ";
        for (int i = 0; i < std::min(5, EMBEDDING_DIM); ++i) {
            oss4 << embedding_out[i] << " ";
        }
        PE_INFO(oss4.str());
        
        // ? DEBUG: Check if result is all zeros
        bool allZero = true;
        for (int i = 0; i < EMBEDDING_DIM; ++i) {
            if (embedding_out[i] != 0.0f) {
                allZero = false;
                break;
            }
        }
        
        if (allZero) {
            PE_ERROR("[E5_ComputeEmbedding] FATAL ERROR: Output embedding is all zeros!");
            if (valid_tokens == 0) {
                PE_ERROR("[E5_ComputeEmbedding] Root cause: attention_mask has no valid tokens");
            } else if (onnx_output_all_zero) {
                PE_ERROR("[E5_ComputeEmbedding] Root cause: ONNX model returned all zeros");
            } else {
                PE_ERROR("[E5_ComputeEmbedding] Root cause: Unknown (check AveragePool implementation)");
            }
            SetError("Embedding computation resulted in all zeros");
            return -1;
        } else {
            PE_INFO("[E5_ComputeEmbedding] SUCCESS: Embedding computation successful (non-zero)");
        }

        return 0;

    } catch (const Ort::Exception& e) {
        std::string error_msg = std::string("ONNX Runtime inference error: ") + e.what();
        PE_ERROR("[E5_ComputeEmbedding] " + error_msg);
        SetError(error_msg);
        return -1;
    } catch (const std::exception& e) {
        std::string error_msg = std::string("Embedding computation error: ") + e.what();
        PE_ERROR("[E5_ComputeEmbedding] " + error_msg);
        SetError(error_msg);
        return -1;
    }
}

E5_API int E5_ComputeEmbeddingBatch(
    const int64_t* input_ids,
    const int64_t* attention_mask,
    const int64_t* token_type_ids,
    int batch_size,
    int max_length,
    float* embeddings_out) {

    if (!g_initialized) {
        SetError("Not initialized. Call E5_Initialize first.");
        return -1;
    }

    if (max_length > MAX_SEQ_LENGTH) {
        SetError("Sequence length exceeds maximum (512)");
        return -1;
    }

    try {
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> input_shape = {batch_size, max_length};
        size_t total_elements = batch_size * max_length;

        // Copy data
        std::vector<int64_t> input_ids_copy(input_ids, input_ids + total_elements);
        std::vector<int64_t> attention_mask_copy(attention_mask, attention_mask + total_elements);
        std::vector<int64_t> token_type_ids_copy(token_type_ids, token_type_ids + total_elements);

        Ort::Value input_ids_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, input_ids_copy.data(), total_elements, input_shape.data(), input_shape.size());

        Ort::Value attention_mask_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, attention_mask_copy.data(), total_elements, input_shape.data(), input_shape.size());

        Ort::Value token_type_ids_tensor = Ort::Value::CreateTensor<int64_t>(
            memory_info, token_type_ids_copy.data(), total_elements, input_shape.data(), input_shape.size());

        // NOTE: embeddinggemma only uses input_ids and attention_mask (no token_type_ids)
        // Run inference
        const char* input_names[] = {"input_ids", "attention_mask"};
        const char* output_names[] = {"last_hidden_state"};

        std::vector<Ort::Value> input_tensors;
        input_tensors.push_back(std::move(input_ids_tensor));
        input_tensors.push_back(std::move(attention_mask_tensor));

        auto output_tensors = g_session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            input_tensors.data(),
            2,  // Only 2 inputs now
            output_names,
            1
        );

        // Extract output
        float* output_data = output_tensors[0].GetTensorMutableData<float>();

        // Process each sample
        for (int i = 0; i < batch_size; i++) {
            const float* sample_output = output_data + i * max_length * EMBEDDING_DIM;
            const int64_t* sample_mask = attention_mask + i * max_length;
            float* sample_embedding = embeddings_out + i * EMBEDDING_DIM;

            AveragePool(sample_output, sample_mask, max_length, sample_embedding);
            Normalize(sample_embedding);
        }

        return 0;

    } catch (const Ort::Exception& e) {
        SetError(std::string("ONNX Runtime batch inference error: ") + e.what());
        return -1;
    } catch (const std::exception& e) {
        SetError(std::string("Batch embedding computation error: ") + e.what());
        return -1;
    }
}

E5_API int E5_ComputeSimilarity(
    const int64_t* input_ids1,
    const int64_t* attention_mask1,
    const int64_t* token_type_ids1,
    int length1,
    const int64_t* input_ids2,
    const int64_t* attention_mask2,
    const int64_t* token_type_ids2,
    int length2,
    float* similarity_out) {

    std::vector<float> embedding1(EMBEDDING_DIM);
    std::vector<float> embedding2(EMBEDDING_DIM);

    // Compute both embeddings
    int result = E5_ComputeEmbedding(input_ids1, attention_mask1, token_type_ids1, length1, embedding1.data());
    if (result != 0) return result;

    result = E5_ComputeEmbedding(input_ids2, attention_mask2, token_type_ids2, length2, embedding2.data());
    if (result != 0) return result;

    // Compute similarity (dot product * 100, since embeddings are normalized)
    *similarity_out = DotProduct(embedding1.data(), embedding2.data()) * 100.0f;

    return 0;
}

E5_API int E5_GetEmbeddingDimension() {
    return EMBEDDING_DIM;
}

E5_API int E5_GetMaxSequenceLength() {
    return MAX_SEQ_LENGTH;
}

E5_API int E5_IsInitialized() {
    return g_initialized ? 1 : 0;
}

E5_API const char* E5_GetLastError() {
    return g_last_error.c_str();
}

// ============================================================================
// Helper Functions for Text Processing
// ============================================================================

// Helper: Save text to temp file
bool SaveTextToFile(const std::string& filename, const std::string& text) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.write(text.c_str(), text.size());
    file.close();
    return true;
}

// Helper: Read binary chunks file (used by E5_CompareDocuments)
bool ReadChunksFromFile(const std::string& filename,
                        std::vector<std::vector<int64_t>>& input_ids,
                        std::vector<std::vector<int64_t>>& attention_mask,
                        std::vector<std::vector<int64_t>>& token_type_ids) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Read header
    int64_t num_chunks, seq_length;
    file.read(reinterpret_cast<char*>(&num_chunks), sizeof(int64_t));
    file.read(reinterpret_cast<char*>(&seq_length), sizeof(int64_t));

    input_ids.resize(num_chunks);
    attention_mask.resize(num_chunks);
    token_type_ids.resize(num_chunks);

    // Read all input_ids
    for (int i = 0; i < num_chunks; i++) {
        input_ids[i].resize(seq_length);
        file.read(reinterpret_cast<char*>(input_ids[i].data()), seq_length * sizeof(int64_t));
    }

    // Read all attention_mask
    for (int i = 0; i < num_chunks; i++) {
        attention_mask[i].resize(seq_length);
        file.read(reinterpret_cast<char*>(attention_mask[i].data()), seq_length * sizeof(int64_t));
    }

    // Read all token_type_ids
    for (int i = 0; i < num_chunks; i++) {
        token_type_ids[i].resize(seq_length);
        file.read(reinterpret_cast<char*>(token_type_ids[i].data()), seq_length * sizeof(int64_t));
    }

    file.close();
    return true;
}

// Helper: Compute embeddings for all chunks (calls E5_ComputeEmbedding)
bool ComputeChunkEmbeddings(
    const std::vector<std::vector<int64_t>>& input_ids,
    const std::vector<std::vector<int64_t>>& attention_mask,
    const std::vector<std::vector<int64_t>>& token_type_ids,
    std::vector<std::vector<float>>& embeddings_out) {

    int num_chunks = static_cast<int>(input_ids.size());
    embeddings_out.resize(num_chunks);

    for (int i = 0; i < num_chunks; i++) {
        embeddings_out[i].resize(EMBEDDING_DIM);

        int result = E5_ComputeEmbedding(
            input_ids[i].data(),
            attention_mask[i].data(),
            token_type_ids[i].data(),
            MAX_SEQ_LENGTH,
            embeddings_out[i].data()
        );

        if (result != 0) {
            return false;
        }
    }

    return true;
}

// Helper: Compute average of embeddings (calls Normalize)
void AverageEmbeddings(const std::vector<std::vector<float>>& embeddings,
                       std::vector<float>& avg_out) {
    avg_out.resize(EMBEDDING_DIM, 0.0f);

    for (const auto& emb : embeddings) {
        for (int i = 0; i < EMBEDDING_DIM; i++) {
            avg_out[i] += emb[i];
        }
    }

    for (int i = 0; i < EMBEDDING_DIM; i++) {
        avg_out[i] /= static_cast<float>(embeddings.size());
    }

    // Normalize
    Normalize(avg_out.data());
}

// Helper: Extract chunk text from document
std::string ExtractChunkText(const std::string& doc_text, int chunk_index, int chunk_size, int overlap) {
    int chars_per_chunk = chunk_size * 4;
    int chars_overlap = overlap * 4;
    int stride = chars_per_chunk - chars_overlap;
    
    int start_pos = chunk_index * stride;
    int end_pos = start_pos + chars_per_chunk;
    
    if (start_pos >= static_cast<int>(doc_text.size())) {
        return "";
    }
    
    if (end_pos > static_cast<int>(doc_text.size())) {
        end_pos = static_cast<int>(doc_text.size());
    }
    
    std::string chunk = doc_text.substr(start_pos, end_pos - start_pos);
    
    if (chunk.size() > 500) {
        chunk = chunk.substr(0, 497) + "...";
    }
    
    return chunk;
}

// ============================================================================
// Text-based Embedding Functions (with internal tokenization)
// ============================================================================

E5_API int E5_ComputeEmbeddingFromText(
    const char* text,
    float* embedding_out) {

    if (!g_initialized) {
        SetError("Not initialized. Call E5_Initialize first");
        return -1;
    }

    if (text == nullptr || embedding_out == nullptr) {
        SetError("NULL pointer provided");
        return -1;
    }
    
    if (!g_tokenizer || !g_tokenizer->isLoaded()) {
        SetError("C++ tokenizer not available. Cannot process text.");
        PE_ERROR("[E5_ComputeEmbeddingFromText] Tokenizer not loaded");
        return -1;
    }

    // Clear old error messages
    g_last_error.clear();

    try {
        PE_INFO("[E5_ComputeEmbeddingFromText] Tokenizing text with C++ tokenizer...");
        
        // Use C++ tokenizer
        auto encoding = g_tokenizer->encode(
            text,
            MAX_SEQ_LENGTH,  // max_length
            true,            // truncation
            "max_length"     // padding
        );
        
        if (encoding.input_ids.empty()) {
            SetError("Tokenization failed - empty result");
            PE_ERROR("[E5_ComputeEmbeddingFromText] Tokenizer returned empty result");
            return -1;
        }
        
        // Verify data integrity
        int valid_mask_count = 0;
        for (size_t i = 0; i < encoding.attention_mask.size(); ++i) {
            if (encoding.attention_mask[i] == 1) {
                valid_mask_count++;
            }
        }
        
        PE_INFO("[E5_ComputeEmbeddingFromText] Tokenized: " + std::to_string(encoding.num_tokens) + 
               " tokens, valid attention_mask count: " + std::to_string(valid_mask_count));
        
        if (valid_mask_count == 0) {
            SetError("Tokenization produced no valid tokens");
            PE_ERROR("[E5_ComputeEmbeddingFromText] FATAL: attention_mask all zeros");
            return -1;
        }
        
        // Debug: Log first few tokens
        std::ostringstream oss_ids;
        oss_ids << "[E5_ComputeEmbeddingFromText] First 10 input_ids: ";
        for (int i = 0; i < std::min(10, (int)encoding.input_ids.size()); ++i) {
            oss_ids << encoding.input_ids[i] << " ";
        }
        PE_INFO(oss_ids.str());
        
        std::ostringstream oss_mask;
        oss_mask << "[E5_ComputeEmbeddingFromText] First 10 attention_mask: ";
        for (int i = 0; i < std::min(10, (int)encoding.attention_mask.size()); ++i) {
            oss_mask << encoding.attention_mask[i] << " ";
        }
        PE_INFO(oss_mask.str());

        // Compute embedding using tokenized data
        int result = E5_ComputeEmbedding(
            encoding.input_ids.data(),
            encoding.attention_mask.data(),
            encoding.token_type_ids.data(),
            MAX_SEQ_LENGTH,
            embedding_out
        );

        return result;

    } catch (const std::exception& e) {
        SetError(std::string("Text embedding error: ") + e.what());
        PE_ERROR("[E5_ComputeEmbeddingFromText] Exception: " + std::string(e.what()));
        return -1;
    }
}

E5_API int E5_ComputeEmbeddingFromTextBatch(
    const char** texts,
    int num_texts,
    float* embeddings_out) {

    if (!g_initialized) {
        SetError("Not initialized. Call E5_Initialize first");
        return -1;
    }

    if (texts == nullptr || embeddings_out == nullptr) {
        SetError("NULL pointer provided");
        return -1;
    }

    if (num_texts <= 0) {
        SetError("num_texts must be positive");
        return -1;
    }

    try {
        // Process each text individually
        // TODO: Optimize with true batch processing
        for (int i = 0; i < num_texts; ++i) {
            int result = E5_ComputeEmbeddingFromText(
                texts[i],
                embeddings_out + i * EMBEDDING_DIM
            );

            if (result != 0) {
                return result;
            }
        }

        return 0;

    } catch (const std::exception& e) {
        SetError(std::string("Batch text embedding error: ") + e.what());
        return -1;
    }
}

E5_API void E5_Cleanup() {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_session.reset();
    g_env.reset();
    g_tokenizer.reset();  // Clean up C++ tokenizer
    g_initialized = false;
    g_last_error.clear();
}

E5_API int E5_CompareDocuments(
    const char* doc_A_text,
    const char* doc_B_text,
    int chunk_size,
    int overlap,
    float* similarity_out) {

    if (!g_initialized) {
        SetError("Not initialized. Call E5_Initialize first");
        return -1;
    }

    if (doc_A_text == nullptr || doc_B_text == nullptr || similarity_out == nullptr) {
        SetError("NULL pointer provided");
        return -1;
    }

    try {
        // Clear previous comparison results
        {
            std::lock_guard<std::mutex> lock(g_comparison_mutex);
            g_last_comparison_results.clear();
            g_last_doc_A_text = doc_A_text;
            g_last_doc_B_text = doc_B_text;
        }

        // Get paths from pe_base::ConfigManager
        std::string python_executable = pe_base::ConfigManager::GetInstance().GetPythonExecutable();
        std::string chunk_script = pe_base::ConfigManager::GetInstance().GetChunkDocumentScript();

        // Normalize paths: replace forward slashes with backslashes for Windows
        std::replace(chunk_script.begin(), chunk_script.end(), '/', '\\');

        // Get executable directory (not current working directory!)
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string exePathStr(exePath);
        size_t lastSlash = exePathStr.find_last_of("\\/");
        std::string exe_dir = exePathStr.substr(0, lastSlash);

        // Create temp files in the same directory as executable
        std::string temp_doc_A = exe_dir + "\\temp_doc_A.txt";
        std::string temp_doc_B = exe_dir + "\\temp_doc_B.txt";
        std::string chunks_A_file = exe_dir + "\\temp_chunks_A.bin";
        std::string chunks_B_file = exe_dir + "\\temp_chunks_B.bin";

        PE_INFO("Executable directory: " + exe_dir);
        PE_INFO("Python executable: " + python_executable);
        PE_INFO("Chunk script: " + chunk_script);

        // Save documents to temp files
        if (!SaveTextToFile(temp_doc_A, doc_A_text)) {
            SetError("Failed to save document A to temp file: " + temp_doc_A);
            PE_ERROR(g_last_error);
            return -1;
        }

        if (!SaveTextToFile(temp_doc_B, doc_B_text)) {
            SetError("Failed to save document B to temp file: " + temp_doc_B);
            PE_ERROR(g_last_error);
            return -1;
        }

        // Call Python to chunk documents
        // Convert tokenizer path to UTF-8 and normalize
        std::wstring tokenizer_path_w = g_tokenizer_path;
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, tokenizer_path_w.c_str(), (int)tokenizer_path_w.length(), NULL, 0, NULL, NULL);
        std::string tokenizer_path(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, tokenizer_path_w.c_str(), (int)tokenizer_path_w.length(), &tokenizer_path[0], size_needed, NULL, NULL);

        // Normalize tokenizer path too
        std::replace(tokenizer_path.begin(), tokenizer_path.end(), '/', '\\');

        // Build commands using absolute paths - no quotes around python_executable
        char cmd_A[4096];
        sprintf_s(cmd_A, sizeof(cmd_A),
            "%s \"%s\" \"%s\" \"%s\" %d %d \"%s\"",
            python_executable.c_str(),
            chunk_script.c_str(),
            temp_doc_A.c_str(),
            chunks_A_file.c_str(),
            chunk_size,
            overlap,
            tokenizer_path.c_str());

        char cmd_B[4096];
        sprintf_s(cmd_B, sizeof(cmd_B),
            "%s \"%s\" \"%s\" \"%s\" %d %d \"%s\"",
            python_executable.c_str(),
            chunk_script.c_str(),
            temp_doc_B.c_str(),
            chunks_B_file.c_str(),
            chunk_size,
            overlap,
            tokenizer_path.c_str());

        PE_INFO("Executing command A: " + std::string(cmd_A));

        if (system(cmd_A) != 0) {
            SetError("Failed to chunk document A (Python error). Command: " + std::string(cmd_A));
            PE_ERROR(g_last_error);
            return -1;
        }

        PE_INFO("Executing command B: " + std::string(cmd_B));

        if (system(cmd_B) != 0) {
            SetError("Failed to chunk document B (Python error). Command: " + std::string(cmd_B));
            PE_ERROR(g_last_error);
            return -1;
        }

        // Read chunked data
        std::vector<std::vector<int64_t>> input_ids_A, attention_mask_A, token_type_ids_A;
        std::vector<std::vector<int64_t>> input_ids_B, attention_mask_B, token_type_ids_B;

        if (!ReadChunksFromFile(chunks_A_file, input_ids_A, attention_mask_A, token_type_ids_A)) {
            SetError("Failed to read chunks for document A");
            return -1;
        }

        if (!ReadChunksFromFile(chunks_B_file, input_ids_B, attention_mask_B, token_type_ids_B)) {
            SetError("Failed to read chunks for document B");
            return -1;
        }

        // Compute embeddings for all chunks
        std::vector<std::vector<float>> embeddings_A, embeddings_B;

        if (!ComputeChunkEmbeddings(input_ids_A, attention_mask_A, token_type_ids_A, embeddings_A)) {
            SetError("Failed to compute embeddings for document A");
            return -1;
        }

        if (!ComputeChunkEmbeddings(input_ids_B, attention_mask_B, token_type_ids_B, embeddings_B)) {
            SetError("Failed to compute embeddings for document B");
            return -1;
        }

        // Stage 1: Coarse similarity (document-level)
        std::vector<float> doc_emb_A, doc_emb_B;
        AverageEmbeddings(embeddings_A, doc_emb_A);
        AverageEmbeddings(embeddings_B, doc_emb_B);

        float coarse_similarity = DotProduct(doc_emb_A.data(), doc_emb_B.data()) * 100.0f;

        // Early exit if coarse similarity is too low
        if (coarse_similarity < 60.0f) {
            *similarity_out = coarse_similarity;

            // Cleanup temp files
            DeleteFileA(temp_doc_A.c_str());
            DeleteFileA(temp_doc_B.c_str());
            DeleteFileA(chunks_A_file.c_str());
            DeleteFileA(chunks_B_file.c_str());

            return 0;
        }

        // Stage 2: Fine-grained similarity (cross-document chunk comparison)
        std::vector<ChunkComparisonResult> all_chunk_comparisons;
        all_chunk_comparisons.reserve(embeddings_A.size() * embeddings_B.size());

        // Compare all chunk pairs (A vs B only, no A vs A or B vs B)
        for (size_t i = 0; i < embeddings_A.size(); i++) {
            for (size_t j = 0; j < embeddings_B.size(); j++) {
                float sim = DotProduct(embeddings_A[i].data(), embeddings_B[j].data()) * 100.0f;
                
                ChunkComparisonResult result;
                result.chunk_index_A = (int)i;
                result.chunk_index_B = (int)j;
                result.similarity_score = sim;
                result.text_A = ExtractChunkText(doc_A_text, (int)i, chunk_size, overlap);
                result.text_B = ExtractChunkText(doc_B_text, (int)j, chunk_size, overlap);
                
                all_chunk_comparisons.push_back(result);
            }
        }

        // Sort by similarity score (highest first)
        std::sort(all_chunk_comparisons.begin(), all_chunk_comparisons.end(),
            [](const ChunkComparisonResult& a, const ChunkComparisonResult& b) {
                return a.similarity_score > b.similarity_score;
            });

        // Store top results (up to 100 pairs)
        {
            std::lock_guard<std::mutex> lock(g_comparison_mutex);
            int num_to_store = (std::min)((int)all_chunk_comparisons.size(), 100);
            g_last_comparison_results.assign(
                all_chunk_comparisons.begin(),
                all_chunk_comparisons.begin() + num_to_store
            );
        }

        // Calculate final similarity (top 10% average)
        int top_n = (std::max)(1, (int)(all_chunk_comparisons.size() * 0.1));
        float sum = 0.0f;
        for (int i = 0; i < top_n; i++) {
            sum += all_chunk_comparisons[i].similarity_score;
        }

        *similarity_out = sum / top_n;

        // Cleanup temp files
        DeleteFileA(temp_doc_A.c_str());
        DeleteFileA(temp_doc_B.c_str());
        DeleteFileA(chunks_A_file.c_str());
        DeleteFileA(chunks_B_file.c_str());

        PE_INFO("Document comparison complete. Stored " + std::to_string(g_last_comparison_results.size()) + " chunk pairs");

        return 0;

    } catch (const std::exception& e) {
        SetError(std::string("Document comparison error: ") + e.what());
        return -1;
    }
}

E5_API int E5_CompareDocumentsSimple(
    const char* doc_A_text,
    const char* doc_B_text,
    float* similarity_out) {

    return E5_CompareDocuments(doc_A_text, doc_B_text, 450, 50, similarity_out);
}

// ============================================================================
// Comparison Results Retrieval
// ============================================================================

E5_API int E5_GetSimilarChunks(
    E5_SimilarChunkPair* pairs_out,
    int max_pairs,
    int* num_pairs_out) {

    if (pairs_out == nullptr || num_pairs_out == nullptr) {
        SetError("NULL pointer provided");
        return -1;
    }

    if (max_pairs <= 0) {
        SetError("max_pairs must be positive");
        return -1;
    }

    try {
        std::lock_guard<std::mutex> lock(g_comparison_mutex);

        if (g_last_comparison_results.empty()) {
            SetError("No comparison results available. Call E5_CompareDocuments first");
            *num_pairs_out = 0;
            return -1;
        }

        // Return up to max_pairs results
        int num_to_return = (std::min)(max_pairs, (int)g_last_comparison_results.size());
        *num_pairs_out = num_to_return;

        for (int i = 0; i < num_to_return; i++) {
            const auto& result = g_last_comparison_results[i];
            
            pairs_out[i].chunk_index_A = result.chunk_index_A;
            pairs_out[i].chunk_index_B = result.chunk_index_B;
            pairs_out[i].similarity_score = result.similarity_score;
            
            // Copy text snippets (truncate if needed)
            strncpy_s(pairs_out[i].text_A, sizeof(pairs_out[i].text_A),
                      result.text_A.c_str(), _TRUNCATE);
            strncpy_s(pairs_out[i].text_B, sizeof(pairs_out[i].text_B),
                      result.text_B.c_str(), _TRUNCATE);
        }

        return 0;

    } catch (const std::exception& e) {
        SetError(std::string("Get similar chunks error: ") + e.what());
        return -1;
    }
}

E5_API void E5_ClearComparisonResults() {
    std::lock_guard<std::mutex> lock(g_comparison_mutex);
    g_last_comparison_results.clear();
    g_last_doc_A_text.clear();
    g_last_doc_B_text.clear();
}
