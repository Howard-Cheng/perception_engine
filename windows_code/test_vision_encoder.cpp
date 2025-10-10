// Minimal test for vision encoder ONNX model
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>

int main() {
    try {
        std::cout << "Creating ONNX Runtime environment..." << std::endl;
        Ort::Env env(ORT_LOGGING_LEVEL_VERBOSE, "test_vision_encoder");

        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

        std::wstring modelPath = L"models/fastvlm/onnx/vision_encoder_simplified.onnx";
        std::cout << "Loading model..." << std::endl;
        Ort::Session session(env, modelPath.c_str(), sessionOptions);

        std::cout << "Model loaded successfully!" << std::endl;

        // Create input tensor
        std::vector<int64_t> inputShape = {1, 3, 224, 224};
        size_t inputSize = 1 * 3 * 224 * 224;
        std::vector<float> inputData(inputSize, 0.5f);  // Fill with 0.5

        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputData.data(),
            inputSize,
            inputShape.data(),
            inputShape.size()
        );

        std::cout << "Input tensor created" << std::endl;

        const char* inputNames[] = {"pixel_values"};
        const char* outputNames[] = {"image_features"};

        std::cout << "Running inference..." << std::endl;
        auto outputTensors = session.Run(
            Ort::RunOptions{nullptr},
            inputNames,
            &inputTensor,
            1,
            outputNames,
            1
        );

        std::cout << "Inference complete!" << std::endl;

        auto tensorInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();
        std::cout << "Output shape: [";
        for (size_t i = 0; i < shape.size(); ++i) {
            std::cout << shape[i];
            if (i < shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
