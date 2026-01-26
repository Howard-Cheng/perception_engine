# Force UTF-8 encoding for source files (for Chinese characters)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Database Client Library (for PerceptionEngine)

cmake_minimum_required(VERSION 3.15)
project(database_client VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies (vcpkg manifest mode)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

# Library sources
set(LIB_SOURCES
    src/ElasticsearchClient.cpp
    src/PostgreSQLClient.cpp
    src/MeiliSearchClient.cpp
    src/DatabaseClientFactory.cpp
    src/ElasticsearchClientAPI.cpp
)

set(LIB_HEADERS
    include/DatabaseTypes.h
    include/IDatabaseClient.h
    include/ElasticsearchClient.h
    include/PostgreSQLClient.h
    include/MeiliSearchClient.h
    include/DatabaseClientFactory.h
    include/ElasticsearchClientAPI.h
)

# Create shared library (DLL)
add_library(database_client SHARED
    ${LIB_SOURCES}
    ${LIB_HEADERS}
)

# Include directories
target_include_directories(database_client
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${PostgreSQL_INCLUDE_DIRS}
        # ADD: pe_base include directory for logger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../pe_base/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(database_client
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        ${PostgreSQL_LIBRARIES}
        # ADD: Link to pe_base library for logger
        pe_base
)

# Compiler definitions for DLL export
target_compile_definitions(database_client
    PRIVATE
        DB_CLIENT_EXPORTS
        ES_CLIENT_EXPORTS
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(database_client PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        CURL_STATICLIB
    )
endif()

# Set output directory
set_target_properties(database_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Copy db_structure.ini to build directory during configure
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/db_structure.ini"
    "${CMAKE_BINARY_DIR}/db_structure.ini"
    COPYONLY
)

# Post-build: Copy db_structure.ini to output directory
add_custom_command(TARGET database_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/db_structure.ini"
        ${CMAKE_BINARY_DIR}/bin/db_structure.ini
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/db_structure.ini"
        ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/db_structure.ini
    COMMENT "Copying db_structure.ini to output directory"
)

# Post-build: Copy CURL DLL to output directory
if(WIN32)
    set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows")
    
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll")
        add_custom_command(TARGET database_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll"
                ${CMAKE_BINARY_DIR}/bin/libcurl.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libcurl.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libcurl.dll
            COMMENT "Copying libcurl.dll to output directory"
        )
    endif()
    
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/zlib1.dll")
        add_custom_command(TARGET database_client POST_BUILD
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
        add_custom_command(TARGET database_client POST_BUILD
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
        add_custom_command(TARGET database_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/libssl-3-x64.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libssl-3-x64.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libssl-3-x64.dll
            COMMENT "Copying libssl-3-x64.dll to output directory"
        )
    endif()
    
    # Copy PostgreSQL DLL (libpq.dll)
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libpq.dll")
        add_custom_command(TARGET database_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libpq.dll"
                ${CMAKE_BINARY_DIR}/bin/libpq.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libpq.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libpq.dll
            COMMENT "Copying libpq.dll to output directory"
        )
    endif()
    
    # Copy libiconv (dependency of libpq)
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libiconv-2.dll")
        add_custom_command(TARGET database_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libiconv-2.dll"
                ${CMAKE_BINARY_DIR}/bin/libiconv-2.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libiconv-2.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libiconv-2.dll
            COMMENT "Copying libiconv-2.dll to output directory"
        )
    endif()
    
    # Copy libintl (dependency of libpq)
    if(EXISTS "${VCPKG_INSTALLED_DIR}/bin/libintl-8.dll")
        add_custom_command(TARGET database_client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libintl-8.dll"
                ${CMAKE_BINARY_DIR}/bin/libintl-8.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_INSTALLED_DIR}/bin/libintl-8.dll"
                ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/libintl-8.dll
            COMMENT "Copying libintl-8.dll to output directory"
        )
    endif()
endif()

message(STATUS "Database Client DLL configured")
message(STATUS "  Output: ${CMAKE_BINARY_DIR}/bin/database_client.dll")
message(STATUS "  Supported databases: Elasticsearch, PostgreSQL, MeiliSearch (MongoDB - planned)")
message(STATUS "  pe_base logger dependency: ENABLED")

# Optional: Build tests
option(BUILD_TESTS "Build test programs" ON)

if(BUILD_TESTS)
    # Elasticsearch client test
    add_executable(elasticsearch_client_test
        tests/elasticsearch_client_test.cpp
    )
    
    target_link_libraries(elasticsearch_client_test
        PRIVATE
            database_client
    )
    
    target_include_directories(elasticsearch_client_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(elasticsearch_client_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # PostgreSQL client test
    add_executable(postgresql_client_test
        tests/postgresql_client_test.cpp
    )
    
    target_link_libraries(postgresql_client_test
        PRIVATE
            database_client
    )
    
    target_include_directories(postgresql_client_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(postgresql_client_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # GetESDBData fuzzy matching test (NEW!)
    add_executable(test_getesdbdata_fuzzy
        tests/test_getesdbdata_fuzzy.cpp
    )
    
    target_link_libraries(test_getesdbdata_fuzzy
        PRIVATE
            database_client
    )
    
    target_include_directories(test_getesdbdata_fuzzy
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(test_getesdbdata_fuzzy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # MeiliSearch client test
    add_executable(meilisearch_client_test
        tests/meilisearch_client_test.cpp
    )
    
    target_link_libraries(meilisearch_client_test
        PRIVATE
            database_client
    )
    
    target_include_directories(meilisearch_client_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(meilisearch_client_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    message(STATUS "Building database client tests:")
    message(STATUS "  - Elasticsearch client test")
    message(STATUS "  - PostgreSQL client test")
    message(STATUS "  - GetESDBData fuzzy matching test (NEW!)")
    message(STATUS "  - MeiliSearch client test")
endif()

# Optional: Build examples
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    # Elasticsearch example
    add_executable(elasticsearch_example
        examples/elasticsearch_example.cpp
    )
    
    target_link_libraries(elasticsearch_example
        PRIVATE
            database_client
    )
    
    target_include_directories(elasticsearch_example
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(elasticsearch_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # PostgreSQL example
    add_executable(postgresql_example
        examples/postgresql_example.cpp
    )
    
    target_link_libraries(postgresql_example
        PRIVATE
            database_client
    )
    
    target_include_directories(postgresql_example
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(postgresql_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # MeiliSearch example
    add_executable(meilisearch_example
        examples/meilisearch_example.cpp
    )
    
    target_link_libraries(meilisearch_example
        PRIVATE
            database_client
    )
    
    target_include_directories(meilisearch_example
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    set_target_properties(meilisearch_example PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    message(STATUS "Building database client examples:")
    message(STATUS "  - Elasticsearch example")
    message(STATUS "  - PostgreSQL example")
    message(STATUS "  - MeiliSearch example")
endif()