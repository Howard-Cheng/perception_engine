# Elasticsearch C++ Client DLL

cmake_minimum_required(VERSION 3.15)
project(elasticsearch_client_dll VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_TESTS "Build test programs" ON)

# Find dependencies
find_package(CURL REQUIRED)

# Try to find nlohmann_json with CONFIG mode first
find_package(nlohmann_json CONFIG QUIET)

if(NOT nlohmann_json_FOUND)
    # Fallback: try to find nlohmann_json as header-only
    message(STATUS "nlohmann_json CONFIG not found, trying header-only mode")
    
    # Check vcpkg installed locations (both classic and manifest mode)
    set(VCPKG_SEARCH_PATHS "")
    
    # Manifest mode: vcpkg_installed/x64-windows
    if(EXISTS "${CMAKE_SOURCE_DIR}/../vcpkg_installed/x64-windows/include")
        list(APPEND VCPKG_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/../vcpkg_installed/x64-windows/include")
    endif()
    
    # Classic mode: VCPKG_ROOT/installed/x64-windows
    if(DEFINED ENV{VCPKG_ROOT})
        list(APPEND VCPKG_SEARCH_PATHS "$ENV{VCPKG_ROOT}/installed/x64-windows/include")
    endif()
    
    # Search for json.hpp
    set(NLOHMANN_JSON_FOUND FALSE)
    foreach(SEARCH_PATH ${VCPKG_SEARCH_PATHS})
        if(EXISTS "${SEARCH_PATH}/nlohmann/json.hpp")
            message(STATUS "Found nlohmann/json.hpp in: ${SEARCH_PATH}")
            set(NLOHMANN_JSON_INCLUDE_DIR "${SEARCH_PATH}")
            set(NLOHMANN_JSON_FOUND TRUE)
            break()
        endif()
    endforeach()
    
    if(NLOHMANN_JSON_FOUND)
        add_library(nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
        )
        # Create alias for consistency
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        message(STATUS "Using nlohmann_json as header-only library")
    else()
        message(FATAL_ERROR "nlohmann_json not found. Please run: vcpkg install (in manifest mode)")
    endif()
else()
    message(STATUS "Found nlohmann_json via CONFIG")
endif()

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

# Create shared library
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
    INTERFACE
        ES_CLIENT_IMPORTS
)

# Set output directory
set_target_properties(elasticsearch_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(elasticsearch_client PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Build tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS elasticsearch_client
    EXPORT elasticsearch_client-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${LIB_HEADERS}
    DESTINATION include/elasticsearch_client
)

install(EXPORT elasticsearch_client-targets
    FILE elasticsearch_client-config.cmake
    NAMESPACE elasticsearch_client::
    DESTINATION lib/cmake/elasticsearch_client
)
