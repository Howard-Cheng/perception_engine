cmake_minimum_required(VERSION 3.15)
project(PerceptionEngine VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find vcpkg packages
find_package(nlohmann_json CONFIG REQUIRED)

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

# Embedding Model (subdirectory)
# ============================================================================

# Add embeddingmodel as a subdirectory
add_subdirectory(embeddingmodel)

# ============================================================================

# PE Base (subdirectory)
# ============================================================================

# Add pe_base as a subdirectory
add_subdirectory(pe_base)

# ============================================================================

# LLM Summary Library (subdirectory)
# ============================================================================

# Add llm_summary as a subdirectory (copied to windows_code/llm_summary)
add_subdirectory(llm_summary)

# ============================================================================

# VectorDB Client Library (subdirectory)
# ============================================================================

# Add vectordb_client as a subdirectory (copied to windows_code/vectordb_client)
add_subdirectory(vectordb_client)

# ============================================================================

# Tools
# ============================================================================

# Add revise_database tool
add_subdirectory(tools/revise_database)

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

# Note: E5EmbeddingDLL sources are now in embeddingmodel/CMakeLists.txt

set(PERCEPTION_ENGINE_SOURCES
    src/core/PerceptionEngine.cpp
    src/core/WindowsService.cpp
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
)

set(DATAPROCESS_SERVER_HEADERS
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/TaskQueue.h
)

# LinguaCore source files
set(LINGUACORE_SOURCES
    src/linguacore/LinguaCoreMain.cpp
    src/linguacore/LinguaCore.cpp
)

set(LINGUACORE_HEADERS
    include/linguacore/LinguaCore.h
)

# ============================================================================

# Executables
# ============================================================================

# Note: E5EmbeddingDLL is now built from embeddingmodel subdirectory

# Main PerceptionEngine service
add_executable(PerceptionEngine ${PERCEPTION_ENGINE_SOURCES} ${PERCEPTION_ENGINE_HEADERS})

# DataProcessServer executable
add_executable(DataProcessServer ${DATAPROCESS_SERVER_SOURCES} ${DATAPROCESS_SERVER_HEADERS})

# LinguaCore executable
add_executable(LinguaCore ${LINGUACORE_SOURCES} ${LINGUACORE_HEADERS})

# ============================================================================
# Visual Studio Source Groups (organize files in Solution Explorer)
# ============================================================================

# ========== PerceptionEngine Source Groups ==========

# Source files for PerceptionEngine
source_group("Source Files\\core" FILES
    src/core/PerceptionEngine.cpp
    src/core/WindowsService.cpp
)

source_group("Source Files\\utils" FILES
)

source_group("Source Files\\config" FILES
)

source_group("Source Files\\embeddingmodel" FILES
    embeddingmodel/src/E5EmbeddingDLL.cpp
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
    src/providers/CameraContextProvider.cpp
    src/providers/AppActivityContextProvider.cpp
    src/providers/CompositeContextManager.cpp
)

source_group("Source Files\\dataprocess" FILES
    src/dataprocess/DataProcessClient.cpp
)

source_group("Source Files\\sessionmanager" FILES
    src/sessionmanager/SessionManager.cpp
)

# Header files for PerceptionEngine
source_group("Header Files\\core" FILES
    include/core/WindowsService.h
)

source_group("Header Files\\utils" FILES
)

source_group("Header Files\\config" FILES
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

# ========== DataProcessServer Source Groups ==========

# Source files for DataProcessServer
source_group("Source Files\\dataprocess" FILES
    src/dataprocess/DataProcessServerMain.cpp
    src/dataprocess/TaskQueue.cpp
)

# Header files for DataProcessServer
source_group("Header Files\\dataprocess" FILES
    include/dataprocess/DataProcessProtocol.h
    include/dataprocess/TaskQueue.h
)

# ========== LinguaCore Source Groups ==========

# Source files for LinguaCore
source_group("Source Files\\linguacore" FILES
    src/linguacore/LinguaCoreMain.cpp
    src/linguacore/LinguaCore.cpp
)

# Header files for LinguaCore
source_group("Header Files\\linguacore" FILES
    include/linguacore/LinguaCore.h
)

# ============================================================================

# Include Directories
# ============================================================================

target_include_directories(PerceptionEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/embeddingmodel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/pe_base/include
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
    ${CMAKE_CURRENT_SOURCE_DIR}/pe_base/include
)

target_include_directories(LinguaCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/pe_base/include
    ${CMAKE_CURRENT_SOURCE_DIR}/database_client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/llm_summary
    ${CMAKE_CURRENT_SOURCE_DIR}/vectordb_client/include
    ${JSON_INCLUDE_DIR}
)

# Note: test_embedding_comparison include directories are now in embeddingmodel/CMakeLists.txt

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

    # PE Base
    pe_base
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
    pe_base
)

# LinguaCore links to database_client, llm_summary, and vectordb_client
target_link_libraries(LinguaCore PRIVATE
    # PE Base (for logging)
    pe_base
    
    # Database client (PostgreSQL)
    database_client
    
    # LLM Summary library
    llm_summary_cpp
    
    # Vector DB library (Qdrant)
    vectordb_client
    
    # nlohmann_json for JSON handling
    nlohmann_json::nlohmann_json
    
    # Windows APIs
    ws2_32      # Winsock for networking
)

# Note: test_embedding_comparison link libraries are now in embeddingmodel/CMakeLists.txt

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

# Post-build: Copy dependencies for all targets
# ============================================================================

# Post-build commands for PerceptionEngine
add_custom_command(TARGET PerceptionEngine POST_BUILD
    # Copy config files to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.ini

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

    # Copy ONNX Runtime DLL to bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/onnxruntime.dll

    # Copy our E5EmbeddingDLL to bin/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying E5EmbeddingDLL..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:E5EmbeddingDLL>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/E5EmbeddingDLL.dll

    # Copy DataProcessServer.exe to bin/ (same directory as PerceptionEngine.exe)
    COMMAND ${CMAKE_COMMAND} -E echo "Copying DataProcessServer.exe to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DataProcessServer>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DataProcessServer.exe

    # For MSVC multi-config: also copy to bin/$<CONFIG>/}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/config.ini
    
    # Copy models folder to bin/$<CONFIG>/}
    COMMAND ${CMAKE_COMMAND} -E echo "Copying models folder to $<CONFIG> directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models
    
    # Copy scripts folder to bin/$<CONFIG>/}
    COMMAND ${CMAKE_COMMAND} -E echo "Copying scripts folder to $<CONFIG> directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/scripts

    # Copy E5EmbeddingDLL to bin/$<CONFIG>/}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:E5EmbeddingDLL>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/E5EmbeddingDLL.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/onnxruntime.dll

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/models
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/models

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/scripts

    COMMENT "Copying models, scripts, and DLLs for PerceptionEngine"
)

# Note: test_embedding_comparison post-build commands are now in embeddingmodel/CMakeLists.txt
