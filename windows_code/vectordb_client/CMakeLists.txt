cmake_minimum_required(VERSION 3.15)
project(vectordb_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Find dependencies
find_package(nlohmann_json QUIET CONFIG)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found via find_package, trying pkg-config...")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(nlohmann_json QUIET nlohmann_json)
    endif()
endif()

if(NOT nlohmann_json_FOUND)
    message(WARNING 
        "nlohmann/json not found. JSON operations may not work.\n"
        "Please install nlohmann/json:\n"
        "  - Windows: vcpkg install nlohmann-json\n"
        "  - Ubuntu: sudo apt-get install nlohmann-json3-dev\n"
        "  - macOS:  brew install nlohmann-json\n"
    )
endif()

# Find ONNX Runtime (required for embedding model)
# Use local ONNX Runtime from windows_code/third-party
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/onnxruntime")
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")

# Check if local ONNX Runtime exists
if(EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    set(onnxruntime_FOUND TRUE)
    message(STATUS "ONNX Runtime found at: ${ONNXRUNTIME_DIR}")
    message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIR}")
    message(STATUS "  Library: ${ONNXRUNTIME_LIB_DIR}")
else()
    # Fallback: try to find via find_package
    find_package(onnxruntime QUIET)
    
    if(NOT onnxruntime_FOUND)
        message(FATAL_ERROR 
            "ONNX Runtime is required but not found!\n"
            "Expected location: ${ONNXRUNTIME_DIR}\n"
            "Please ensure ONNX Runtime is installed:\n"
            "  - Ensure ONNX Runtime is in windows_code/third-party/onnxruntime\n"
            "  - Or download from: https://onnxruntime.ai/"
        )
    endif()
endif()

# Find CURL (required for HTTP requests to Qdrant)
find_package(CURL REQUIRED)

if(NOT CURL_FOUND)
    message(FATAL_ERROR 
        "CURL is required for Qdrant HTTP client.\n"
        "Please install CURL:\n"
        "  - Windows: vcpkg install curl\n"
        "  - Ubuntu: sudo apt-get install libcurl4-openssl-dev\n"
        "  - macOS:  brew install curl"
    )
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(VECTORDB_SOURCES
    src/QdrantClient.cpp
    src/EmbeddingModel.cpp
    src/VectorStore.cpp
)

# Create library
add_library(vectordb_client SHARED ${VECTORDB_SOURCES})

# Define export macro
target_compile_definitions(vectordb_client PRIVATE 
    VECTORDB_EXPORTS
    _CRT_SECURE_NO_WARNINGS
)

# Set output directory to match the build system
set_target_properties(vectordb_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Link libraries
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(vectordb_client PRIVATE nlohmann_json::nlohmann_json)
elseif(nlohmann_json_FOUND)
    target_include_directories(vectordb_client PRIVATE ${nlohmann_json_INCLUDE_DIRS})
    target_link_libraries(vectordb_client PRIVATE ${nlohmann_json_LIBRARIES})
endif()

if(onnxruntime_FOUND)
    # Use local ONNX Runtime if available
    if(EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
        target_include_directories(vectordb_client PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_directories(vectordb_client PRIVATE ${ONNXRUNTIME_LIB_DIR})
        target_link_libraries(vectordb_client PRIVATE onnxruntime.lib)
        
        # Copy DLL to output directory on Windows
        if(WIN32)
            add_custom_command(TARGET vectordb_client POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
                    $<TARGET_FILE_DIR:vectordb_client>/onnxruntime.dll
                COMMENT "Copying ONNX Runtime DLL to output directory"
            )
        endif()
    else()
        # Use find_package result
        target_link_libraries(vectordb_client PRIVATE onnxruntime::onnxruntime)
    endif()
else()
    message(FATAL_ERROR "ONNX Runtime is required but not found!")
endif()

# Link CURL library (required for Qdrant HTTP client)
if(TARGET CURL::libcurl)
    target_link_libraries(vectordb_client PRIVATE CURL::libcurl)
elseif(CURL_FOUND)
    target_include_directories(vectordb_client PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(vectordb_client PRIVATE ${CURL_LIBRARIES})
else()
    message(FATAL_ERROR "CURL library not found but is required for Qdrant HTTP client")
endif()

# Link E5EmbeddingDLL (required for embedding model)
# The E5EmbeddingDLL should be built as part of the main project
if(TARGET E5EmbeddingDLL)
    target_link_libraries(vectordb_client PRIVATE E5EmbeddingDLL)
    target_include_directories(vectordb_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../embeddingmodel/include)
    message(STATUS "E5EmbeddingDLL: Found and linked")
else()
    message(WARNING "E5EmbeddingDLL target not found. EmbeddingModel may not work correctly.")
endif()

# Compiler flags
if(MSVC)
    target_compile_options(vectordb_client PRIVATE
        /W3
        /permissive-
        /EHsc
    )
else()
    target_compile_options(vectordb_client PRIVATE
        -Wall
        -Wextra
    )
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install
install(TARGETS vectordb_client
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration
message(STATUS "")
message(STATUS "=== vectordb_client Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "nlohmann/json: ${nlohmann_json_FOUND}")
message(STATUS "ONNX Runtime: ${onnxruntime_FOUND}")
message(STATUS "CURL: ${CURL_FOUND}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "==================================")
message(STATUS "")

