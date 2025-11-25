cmake_minimum_required(VERSION 3.15)
project(PerceptionEngine VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================================================

# Database Client DLL (subdirectory)
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

# E5 Embedding - We build it ourselves now
set(E5_EMBEDDING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/Embedding")
set(E5_EMBEDDING_INCLUDE_DIR "${E5_EMBEDDING_DIR}/include")
set(E5_EMBEDDING_LIB_DIR "${E5_EMBEDDING_DIR}/lib")
set(E5_EMBEDDING_BIN_DIR "${E5_EMBEDDING_DIR}/bin")

# We always build E5EmbeddingDLL ourselves
set(E5_EMBEDDING_AVAILABLE TRUE)
message(STATUS "E5 Embedding: Building from source")

# Database Client (built as subdirectory)
# The database_client target is now available from the subdirectory
set(ELASTICSEARCH_AVAILABLE TRUE)
message(STATUS "Database client enabled (built from subdirectory)")

# ============================================================================

# Source Files
# ============================================================================

# E5EmbeddingDLL source files
set(E5_EMBEDDING_DLL_SOURCES
    src/embeddingmodel/E5EmbeddingDLL.cpp
    src/config/ConfigManager.cpp        # NEW: Add ConfigManager
    src/utils/Logger.cpp                # NEW: Add Logger (ConfigManager depends on it)
)

set(E5_EMBEDDING_DLL_HEADERS
    include/embeddingmodel/E5EmbeddingDLL.h
    include/config/ConfigManager.h      # NEW: Add ConfigManager header
    include/utils/Logger.h              # NEW: Add Logger header
)

set(PERCEPTION_ENGINE_SOURCES
    src/core/PerceptionEngine.cpp
    src/core/WindowsService.cpp
    src/utils/Logger.cpp
    src/config/ConfigManager.cpp
    src/context/ContextCollector.cpp
    src/communication/HttpServer.cpp
    src/platform/WindowsAPIs.cpp
    src/platform/WindowEventMonitor.cpp
    src/platform/MouseTracker.cpp
    src/platform/BrowserContentExtractor.cpp
    src/audio/AudioCaptureEngine.cpp
    src/audio/AsyncWhisperQueue.cpp
    src/audio/SileroVAD.cpp
    src/providers/SystemContextProvider.cpp
    src/providers/VoiceContextProvider.cpp
    src/providers/CameraContextProvider.cpp
    src/providers/AppActivityContextProvider.cpp
    src/providers/CompositeContextManager.cpp
    src/dataprocess/DataProcessClient.cpp
    src/sessionmanager/SessionManager.cpp
)

set(PERCEPTION_ENGINE_HEADERS
    include/core/WindowsService.h
    include/utils/Logger.h
    include/config/ConfigManager.h
    include/context/ContextCollector.h
    include/communication/HttpServer.h
    include/platform/WindowsAPIs.h
    include/platform/WindowEventMonitor.h
    include/platform/MouseTracker.h
    include/platform/BrowserContentExtractor.h
    include/audio/AudioCaptureEngine.h
    include/audio/AsyncWhisperQueue.h
    include/audio/SileroVAD.h
    include/providers/IContextProvider.h
    include/providers/CompositeContextManager.h
    include/providers/SystemContextProvider.h
    include/providers/AppActivityContextProvider.h
    include/providers/VoiceContextProvider.h
    include/providers/CameraContextProvider.h
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/DataProcessClient.h
    include/dataprocess/TaskQueue.h
    include/sessionmanager/SessionManager.h
)

# DataProcessServer source files
set(DATAPROCESS_SERVER_SOURCES
    src/dataprocess/DataProcessServerMain.cpp
    src/dataprocess/TaskQueue.cpp
    src/utils/Logger.cpp
)

set(DATAPROCESS_SERVER_HEADERS
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/TaskQueue.h
    include/utils/Logger.h
)

# ============================================================================

# Executables
# ============================================================================

# E5EmbeddingDLL library
add_library(E5EmbeddingDLL SHARED ${E5_EMBEDDING_DLL_SOURCES} ${E5_EMBEDDING_DLL_HEADERS})

# E5EmbeddingDLL include directories
target_include_directories(E5EmbeddingDLL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/embeddingmodel
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# E5EmbeddingDLL link libraries
target_link_libraries(E5EmbeddingDLL PRIVATE
    ${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib
)

# E5EmbeddingDLL compiler definitions
target_compile_definitions(E5EmbeddingDLL PRIVATE
    E5EMBEDDING_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)

# E5EmbeddingDLL compiler flags (for MSVC)
if(MSVC)
    target_compile_options(E5EmbeddingDLL PRIVATE
        /W3
        /permissive-
        /Zc:__cplusplus
        /EHsc
    )
endif()

# Main PerceptionEngine service
add_executable(PerceptionEngine ${PERCEPTION_ENGINE_SOURCES} ${PERCEPTION_ENGINE_HEADERS})

# DataProcessServer executable
add_executable(DataProcessServer ${DATAPROCESS_SERVER_SOURCES} ${DATAPROCESS_SERVER_HEADERS})

# Add dependency: PerceptionEngine depends on E5EmbeddingDLL
add_dependencies(PerceptionEngine E5EmbeddingDLL)

# ============================================================================
# Visual Studio Source Groups (organize files in Solution Explorer)
# ============================================================================

# Group source files by directory structure
source_group("Source Files\\core" FILES
    src/core/PerceptionEngine.cpp
    src/core/WindowsService.cpp
)

source_group("Source Files\\utils" FILES
    src/utils/Logger.cpp
)

source_group("Source Files\\config" FILES
    src/config/ConfigManager.cpp
)

source_group("Source Files\\embeddingmodel" FILES
    src/embeddingmodel/E5EmbeddingDLL.cpp
)

source_group("Source Files\\context" FILES
    src/context/ContextCollector.cpp
)

source_group("Source Files\\communication" FILES
    src/communication/HttpServer.cpp
)

source_group("Source Files\\platform" FILES
    src/platform/WindowsAPIs.cpp
    src/platform/WindowEventMonitor.cpp
    src/platform/MouseTracker.cpp
    src/platform/BrowserContentExtractor.cpp
)

source_group("Source Files\\audio" FILES
    src/audio/AudioCaptureEngine.cpp
    src/audio/AsyncWhisperQueue.cpp
    src/audio/SileroVAD.cpp
)

source_group("Source Files\\providers" FILES
    src/providers/SystemContextProvider.cpp
    src/providers/VoiceContextProvider.cpp
    src.providers/CameraContextProvider.cpp
    src.providers/AppActivityContextProvider.cpp
    src.providers/CompositeContextManager.cpp
)

source_group("Source Files\\dataprocess" FILES
    src/dataprocess/DataProcessClient.cpp
)

source_group("Source Files\\sessionmanager" FILES
    src/sessionmanager/SessionManager.cpp
)
# Group header files by directory structure
source_group("Header Files\\core" FILES
    include/core/WindowsService.h
)

source_group("Header Files\\utils" FILES
    include/utils/Logger.h
)

source_group("Header Files\\config" FILES
    include/config/ConfigManager.h
)

source_group("Header Files\\context" FILES
    include/context/ContextCollector.h
)

source_group("Header Files\\communication" FILES
    include/communication/HttpServer.h
)

source_group("Header Files\\platform" FILES
    include/platform/WindowsAPIs.h
    include/platform/WindowEventMonitor.h
    include/platform/MouseTracker.h
    include/platform/BrowserContentExtractor.h
)

source_group("Header Files\\audio" FILES
    include/audio/AudioCaptureEngine.h
    include/audio/AsyncWhisperQueue.h
    include/audio/SileroVAD.h
)

source_group("Header Files\\providers" FILES
    include/providers/IContextProvider.h
    include/providers/CompositeContextManager.h
    include/providers/SystemContextProvider.h
    include/providers/AppActivityContextProvider.h
    include/providers/VoiceContextProvider.h
    include/providers/CameraContextProvider.h
)

source_group("Header Files\\dataprocess" FILES
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/DataProcessClient.h
    include/dataprocess/TaskQueue.h
)

source_group("Header Files\\sessionmanager" FILES
    include/sessionmanager/SessionManager.h
)

# DataProcessServer source groups
source_group("Source Files" FILES
    src/dataprocess/DataProcessServerMain.cpp
    src/dataprocess/TaskQueue.cpp
    src/utils/Logger.cpp
)

source_group("Header Files" FILES
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/TaskQueue.h
    include/utils/Logger.h
)

# ============================================================================

# Include Directories
# ============================================================================

target_include_directories(PerceptionEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/embeddingmodel
    ${WHISPER_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/whisper.cpp/ggml/include
    ${JSON_INCLUDE_DIR}
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Add E5 Embedding include directory
if(E5_EMBEDDING_AVAILABLE)
    target_include_directories(PerceptionEngine PRIVATE
        ${E5_EMBEDDING_INCLUDE_DIR}
    )
endif()

target_include_directories(DataProcessServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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

# Link E5 Embedding library
if(E5_EMBEDDING_AVAILABLE)
    target_link_libraries(PerceptionEngine PRIVATE
        E5EmbeddingDLL  # Link to our built DLL
    )
else()
    message(WARNING "E5 Embedding not available - some features may be disabled")
endif()

# DataProcessServer only needs basic Windows APIs
target_link_libraries(DataProcessServer PRIVATE
    # No external dependencies needed
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
    # Copy config files to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json

    # Copy models folder from windows_code to bin/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying models folder to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models

    # Copy scripts folder from windows_code to bin/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying scripts folder to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scripts

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

    # Copy our E5EmbeddingDLL to bin/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying E5EmbeddingDLL..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:E5EmbeddingDLL>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/E5EmbeddingDLL.dll

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

    # Copy DataProcessServer.exe to bin/ (same directory as PerceptionEngine.exe)
    COMMAND ${CMAKE_COMMAND} -E echo "Copying DataProcessServer.exe to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DataProcessServer>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DataProcessServer.exe

    # For MSVC multi-config: also copy to bin/Release/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/config.ini
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/config.json
    
    # Copy models folder to bin/$<CONFIG>/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying models folder to $<CONFIG> directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models
    
    # Copy scripts folder to bin/$<CONFIG>/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying scripts folder to $<CONFIG> directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/scripts
    
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

    # Copy E5EmbeddingDLL to bin/$<CONFIG>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:E5EmbeddingDLL>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/E5EmbeddingDLL.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/onnxruntime.dll

    COMMENT "Copying models, scripts, and DLLs to output directory"
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
message(STATUS "E5 Embedding: ${E5_EMBEDDING_AVAILABLE}")
message(STATUS "Elasticsearch: ${ELASTICSEARCH_AVAILABLE}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=====================================")
message(STATUS "")
