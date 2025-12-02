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
    ../src/config/ConfigManager.cpp
    ../src/utils/Logger.cpp
)

set(E5_EMBEDDING_HEADERS
    include/E5EmbeddingDLL.h
    ../include/config/ConfigManager.h
    ../include/utils/Logger.h
)

# Create shared library
add_library(E5EmbeddingDLL SHARED ${E5_EMBEDDING_SOURCES} ${E5_EMBEDDING_HEADERS})

# Include directories
target_include_directories(E5EmbeddingDLL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/onnxruntime/include
)

# Link libraries
target_link_libraries(E5EmbeddingDLL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/onnxruntime/lib/onnxruntime.lib
)

# Compiler definitions
target_compile_definitions(E5EmbeddingDLL PRIVATE
    E5EMBEDDING_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)

# Compiler flags (for MSVC)
if(MSVC)
    target_compile_options(E5EmbeddingDLL PRIVATE
        /W3
        /permissive-
        /Zc:__cplusplus
        /EHsc
    )
endif()

# ============================================================================
# Tests (Optional)
# ============================================================================

option(BUILD_EMBEDDING_TESTS "Build embedding model tests" ON)

if(BUILD_EMBEDDING_TESTS)
    # Test executable - Document Comparison
    add_executable(test_document_comparison
        tests/test_document_comparison.cpp
        ../src/config/ConfigManager.cpp
        ../src/utils/Logger.cpp
    )
    
    target_include_directories(test_document_comparison PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    target_link_libraries(test_document_comparison PRIVATE
        E5EmbeddingDLL
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
        ../src/config/ConfigManager.cpp
        ../src/utils/Logger.cpp
    )
    
    target_include_directories(test_elasticsearch_similarity PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../database_client/include
    )
    
    target_link_libraries(test_elasticsearch_similarity PRIVATE
        E5EmbeddingDLL
        database_client
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
message(STATUS "====================================")
message(STATUS "")
