cmake_minimum_required(VERSION 3.15)
project(PerceptionEngine VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================

# Elasticsearch Client DLL (subdirectory)
# ============================================================================

# Add database_client as a subdirectory
add_subdirectory(database_client)

# ============================================================================

# Dependencies
# ============================================================================

# whisper.cpp (CUDA-enabled for GPU acceleration)
set(WHISPER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/include")
set(WHISPER_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/src/Release")
set(GGML_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/ggml/src/Release")

# Check if whisper.lib exists
if(NOT EXISTS "${WHISPER_LIB_DIR}/whisper.lib")
    message(FATAL_ERROR "whisper.lib not found! Run copy_whisper_lib.bat first")
endif()

# Check if ggml libraries exist
if(NOT EXISTS "${GGML_LIB_DIR}/ggml.lib")
    message(FATAL_ERROR "ggml.lib not found! Expected at: ${GGML_LIB_DIR}/ggml.lib")
endif()

# nlohmann/json (header-only)
set(JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/include")

# ONNX Runtime
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/onnxruntime")
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")

# Check if ONNX Runtime exists
if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}")
    message(FATAL_ERROR "ONNX Runtime not found! Run: python download_onnxruntime.py")
endif()

# OpenCV
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/opencv/opencv/build")
set(OpenCV_INCLUDE_DIRS "${OpenCV_DIR}/include")
set(OpenCV_LIB_DIR "${OpenCV_DIR}/x64/vc16/lib")
set(OpenCV_BIN_DIR "${OpenCV_DIR}/x64/vc16/bin")

# Find OpenCV world library
file(GLOB OPENCV_WORLD_LIB "${OpenCV_LIB_DIR}/opencv_world*.lib")

# Elasticsearch Client (built as subdirectory)
# The database_client target is now available from the subdirectory
set(ELASTICSEARCH_AVAILABLE TRUE)
message(STATUS "Elasticsearch client enabled (built from subdirectory)")

# ============================================================================

# Source Files
# ============================================================================

set(PERCEPTION_ENGINE_SOURCES
    PerceptionEngine.cpp
    Logger.cpp
    ContextCollector.cpp
    HttpServer.cpp
    WindowsAPIs.cpp
    WindowsService.cpp
    WindowEventMonitor.cpp
    AudioCaptureEngine.cpp
    AsyncWhisperQueue.cpp
    SileroVAD.cpp
    BrowserContentExtractor.cpp
    MouseTracker.cpp
    
    # NEW: Refactored Context Collector (Strategy Pattern)
    ContextCollectorRefactored.cpp
)

set(PERCEPTION_ENGINE_HEADERS
    Logger.h
    ContextCollector.h
    HttpServer.h
    WindowsAPIs.h
    WindowsService.h
    WindowEventMonitor.h
    AudioCaptureEngine.h
    AsyncWhisperQueue.h
    SileroVAD.h
    BrowserContentExtractor.h
    MouseTracker.h
    
    # NEW: Refactored Context Collector Headers (Strategy + Composite Pattern)
    IContextProvider.h
    SystemContextProvider.h
    VoiceContextProvider.h
    CameraContextProvider.h
    AppActivityContextProvider.h
    CompositeContextManager.h
    ContextCollectorRefactored.h
    ContextCollectorAdapter.h
    json.hpp
)

# ============================================================================

# Executables
# ============================================================================

# Main PerceptionEngine service
add_executable(PerceptionEngine ${PERCEPTION_ENGINE_SOURCES} ${PERCEPTION_ENGINE_HEADERS})

# ============================================================================

# Include Directories
# ============================================================================

target_include_directories(PerceptionEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WHISPER_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/ggml/include
    ${JSON_INCLUDE_DIR}
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Add Elasticsearch include directory (from subdirectory target)
if(ELASTICSEARCH_AVAILABLE)
    target_include_directories(PerceptionEngine PRIVATE
        $<TARGET_PROPERTY:database_client,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(PerceptionEngine PRIVATE
        ELASTICSEARCH_ENABLED
    )
endif()

# ============================================================================

# Link Libraries
# ============================================================================

target_link_libraries(PerceptionEngine PRIVATE
    # whisper.cpp and ggml
    ${WHISPER_LIB_DIR}/whisper.lib
    ${GGML_LIB_DIR}/ggml.lib
    ${GGML_LIB_DIR}/ggml-base.lib
    ${GGML_LIB_DIR}/ggml-cpu.lib
    # NOTE: NOT linking ggml-cuda.lib - it's dynamically loaded via ggml-cuda.dll when use_gpu=true
    # This allows fallback to CPU if CUDA runtime is not available

    # ONNX Runtime
    ${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib

    # OpenCV
    ${OPENCV_WORLD_LIB}

    # Windows APIs
    ws2_32      # Winsock
    ole32       # COM
    winmm       # Multimedia

    # WinRT (for location services)
    WindowsApp  # WinRT APIs
)

# Link Elasticsearch client library (from subdirectory target)
if(ELASTICSEARCH_AVAILABLE)
    target_link_libraries(PerceptionEngine PRIVATE
        database_client
    )
endif()

# ============================================================================

# Compiler Flags
# ============================================================================

if(MSVC)
    target_compile_options(PerceptionEngine PRIVATE
        /W3                 # Warning level 3
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable __cplusplus macro
        /await              # Enable coroutines (for WinRT)
        /EHsc               # Exception handling
    )

    # Add WinRT support
    target_compile_definitions(PerceptionEngine PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WINVER=0x0A00       # Windows 10
        _WIN32_WINNT=0x0A00
    )

endif()

# ============================================================================

# Post-build: Copy models and DLLs to output directory
# ============================================================================

add_custom_command(TARGET PerceptionEngine POST_BUILD
    # Copy whisper model to bin/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/whisper
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../models/whisper/ggml-small.bin
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/whisper/ggml-small.bin

    # Copy Silero VAD model to bin/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/vad
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../models/vad/silero_vad.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/vad/silero_vad.onnx

    # Copy ONNX Runtime DLL to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/onnxruntime.dll

    # Copy OpenCV DLL to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OpenCV_BIN_DIR}/opencv_world4100.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/opencv_world4100.dll

    # Copy whisper.cpp DLLs to bin/ (CUDA build)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/whisper.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/whisper.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ggml.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml-base.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ggml-base.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml-cpu.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ggml-cpu.dll
    # Copy ggml-cuda.dll if it exists (CUDA support may not be built)
    COMMAND ${CMAKE_COMMAND} -E echo "Checking for CUDA support..."
    COMMAND ${CMAKE_COMMAND} -E true
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml-cuda.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ggml-cuda.dll || ${CMAKE_COMMAND} -E echo "Warning: ggml-cuda.dll not found, CUDA acceleration may not be available"

    # Copy CUDA runtime DLLs to bin/ (required for GPU acceleration)
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cudart64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cudart64_13.dll
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cublas64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cublas64_13.dll
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cublasLt64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cublasLt64_13.dll

    # Copy dashboard.html to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/dashboard.html
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dashboard.html

    # For MSVC multi-config: also copy to bin/Release/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/whisper
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/vad
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../models/whisper/ggml-small.bin
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/whisper/ggml-small.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../models/vad/silero_vad.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/vad/silero_vad.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/onnxruntime.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OpenCV_BIN_DIR}/opencv_world4100.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/opencv_world4100.dll

    # Copy whisper.cpp DLLs to bin/Release/ (CUDA build)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/whisper.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/whisper.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/ggml.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml-base.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/ggml-base.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build_cuda/bin/Release/ggml-cpu.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/ggml-cpu.dll
    # Skip ggml-cuda.dll for Debug build
    COMMAND ${CMAKE_COMMAND} -E echo "Note: ggml-cuda.dll not copied (file not found)"

    # Copy CUDA runtime DLLs to bin/Release/ (required for GPU acceleration)
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cudart64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/cudart64_13.dll
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cublas64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/cublas64_13.dll
    # COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #     "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v13.0/bin/x64/cublasLt64_13.dll"
    #     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/cublasLt64_13.dll

    # Copy dashboard.html to bin/Release/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/dashboard.html
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/dashboard.html

    COMMENT "Copying models and DLLs to output directory"
)

# ============================================================================

# Installation (optional)
# ============================================================================

install(TARGETS PerceptionEngine
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/models/
    DESTINATION bin/models
)

# ============================================================================

# Print Configuration
# ============================================================================

message(STATUS "")
message(STATUS "=== PerceptionEngine Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Whisper include: ${WHISPER_INCLUDE_DIR}")
message(STATUS "Whisper library: ${WHISPER_LIB_DIR}/whisper.lib")
message(STATUS "GGML libraries: ${GGML_LIB_DIR}/")
message(STATUS "Elasticsearch: ${ELASTICSEARCH_AVAILABLE}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=====================================")
message(STATUS "")
