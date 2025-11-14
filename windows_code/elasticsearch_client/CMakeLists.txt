# Elasticsearch C++ Client DLL (for PerceptionEngine)

cmake_minimum_required(VERSION 3.15)
project(elasticsearch_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies (vcpkg manifest mode)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Library sources
set(LIB_SOURCES
    src/ElasticsearchClient.cpp
    src/ElasticsearchClientAPI.cpp
)

set(LIB_HEADERS
    include/ElasticsearchClient.h
    include/ElasticsearchClientAPI.h
    include/ElasticsearchTypes.h
)

# Create shared library (DLL)
add_library(elasticsearch_client SHARED
    ${LIB_SOURCES}
    ${LIB_HEADERS}
)

# Include directories
target_include_directories(elasticsearch_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(elasticsearch_client
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# Compiler definitions for DLL export
target_compile_definitions(elasticsearch_client
    PRIVATE
        ES_CLIENT_EXPORTS
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(elasticsearch_client PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        CURL_STATICLIB  # Use static curl if available
    )
endif()

# Set output directory (match PerceptionEngine output)
set_target_properties(elasticsearch_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Post-build: Copy CURL DLL to output directory (if using shared CURL)
if(WIN32)
    # Find CURL DLL in vcpkg_installed
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
    
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll")
        add_custom_command(TARGET elasticsearch_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll"
                ${CMAKE_BINARY_DIR}/bin/libcurl.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libcurl.dll
            COMMENT "Copying libcurl.dll to output directory"
        )
    endif()
    
    # Also copy zlib and SSL DLLs if they exist
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/zlib1.dll")
        add_custom_command(TARGET elasticsearch_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/zlib1.dll"
                ${CMAKE_BINARY_DIR}/bin/zlib1.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/zlib1.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/zlib1.dll
            COMMENT "Copying zlib1.dll to output directory"
        )
    endif()
    
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libcrypto-3-x64.dll")
        add_custom_command(TARGET elasticsearch_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcrypto-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/libcrypto-3-x64.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcrypto-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libcrypto-3-x64.dll
            COMMENT "Copying libcrypto-3-x64.dll to output directory"
        )
    endif()
    
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll")
        add_custom_command(TARGET elasticsearch_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/libssl-3-x64.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libssl-3-x64.dll
            COMMENT "Copying libssl-3-x64.dll to output directory"
        )
    endif()
endif()

message(STATUS "Elasticsearch Client DLL configured")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/bin/elasticsearch_client.dll")
