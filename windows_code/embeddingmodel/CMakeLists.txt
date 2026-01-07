cmake_minimum_required(VERSION 3.15)
project(E5EmbeddingDLL VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================

# E5EmbeddingDLL Library

# ============================================================================

# Source files
set(E5_EMBEDDING_SOURCES
    src/E5EmbeddingDLL.cpp
    src/GemmaTokenizer.cpp
    src/DocumentChunker.cpp
)

set(E5_EMBEDDING_HEADERS
    include/E5EmbeddingDLL.h
    include/GemmaTokenizer.h
    include/DocumentChunker.h
)

# Create shared library
add_library(E5EmbeddingDLL SHARED 
    ${E5_EMBEDDING_SOURCES} 
    ${E5_EMBEDDING_HEADERS}
)

# Define E5EMBEDDING_EXPORTS to enable __declspec(dllexport)
target_compile_definitions(E5EmbeddingDLL PRIVATE E5EMBEDDING_EXPORTS)

# ============================================================================

# SentencePiece Integration (Static Library Only)

# ============================================================================

# Use local third-party static library
set(SENTENCEPIECE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/sentencepiece")

if(EXISTS "${SENTENCEPIECE_ROOT}/include/sentencepiece_processor.h")
    message(STATUS "Found local SentencePiece at: ${SENTENCEPIECE_ROOT}")
    
    # Add include directory
    include_directories(${SENTENCEPIECE_ROOT}/include)
    
    # Find static library
    find_library(SENTENCEPIECE_STATIC_LIB
        NAMES sentencepiece sentencepiece_static sentencepiece-static
        PATHS ${SENTENCEPIECE_ROOT}/lib
        NO_DEFAULT_PATH
    )
    
    if(SENTENCEPIECE_STATIC_LIB)
        message(STATUS "  ✓ Found static library: ${SENTENCEPIECE_STATIC_LIB}")
        message(STATUS "  → Using static linking (no DLL needed)")
        set(SENTENCEPIECE_LIBRARIES ${SENTENCEPIECE_STATIC_LIB})
        
    else()
        message(WARNING "  ✗ No SentencePiece library found in ${SENTENCEPIECE_ROOT}/lib")
        message(WARNING "  ✗ C++ tokenizer will be DISABLED at compile time")
        target_compile_definitions(E5EmbeddingDLL PRIVATE NO_CPP_TOKENIZER)
        set(SENTENCEPIECE_LIBRARIES "")
    endif()
    
else()
    message(WARNING "SentencePiece not found!")
    message(WARNING "Please run: powershell -File ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/build_sentencepiece.ps1")
    message(WARNING "C++ tokenizer will be DISABLED. Python fallback will be used.")
    target_compile_definitions(E5EmbeddingDLL PRIVATE NO_CPP_TOKENIZER)
endif()

# Include directories
target_include_directories(E5EmbeddingDLL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pe_base/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/onnxruntime/include
)

# Find nlohmann_json package
find_package(nlohmann_json CONFIG REQUIRED)

# Link libraries
target_link_libraries(E5EmbeddingDLL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/onnxruntime/lib/onnxruntime.lib
    pe_base
    nlohmann_json::nlohmann_json
)

# Link SentencePiece static library if available
if(DEFINED SENTENCEPIECE_LIBRARIES AND NOT SENTENCEPIECE_LIBRARIES STREQUAL "")
    target_link_libraries(E5EmbeddingDLL PRIVATE ${SENTENCEPIECE_LIBRARIES})
    message(STATUS "✓ SentencePiece will be linked (STATIC)")
else()
    message(STATUS "✗ Building WITHOUT SentencePiece (Python fallback only)")
endif()

# ============================================================================

# Tests (Optional)

# ============================================================================

option(BUILD_EMBEDDING_TESTS "Build embedding model tests" ON)

if(BUILD_EMBEDDING_TESTS)
    # Test executable - Document Comparison
    add_executable(test_document_comparison
        tests/test_document_comparison.cpp
    )
    
    target_include_directories(test_document_comparison PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pe_base/include
    )
    
    target_link_libraries(test_document_comparison PRIVATE
        E5EmbeddingDLL
        pe_base
    )
    
    # Copy test executable to bin directory
    set_target_properties(test_document_comparison PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Add dependency
    add_dependencies(test_document_comparison E5EmbeddingDLL)
    
    # Test executable - Elasticsearch Similarity Integration
    add_executable(test_elasticsearch_similarity
        tests/test_elasticsearch_similarity.cpp
    )
    
    target_include_directories(test_elasticsearch_similarity PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../database_client/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pe_base/include
    )
    
    target_link_libraries(test_elasticsearch_similarity PRIVATE
        E5EmbeddingDLL
        database_client
        pe_base
    )
    
    # Copy test executable to bin directory
    set_target_properties(test_elasticsearch_similarity PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Add dependencies
    add_dependencies(test_elasticsearch_similarity E5EmbeddingDLL database_client)
endif()

# ============================================================================

# Installation

# ============================================================================

install(TARGETS E5EmbeddingDLL
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/E5EmbeddingDLL.h
    DESTINATION include
)

# ============================================================================

# Print Configuration

# ============================================================================

message(STATUS "")
message(STATUS "=== E5EmbeddingDLL Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build tests: ${BUILD_EMBEDDING_TESTS}")
if(DEFINED SENTENCEPIECE_LIBRARIES AND NOT SENTENCEPIECE_LIBRARIES STREQUAL "")
    message(STATUS "SentencePiece: ENABLED (Static)")
else()
    message(STATUS "SentencePiece: DISABLED (Python fallback)")
endif()
message(STATUS "====================================")
message(STATUS "")
