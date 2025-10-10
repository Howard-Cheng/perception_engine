#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

/**
 * @brief Camera vision engine using FastVLM ONNX models for scene description
 *
 * Architecture:
 *   Camera → Vision Encoder → Image Features → Text Decoder → Scene Description
 *
 * Models (q4f16 quantization):
 *   - vision_encoder_q4f16.onnx (241MB): Extracts image features
 *   - embed_tokens_q4f16.onnx (260MB): Embeds text tokens
 *   - decoder_model_merged_q4f16.onnx (270MB): Generates text description
 *
 * Performance:
 *   - Vision encoder: 250-400ms (CPU)
 *   - Decoder: 1200-1800ms for 50 tokens (CPU)
 *   - Total: 1.5-2.5 seconds (vs 8-12s Python)
 */
class CameraVisionEngine {
public:
    CameraVisionEngine();
    ~CameraVisionEngine();

    /**
     * @brief Initialize the camera and ONNX models
     * @param modelPath Path to models directory (contains onnx/ subfolder)
     * @param cameraIndex Camera device index (default: 0)
     * @return true if initialization successful
     */
    bool Initialize(const std::string& modelPath, int cameraIndex = 0);

    /**
     * @brief Capture frame and generate scene description
     * @return Scene description text (empty on error)
     */
    std::string DescribeScene();

    /**
     * @brief Get last inference latency in milliseconds
     */
    float GetLastLatencyMs() const { return lastLatencyMs; }

    /**
     * @brief Check if engine is initialized and ready
     */
    bool IsReady() const { return isInitialized; }

private:
    // ONNX Runtime components
    std::unique_ptr<Ort::Env> ortEnv;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session> visionEncoder;
    std::unique_ptr<Ort::Session> embedTokens;
    std::unique_ptr<Ort::Session> decoder;
    std::unique_ptr<Ort::MemoryInfo> memoryInfo;

    // Camera
    cv::VideoCapture camera;

    // Tokenizer (simplified - hardcoded tokens for now)
    static constexpr int IMAGE_TOKEN_ID = 128256;  // <image> token
    static constexpr int BOS_TOKEN_ID = 128000;    // Beginning of sequence
    static constexpr int EOS_TOKEN_ID = 128001;    // End of sequence

    // State
    bool isInitialized;
    float lastLatencyMs;

    // Model constants
    static constexpr int NUM_LAYERS = 24;          // Decoder layers (for KV cache)
    static constexpr int HIDDEN_SIZE = 896;        // Model hidden dimension
    static constexpr int NUM_HEADS = 14;           // Attention heads
    static constexpr int HEAD_DIM = 64;            // Head dimension

    /**
     * @brief Preprocess camera frame for vision encoder
     * @param frame Input BGR frame from camera
     * @param output Output float tensor (1, 3, 224, 224)
     */
    void PreprocessImage(const cv::Mat& frame, std::vector<float>& output);

    /**
     * @brief Run vision encoder on preprocessed image
     * @param imageData Preprocessed image (1, 3, 224, 224)
     * @return Image features (1, 16, 896)
     */
    std::vector<float> RunVisionEncoder(const std::vector<float>& imageData);

    /**
     * @brief Embed tokens and combine with image features
     * @param tokens Pre-tokenized prompt tokens
     * @param imageFeatures Image features from vision encoder
     * @return Combined embeddings (image + text)
     */
    std::vector<float> TokenizeAndEmbed(const std::vector<int64_t>& tokens, const std::vector<float>& imageFeatures);

    /**
     * @brief Run decoder to generate text
     * @param inputEmbeds Combined text + image embeddings
     * @param maxTokens Maximum tokens to generate
     * @return Generated token IDs
     */
    std::vector<int64_t> Generate(const std::vector<float>& inputEmbeds, int maxTokens = 50);

    /**
     * @brief Decode token IDs to text (simplified)
     * @param tokenIds Generated token IDs
     * @return Decoded text
     */
    std::string DecodeTokens(const std::vector<int64_t>& tokenIds);

    /**
     * @brief Load ONNX model
     * @param modelPath Path to .onnx file
     * @return ONNX session
     */
    std::unique_ptr<Ort::Session> LoadOnnxModel(const std::string& modelPath);
};
