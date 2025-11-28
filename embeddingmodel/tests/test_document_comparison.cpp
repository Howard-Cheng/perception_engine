/**
 * Test Document Comparison - Compare two large documents
 *
 * This demonstrates the E5_CompareDocuments API for comparing
 * large documents (100k+ tokens) using automatic chunking.
 *
 * Compile:
 *   cmake -B build -G "Visual Studio 17 2022" -A x64
 *   cmake --build build --config Release
 *
 * Run:
 *   cd build\bin\Release
 *   test_document_comparison.exe [model_path] [doc_A_file] [doc_B_file]
 *
 * Examples:
 *   test_document_comparison.exe
 *   test_document_comparison.exe models/embedding/model_q4.onnx
 *   test_document_comparison.exe models/embedding/model_q4.onnx doc1.txt doc2.txt
 */

#include "E5EmbeddingDLL.h"
#include "config/ConfigManager.h"  // Add ConfigManager
#include "pe_base/logger.h"          // Add Logger
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <Windows.h>
#include <filesystem>

// Helper: Read file to string
std::string ReadFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper: Convert std::string to std::wstring
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

void PrintUsage(const char* programName) {
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  " << programName << " [model_path] [doc_A_file] [doc_B_file]" << std::endl;
    std::cout << "\nArguments:" << std::endl;
    std::cout << "  model_path   - Path to ONNX model file (default: from config.ini or models/embedding/model_q4.onnx)" << std::endl;
    std::cout << "  doc_A_file   - Path to first document (optional, for file comparison)" << std::endl;
    std::cout << "  doc_B_file   - Path to second document (optional, for file comparison)" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << programName << std::endl;
    std::cout << "  " << programName << " models/embedding/model_q4.onnx" << std::endl;
    std::cout << "  " << programName << " models/embedding/model_q4.onnx doc1.txt doc2.txt" << std::endl;
    std::cout << "\nNote: If no model path is provided, it will be loaded from config.ini" << std::endl;
    std::cout << std::endl;
}

std::string GetExePath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exePathStr(exePath);
    size_t lastSlash = exePathStr.find_last_of("\\/");
    std::string exe_dir = exePathStr.substr(0, lastSlash);
    return exe_dir;
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  E5 Embedding - Document Comparison" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Initialize Logger
    std::filesystem::path log_path = "";
     if (auto* p_appdata = getenv("APPDATA")) {
        log_path =
            std::filesystem::path(p_appdata) / "Lenovo" / "PerceptionEngine" / "logs";
    }
    pe_base::LogWriter::SetLogFilePrefix(
        (log_path / "test_doc_compare").generic_string());
    PE_INFO("Test embedding comparison started");


    // Create temp files in the same directory as executable
    std::string config_path = GetExePath() + "\\config.ini";
    // Load configuration
    std::cout << "Loading configuration..." << std::endl;
    if (!ConfigManager::GetInstance().LoadConfig(config_path)) {
        PE_WARN("Failed to load config.ini, using default values");
        std::cout << "Warning: Failed to load config.ini, using default values" << std::endl;
    } else {
        PE_INFO("Configuration loaded successfully");
        std::cout << "Configuration loaded from config.ini" << std::endl;
    }

    // Get model path from command line or ConfigManager
    std::wstring modelPath;
    if (argc >= 2) {
        // Convert first argument to wide string
        std::string pathArg(argv[1]);
        modelPath = StringToWString(pathArg);
        std::wcout << L"Model path (from command line): " << modelPath << std::endl;
        PE_INFO(std::string("Using model path from command line: ") + pathArg);
    } else {
        // Use ConfigManager to get model path
        modelPath = ConfigManager::GetInstance().GetEmbeddingModelPath();
        std::wcout << L"Model path (from config.ini): " << modelPath << std::endl;
        PE_INFO("Using model path from ConfigManager");
        PrintUsage(argv[0]);
    }

    // Initialize the model
    std::cout << "\nInitializing E5 embedding model..." << std::endl;
    PE_INFO("Initializing E5 embedding model...");
    int result = E5_Initialize(modelPath.c_str());
    if (result != 0) {
        std::cerr << "X Initialization failed: " << E5_GetLastError() << std::endl;
        std::cerr << "  Make sure the model file exists at the specified path" << std::endl;
        PE_ERROR(std::string("Initialization failed: ") + E5_GetLastError());
        return 1;
    }
    std::cout << "V Model loaded successfully!" << std::endl;
    std::cout << "  Embedding dimension: " << E5_GetEmbeddingDimension() << std::endl;
    std::cout << "  Max sequence length: " << E5_GetMaxSequenceLength() << std::endl;
    std::cout << std::endl;
    PE_INFO("Model loaded successfully");

    // Test Case 1: Similar documents (sample text about AI/ML)
    std::cout << "========================================" << std::endl;
    std::cout << "  Test 1: Similar Documents (AI/ML)" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string doc_A = R"(
Machine learning is a subset of artificial intelligence that focuses on the development
of algorithms and statistical models that enable computer systems to improve their
performance on a specific task through experience. Deep learning, a subfield of machine
learning, uses neural networks with multiple layers to progressively extract higher-level
features from raw input. These deep neural networks have revolutionized computer vision,
natural language processing, and speech recognition. The success of deep learning can be
attributed to the availability of large datasets, powerful GPUs for parallel computation,
and innovations in network architectures such as convolutional neural networks (CNNs) and
recurrent neural networks (RNNs). Modern applications of deep learning include image
classification, object detection, machine translation, and autonomous driving systems.
)";

    std::string doc_B = R"(
Deep learning represents a powerful approach within the field of artificial intelligence,
utilizing artificial neural networks with multiple hidden layers to learn complex patterns
from data. This technology has achieved remarkable success in various domains including
computer vision, where convolutional neural networks excel at image recognition tasks,
and natural language processing, where transformer architectures have enabled breakthrough
performance in language understanding. The recent advances in deep learning are driven by
the combination of massive datasets, GPU-accelerated computing, and novel network designs.
Applications range from facial recognition and medical image analysis to language translation
and self-driving cars, demonstrating the versatility and power of these techniques.
)";

    float similarity1;
    std::cout << "Comparing AI/ML documents..." << std::endl;
    result = E5_CompareDocumentsSimple(doc_A.c_str(), doc_B.c_str(), &similarity1);

    if (result != 0) {
        std::cerr << "X Comparison failed: " << E5_GetLastError() << std::endl;
    } else {
        std::cout << "  Similarity score: " << similarity1 << " / 100" << std::endl;
        if (similarity1 >= 70.0f) {
            std::cout << "  V Result: SIMILAR documents (threshold: 70)" << std::endl;
        } else {
            std::cout << "  X Result: NOT similar documents" << std::endl;
        }
        
        // NEW: Retrieve and display similar chunks
        std::cout << "\n  Retrieving most similar chunks..." << std::endl;
        E5_SimilarChunkPair similar_chunks[5];  // Get top 5
        int num_chunks = 0;
        
        result = E5_GetSimilarChunks(similar_chunks, 5, &num_chunks);
        if (result == 0 && num_chunks > 0) {
            std::cout << "  Found " << num_chunks << " similar chunk pairs:" << std::endl;
            for (int i = 0; i < num_chunks; i++) {
                std::cout << "\n  --- Pair " << (i + 1) << " (Score: " << similar_chunks[i].similarity_score << ") ---" << std::endl;
                std::cout << "  Doc A [chunk " << similar_chunks[i].chunk_index_A << "]: " << std::endl;
                std::cout << "    " << std::string(similar_chunks[i].text_A).substr(0, 200) << "..." << std::endl;
                std::cout << "  Doc B [chunk " << similar_chunks[i].chunk_index_B << "]: " << std::endl;
                std::cout << "    " << std::string(similar_chunks[i].text_B).substr(0, 200) << "..." << std::endl;
            }
        } else if (result != 0) {
            std::cout << "  Note: Could not retrieve chunks: " << E5_GetLastError() << std::endl;
        }
    }

    std::cout << std::endl;

    // Test Case 2: Different documents (AI vs Music)
    std::cout << "========================================" << std::endl;
    std::cout << "  Test 2: Different Documents (AI vs Music)" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string doc_C = R"(
Classical music is an art form that has evolved over centuries, with roots in Western
liturgical and secular music traditions. The classical period (1750-1820) saw composers
like Mozart, Haydn, and Beethoven develop the symphony, sonata, and concerto forms that
remain central to the repertoire today. Orchestra composition expanded during the Romantic
era, with composers such as Wagner, Brahms, and Tchaikovsky creating emotionally expressive
works that pushed the boundaries of harmony and orchestration. Modern classical music in
the 20th century embraced atonality, serialism, and experimental techniques, with composers
like Schoenberg, Stravinsky, and John Cage challenging traditional notions of melody and
rhythm. Today's classical music scene includes both traditional performances of historical
works and contemporary compositions that incorporate electronic elements and cross-cultural
influences.
)";

    float similarity2;
    std::cout << "Comparing AI vs Music documents..." << std::endl;
    result = E5_CompareDocumentsSimple(doc_A.c_str(), doc_C.c_str(), &similarity2);

    if (result != 0) {
        std::cerr << "X Comparison failed: " << E5_GetLastError() << std::endl;
    } else {
        std::cout << "  Similarity score: " << similarity2 << " / 100" << std::endl;
        if (similarity2 >= 70.0f) {
            std::cout << "  X Result: SIMILAR documents (unexpected)" << std::endl;
        } else {
            std::cout << "  V Result: NOT similar documents (threshold: 70)" << std::endl;
        }
        
        // NEW: Retrieve and display similar chunks
        std::cout << "\n  Retrieving most similar chunks..." << std::endl;
        E5_SimilarChunkPair similar_chunks[5];  // Get top 5
        int num_chunks = 0;
        
        result = E5_GetSimilarChunks(similar_chunks, 5, &num_chunks);
        if (result == 0 && num_chunks > 0) {
            std::cout << "  Found " << num_chunks << " similar chunk pairs:" << std::endl;
            for (int i = 0; i < num_chunks; i++) {
                std::cout << "\n  --- Pair " << (i + 1) << " (Score: " << similar_chunks[i].similarity_score << ") ---" << std::endl;
                std::cout << "  Doc A [chunk " << similar_chunks[i].chunk_index_A << "]: " << std::endl;
                std::cout << "    " << std::string(similar_chunks[i].text_A).substr(0, 200) << "..." << std::endl;
                std::cout << "  Doc B [chunk " << similar_chunks[i].chunk_index_B << "]: " << std::endl;
                std::cout << "    " << std::string(similar_chunks[i].text_B).substr(0, 200) << "..." << std::endl;
            }
        } else if (result != 0) {
            std::cout << "  Note: Could not retrieve chunks: " << E5_GetLastError() << std::endl;
        }
    }

    std::cout << std::endl;

    // Test Case 3: From file (if provided via command line)
    if (argc >= 4) {
        std::cout << "========================================" << std::endl;
        std::cout << "  Test 3: Documents from Files" << std::endl;
        std::cout << "========================================" << std::endl;

        std::string file_A_content = ReadFile(argv[2]);
        std::string file_B_content = ReadFile(argv[3]);

        if (!file_A_content.empty() && !file_B_content.empty()) {
            std::cout << "Document A: " << argv[2] << " (" << file_A_content.size() << " bytes)" << std::endl;
            std::cout << "Document B: " << argv[3] << " (" << file_B_content.size() << " bytes)" << std::endl;

            float similarity3;
            std::cout << "Comparing documents from files..." << std::endl;
            result = E5_CompareDocumentsSimple(
                file_A_content.c_str(),
                file_B_content.c_str(),
                &similarity3
            );

            if (result != 0) {
                std::cerr << "X Comparison failed: " << E5_GetLastError() << std::endl;
            } else {
                std::cout << "  Similarity score: " << similarity3 << " / 100" << std::endl;
                if (similarity3 >= 70.0f) {
                    std::cout << "  V Result: SIMILAR documents" << std::endl;
                } else {
                    std::cout << "  X Result: NOT similar documents" << std::endl;
                }
                
                // NEW: Retrieve and display similar chunks
                std::cout << "\n  Retrieving most similar chunks..." << std::endl;
                E5_SimilarChunkPair similar_chunks[10];  // Get top 10 for file comparison
                int num_chunks = 0;
                
                result = E5_GetSimilarChunks(similar_chunks, 10, &num_chunks);
                if (result == 0 && num_chunks > 0) {
                    std::cout << "  Found " << num_chunks << " similar chunk pairs:" << std::endl;
                    
                    // Show top 5 in detail
                    int show_count = (num_chunks < 5) ? num_chunks : 5;
                    for (int i = 0; i < show_count; i++) {
                        std::cout << "\n  --- Pair " << (i + 1) << " (Score: " 
                                 << similar_chunks[i].similarity_score << ") ---" << std::endl;
                        std::cout << "  File A [chunk " << similar_chunks[i].chunk_index_A << "]: " << std::endl;
                        std::cout << "    " << std::string(similar_chunks[i].text_A).substr(0, 200) << "..." << std::endl;
                        std::cout << "  File B [chunk " << similar_chunks[i].chunk_index_B << "]: " << std::endl;
                        std::cout << "    " << std::string(similar_chunks[i].text_B).substr(0, 200) << "..." << std::endl;
                    }
                    
                    // Show summary of all
                    if (num_chunks > 5) {
                        std::cout << "\n  (Showing top 5 of " << num_chunks << " total pairs)" << std::endl;
                        std::cout << "  Other similarity scores: ";
                        for (int i = 5; i < num_chunks && i < 10; i++) {
                            std::cout << similar_chunks[i].similarity_score << " ";
                        }
                        std::cout << std::endl;
                    }
                } else if (result != 0) {
                    std::cout << "  Note: Could not retrieve chunks: " << E5_GetLastError() << std::endl;
                }
            }
        } else {
            std::cerr << "X Failed to read one or both files" << std::endl;
        }

        std::cout << std::endl;
    } else if (argc == 3) {
        std::cout << "\nNote: To compare files, provide both document paths:" << std::endl;
        std::cout << "  " << argv[0] << " " << argv[1] << " doc1.txt doc2.txt" << std::endl;
        std::cout << std::endl;
    }

    // Cleanup
    std::cout << "========================================" << std::endl;
    std::cout << "Cleaning up..." << std::endl;
    
    // Clear comparison results
    E5_ClearComparisonResults();
    std::cout << "V Comparison results cleared" << std::endl;
    
    E5_Cleanup();
    std::cout << "V Cleanup complete" << std::endl;

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  All Tests Complete!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
