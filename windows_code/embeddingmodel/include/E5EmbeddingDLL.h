#pragma once

/**
 * E5 Embedding DLL - Simple API for Text Embeddings
 *
 * Usage:
 *   1. E5_Initialize() - Load model once at startup
 *   2. E5_ComputeSimilarity() - Compare two texts
 *   3. E5_Cleanup() - Free resources at shutdown
 *
 * Thread Safety: Safe to call from multiple threads after initialization
 *
 * Note: Your application must handle:
 *   - Tokenization (use Python or include tokenizer)
 *   - Chunking long documents
 *   - Batching multiple queries
 */

#ifdef E5EMBEDDING_EXPORTS
#define E5_API __declspec(dllexport)
#else
#define E5_API __declspec(dllimport)
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the E5 embedding model
 *
 * @param model_path Path to model.onnx file (wide string for Windows)
 * @return 0 on success, negative on error
 *
 * Must be called once before any other functions.
 * Thread-safe after initialization completes.
 */
E5_API int E5_Initialize(const wchar_t* model_path);

/**
 * Compute similarity between two texts (pairwise comparison)
 *
 * @param input_ids1 Token IDs for text 1 (from your tokenizer)
 * @param attention_mask1 Attention mask for text 1
 * @param token_type_ids1 Token type IDs for text 1 (can be all zeros)
 * @param length1 Number of tokens for text 1 (max 512)
 * @param input_ids2 Token IDs for text 2
 * @param attention_mask2 Attention mask for text 2
 * @param token_type_ids2 Token type IDs for text 2 (can be all zeros)
 * @param length2 Number of tokens for text 2 (max 512)
 * @param similarity_out Output: similarity score (0-100)
 *
 * @return 0 on success, negative on error
 *
 * Thread-safe.
 */
E5_API int E5_ComputeSimilarity(
    const int64_t* input_ids1,
    const int64_t* attention_mask1,
    const int64_t* token_type_ids1,
    int length1,
    const int64_t* input_ids2,
    const int64_t* attention_mask2,
    const int64_t* token_type_ids2,
    int length2,
    float* similarity_out
);

/**
 * Compute embedding for a single text
 *
 * @param input_ids Token IDs (from your tokenizer)
 * @param attention_mask Attention mask
 * @param token_type_ids Token type IDs (can be all zeros)
 * @param length Number of tokens (max 512)
 * @param embedding_out Output: normalized embedding (384 floats)
 *                      Caller must allocate 384 * sizeof(float) bytes
 *
 * @return 0 on success, negative on error
 *
 * Thread-safe.
 */
E5_API int E5_ComputeEmbedding(
    const int64_t* input_ids,
    const int64_t* attention_mask,
    const int64_t* token_type_ids,
    int length,
    float* embedding_out
);

/**
 * Compute embedding for multiple texts in a batch
 *
 * @param input_ids Flattened array of token IDs [batch_size * max_length]
 * @param attention_mask Flattened array of attention masks
 * @param token_type_ids Flattened array of token type IDs
 * @param batch_size Number of texts
 * @param max_length Maximum sequence length (typically 512)
 * @param embeddings_out Output: normalized embeddings [batch_size * 384]
 *                       Caller must allocate batch_size * 384 * sizeof(float) bytes
 *
 * @return 0 on success, negative on error
 *
 * Thread-safe.
 */
E5_API int E5_ComputeEmbeddingBatch(
    const int64_t* input_ids,
    const int64_t* attention_mask,
    const int64_t* token_type_ids,
    int batch_size,
    int max_length,
    float* embeddings_out
);

/**
 * Get the embedding dimension (always 384 for this model)
 *
 * @return Embedding dimension
 */
E5_API int E5_GetEmbeddingDimension();

/**
 * Get the maximum sequence length (always 512 for this model)
 *
 * @return Maximum sequence length
 */
E5_API int E5_GetMaxSequenceLength();

/**
 * Check if model is initialized
 *
 * @return 1 if initialized, 0 otherwise
 */
E5_API int E5_IsInitialized();

/**
 * Get last error message
 *
 * @return Error message string (valid until next error)
 */
E5_API const char* E5_GetLastError();

/**
 * Structure to hold information about a similar chunk pair
 */
typedef struct E5_SimilarChunkPair {
    int chunk_index_A;          // Index of chunk in document A
    int chunk_index_B;          // Index of chunk in document B
    float similarity_score;     // Similarity score (0-100)
    char text_A[512];          // Text snippet from document A
    char text_B[512];          // Text snippet from document B
} E5_SimilarChunkPair;

/**
 * Get the most similar chunk pairs from the last document comparison
 *
 * @param pairs_out Output: array of similar chunk pairs
 * @param max_pairs Maximum number of pairs to return
 * @param num_pairs_out Output: actual number of pairs returned
 *
 * @return 0 on success, negative on error
 *
 * This function must be called after E5_CompareDocuments or E5_CompareDocumentsSimple.
 * It returns the top N most similar chunk pairs found in the comparison.
 * The results are sorted by similarity score (highest first).
 *
 * Note: Caller must allocate memory for pairs_out array.
 * Thread-safe (returns data from last comparison in same thread context).
 */
E5_API int E5_GetSimilarChunks(
    E5_SimilarChunkPair* pairs_out,
    int max_pairs,
    int* num_pairs_out
);

/**
 * Clear the stored comparison results
 *
 * Call this to free memory used for storing chunk comparison results.
 */
E5_API void E5_ClearComparisonResults();

/**
 * Compare two large documents (handles chunking internally)
 *
 * @param doc_A_text Raw text of document A (UTF-8 encoded)
 * @param doc_B_text Raw text of document B (UTF-8 encoded)
 * @param chunk_size Size of each chunk in tokens (default: 450)
 * @param overlap Overlap between chunks in tokens (default: 50)
 * @param similarity_out Output: similarity score (0-100)
 *
 * @return 0 on success, negative on error
 *
 * This function:
 * 1. Calls Python to chunk and tokenize both documents
 * 2. Computes embeddings for all chunks
 * 3. Compares cross-document chunks (A vs B only, not A vs A)
 * 4. Returns final similarity score
 *
 * Note: Requires Python with transformers installed
 * Thread-safe.
 */
E5_API int E5_CompareDocuments(
    const char* doc_A_text,
    const char* doc_B_text,
    int chunk_size,
    int overlap,
    float* similarity_out
);

/**
 * Compare two large documents (simple version with defaults)
 *
 * @param doc_A_text Raw text of document A (UTF-8 encoded)
 * @param doc_B_text Raw text of document B (UTF-8 encoded)
 * @param similarity_out Output: similarity score (0-100)
 *
 * @return 0 on success, negative on error
 *
 * Uses default chunk_size=450, overlap=50
 */
E5_API int E5_CompareDocumentsSimple(
    const char* doc_A_text,
    const char* doc_B_text,
    float* similarity_out
);

/**
 * Cleanup and free all resources
 *
 * Call this at application shutdown.
 * After calling this, you must call E5_Initialize again before using other functions.
 */
E5_API void E5_Cleanup();

#ifdef __cplusplus
}
#endif
