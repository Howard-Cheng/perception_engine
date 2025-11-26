#define E5EMBEDDING_EXPORTS
#include "E5EmbeddingDLL.h"
#include "config/ConfigManager.h"
#include "utils/Logger.h"  // NEW: Add Logger

#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <windows.h>

// Global state (initialized once)
namespace {
    std::unique_ptr<Ort::Env> g_env;
    std::unique_ptr<Ort::Session> g_session;
    std::mutex g_mutex;  // Protects initialization
    bool g_initialized = false;
    std::string g_last_error;
    std::wstring g_tokenizer_path;  // Store tokenizer path

    const int EMBEDDING_DIM = 256;  // google/embeddinggemma-300m uses 256-dim embeddings
    const int MAX_SEQ_LENGTH = 512;
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

    for (int i = 0; i < seq_length; i++) {
        if (attention_mask[i] == 1) {
            for (int j = 0; j < EMBEDDING_DIM; j++) {
                output[j] += hidden_states[i * EMBEDDING_DIM + j];
            }
            token_count++;
        }
    }

    if (token_count > 0) {
        for (int j = 0; j < EMBEDDING_DIM; j++) {
            output[j] /= token_count;
        }
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

E5_API int E5_Initialize(const wchar_t* model_path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Logger::GetInstance().Initialize("Embedding.log", LogLevel::DEBUG_L);

    if (g_initialized) {
        SetError("Already initialized");
        return -1;
    }


    // =========================================
    // Load Configuration
    // =========================================
    LOG_INFO("Loading configuration from config.ini...");
    if (!ConfigManager::GetInstance().LoadConfig("config.ini")) {
        LOG_WARN("Failed to load config.ini, using default values");
        LOG_WARN(std::string("Error: ") + ConfigManager::GetInstance().GetLastError());
    }
    else {
        LOG_INFO("Configuration loaded successfully");
    }

    // Validate configuration
    if (!ConfigManager::GetInstance().ValidateConfiguration()) {
        LOG_ERROR("Configuration validation failed:");
        LOG_ERROR(ConfigManager::GetInstance().GetLastError());
        LOG_WARN("Continuing with best-effort configuration...");
    }
    else {
        LOG_INFO("Configuration validated successfully");
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

    try {
        // Prepare input tensors (batch size = 1)
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> input_shape = {1, length};

        // We need to copy data because ONNX Runtime expects non-const pointers
        std::vector<int64_t> input_ids_copy(input_ids, input_ids + length);
        std::vector<int64_t> attention_mask_copy(attention_mask, attention_mask + length);
        std::vector<int64_t> token_type_ids_copy(token_type_ids, token_type_ids + length);

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

        // Average pool
        AveragePool(output_data, attention_mask, length, embedding_out);

        // Normalize
        Normalize(embedding_out);

        return 0;

    } catch (const Ort::Exception& e) {
        SetError(std::string("ONNX Runtime inference error: ") + e.what());
        return -1;
    } catch (const std::exception& e) {
        SetError(std::string("Embedding computation error: ") + e.what());
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

E5_API void E5_Cleanup() {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_session.reset();
    g_env.reset();
    g_initialized = false;
    g_last_error.clear();
}

// ============================================================================
// Document Comparison Functions
// ============================================================================

// Helper: Read binary chunks file
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

// Helper: Compute embeddings for all chunks
bool ComputeChunkEmbeddings(
    const std::vector<std::vector<int64_t>>& input_ids,
    const std::vector<std::vector<int64_t>>& attention_mask,
    const std::vector<std::vector<int64_t>>& token_type_ids,
    std::vector<std::vector<float>>& embeddings_out) {

    int num_chunks = input_ids.size();
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

// Helper: Compute average of embeddings
void AverageEmbeddings(const std::vector<std::vector<float>>& embeddings,
                       std::vector<float>& avg_out) {
    avg_out.resize(EMBEDDING_DIM, 0.0f);

    for (const auto& emb : embeddings) {
        for (int i = 0; i < EMBEDDING_DIM; i++) {
            avg_out[i] += emb[i];
        }
    }

    for (int i = 0; i < EMBEDDING_DIM; i++) {
        avg_out[i] /= embeddings.size();
    }

    // Normalize
    Normalize(avg_out.data());
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
        // Get paths from ConfigManager
        std::string python_executable = ConfigManager::GetInstance().GetPythonExecutable();
        std::string chunk_script = ConfigManager::GetInstance().GetChunkDocumentScript();

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

        LOG_INFO("Executable directory: " + exe_dir);
        LOG_INFO("Python executable: " + python_executable);
        LOG_INFO("Chunk script: " + chunk_script);

        // Save documents to temp files
        if (!SaveTextToFile(temp_doc_A, doc_A_text)) {
            SetError("Failed to save document A to temp file: " + temp_doc_A);
            LOG_ERROR(g_last_error);
            return -1;
        }

        if (!SaveTextToFile(temp_doc_B, doc_B_text)) {
            SetError("Failed to save document B to temp file: " + temp_doc_B);
            LOG_ERROR(g_last_error);
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

        LOG_INFO("Executing command A: " + std::string(cmd_A));

        if (system(cmd_A) != 0) {
            SetError("Failed to chunk document A (Python error). Command: " + std::string(cmd_A));
            LOG_ERROR(g_last_error);
            return -1;
        }

        LOG_INFO("Executing command B: " + std::string(cmd_B));

        if (system(cmd_B) != 0) {
            SetError("Failed to chunk document B (Python error). Command: " + std::string(cmd_B));
            LOG_ERROR(g_last_error);
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
        std::vector<float> all_similarities;
        all_similarities.reserve(embeddings_A.size() * embeddings_B.size());

        // Compare all chunk pairs (A vs B only, no A vs A or B vs B)
        for (const auto& emb_A : embeddings_A) {
            for (const auto& emb_B : embeddings_B) {
                float sim = DotProduct(emb_A.data(), emb_B.data()) * 100.0f;
                all_similarities.push_back(sim);
            }
        }

        // Take top 10% average
        std::sort(all_similarities.begin(), all_similarities.end(), std::greater<float>());
        int top_n = (std::max)(1, (int)(all_similarities.size() * 0.1));

        float sum = 0.0f;
        for (int i = 0; i < top_n; i++) {
            sum += all_similarities[i];
        }

        *similarity_out = sum / top_n;

        // Cleanup temp files
        DeleteFileA(temp_doc_A.c_str());
        DeleteFileA(temp_doc_B.c_str());
        DeleteFileA(chunks_A_file.c_str());
        DeleteFileA(chunks_B_file.c_str());

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
