cmake_minimum_required(VERSION 3.15)
project(PerceptionEngine VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================
# Dependencies
# ============================================================================

# whisper.cpp
set(WHISPER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/include")
set(WHISPER_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/build/Release")
set(GGML_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/out/build/windows-x64-release/ggml/src/Release")

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

# ============================================================================
# Source Files
# ============================================================================

set(PERCEPTION_ENGINE_SOURCES
    PerceptionEngine.cpp
    ContextCollector.cpp
    HttpServer.cpp
    WindowsAPIs.cpp
    WindowsService.cpp
    WindowEventMonitor.cpp
    AudioCaptureEngine.cpp
    AsyncWhisperQueue.cpp
    SileroVAD.cpp
    CameraVisionEngine.cpp
    FastVLMTokenizer.cpp
)

set(PERCEPTION_ENGINE_HEADERS
    ContextCollector.h
    HttpServer.h
    WindowsAPIs.h
    WindowsService.h
    WindowEventMonitor.h
    AudioCaptureEngine.h
    AsyncWhisperQueue.h
    SileroVAD.h
    CameraVisionEngine.h
    FastVLMTokenizer.h
)

# ============================================================================
# Executables
# ============================================================================

# Main PerceptionEngine service
add_executable(PerceptionEngine ${PERCEPTION_ENGINE_SOURCES} ${PERCEPTION_ENGINE_HEADERS})

# Audio test program
add_executable(test_audio test_audio.cpp AudioCaptureEngine.cpp AudioCaptureEngine.h AsyncWhisperQueue.cpp AsyncWhisperQueue.h SileroVAD.cpp SileroVAD.h)

# Vision encoder test program
add_executable(test_vision_encoder test_vision_encoder.cpp)

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

target_include_directories(test_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WHISPER_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/ggml/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_include_directories(test_vision_encoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(PerceptionEngine PRIVATE
    # whisper.cpp and ggml
    ${WHISPER_LIB_DIR}/whisper.lib
    ${GGML_LIB_DIR}/ggml.lib
    ${GGML_LIB_DIR}/ggml-base.lib
    ${GGML_LIB_DIR}/ggml-cpu.lib

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

target_link_libraries(test_audio PRIVATE
    # whisper.cpp and ggml
    ${WHISPER_LIB_DIR}/whisper.lib
    ${GGML_LIB_DIR}/ggml.lib
    ${GGML_LIB_DIR}/ggml-base.lib
    ${GGML_LIB_DIR}/ggml-cpu.lib

    # ONNX Runtime
    ${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib

    # Windows APIs
    ole32       # COM
    winmm       # Multimedia
)

target_link_libraries(test_vision_encoder PRIVATE
    # ONNX Runtime
    ${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib
)

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

    target_compile_options(test_audio PRIVATE
        /W3                 # Warning level 3
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable __cplusplus macro
        /EHsc               # Exception handling
    )

    # Add WinRT support
    target_compile_definitions(PerceptionEngine PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WINVER=0x0A00       # Windows 10
        _WIN32_WINNT=0x0A00
    )

    target_compile_definitions(test_audio PRIVATE
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
        ${CMAKE_CURRENT_SOURCE_DIR}/models/whisper/ggml-tiny.en.bin
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/whisper/ggml-tiny.en.bin

    # Copy Silero VAD model to bin/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/vad
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/vad/silero_vad.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/vad/silero_vad.onnx

    # Copy FastVLM models to bin/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/fastvlm/onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/vision_encoder_simplified.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/fastvlm/onnx/vision_encoder_simplified.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/embed_tokens_q4f16.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/fastvlm/onnx/embed_tokens_q4f16.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/decoder_model_merged_q4f16.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/fastvlm/onnx/decoder_model_merged_q4f16.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/vocab.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models/fastvlm/vocab.json

    # Copy ONNX Runtime DLL to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/onnxruntime.dll

    # Copy OpenCV DLL to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OpenCV_BIN_DIR}/opencv_world4100.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/opencv_world4100.dll

    # For MSVC multi-config: also copy to bin/Release/
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/whisper
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/vad
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/fastvlm/onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/whisper/ggml-tiny.en.bin
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/whisper/ggml-tiny.en.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/vad/silero_vad.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/vad/silero_vad.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/vision_encoder_simplified.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/fastvlm/onnx/vision_encoder_simplified.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/embed_tokens_q4f16.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/fastvlm/onnx/embed_tokens_q4f16.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/onnx/decoder_model_merged_q4f16.onnx
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/fastvlm/onnx/decoder_model_merged_q4f16.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/fastvlm/vocab.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models/fastvlm/vocab.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/onnxruntime.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OpenCV_BIN_DIR}/opencv_world4100.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/opencv_world4100.dll

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
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=====================================")
message(STATUS "")
