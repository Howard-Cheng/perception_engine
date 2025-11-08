# Perception Engine Database C++ Implementation
# Layered database architecture for cross-device context collection

cmake_minimum_required(VERSION 3.15)
project(PerceptionEngineDatabase VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_COLLECTOR "Build data collector" ON)

# Find required packages
# Try to find SQLite3 - vcpkg uses unofficial-sqlite3
find_package(unofficial-sqlite3 CONFIG QUIET)

if(unofficial-sqlite3_FOUND)
    message(STATUS "Found unofficial-sqlite3 (vcpkg)")
    set(SQLite3_FOUND TRUE)
    set(SQLITE3_TARGET unofficial::sqlite3::sqlite3)
else()
    # Fallback to standard SQLite3
    find_package(SQLite3 QUIET)
    
    if(NOT SQLite3_FOUND)
        message(STATUS "SQLite3 not found via find_package, trying pkg-config...")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SQLite3 QUIET sqlite3)
        endif()
    endif()
    
    if(NOT SQLite3_FOUND)
        message(FATAL_ERROR 
            "SQLite3 is required but not found!\n"
            "Please install SQLite3:\n"
            "  - Windows: vcpkg install sqlite3\n"
            "            Or download from https://www.sqlite.org/download.html\n"
            "  - Ubuntu: sudo apt-get install libsqlite3-dev\n"
            "  - macOS:  brew install sqlite3\n"
            "Then re-run cmake with:\n"
            "  -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake\n"
            "  or -DCMAKE_PREFIX_PATH=<sqlite3_install_path>"
        )
    endif()
    
    # Setup SQLite3 target for non-vcpkg installations
    if(TARGET SQLite::SQLite3)
        set(SQLITE3_TARGET SQLite::SQLite3)
    else()
        # pkg-config or manual find
        include_directories(${SQLite3_INCLUDE_DIRS})
        set(SQLITE3_LIBRARIES ${SQLite3_LIBRARIES})
    endif()
endif()

# Find nlohmann/json
find_package(nlohmann_json QUIET CONFIG)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found via find_package, trying pkg-config...")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(nlohmann_json QUIET nlohmann_json)
    endif()
endif()

if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR 
        "nlohmann/json is required but not found!\n"
        "Please install nlohmann/json:\n"
        "  - Windows: vcpkg install nlohmann-json\n"
        "  - Ubuntu: sudo apt-get install nlohmann-json3-dev\n"
        "  - macOS:  brew install nlohmann-json\n"
    )
endif()

# Find OpenSSL (optional, for MD5)
find_package(OpenSSL QUIET)

if(NOT OpenSSL_FOUND)
    message(WARNING 
        "OpenSSL not found. MD5 hashing will use a fallback implementation.\n"
        "For better performance, install OpenSSL:\n"
        "  - Windows: vcpkg install openssl\n"
        "  - Ubuntu: sudo apt-get install libssl-dev\n"
        "  - macOS:  brew install openssl\n"
    )
    # Define a flag to use fallback MD5
    add_compile_definitions(NO_OPENSSL)
endif()

# Find CURL (optional, for data collector)
find_package(CURL QUIET)

if(NOT CURL_FOUND AND BUILD_COLLECTOR)
    message(WARNING 
        "CURL not found. Data collector will not be built.\n"
        "To build the data collector, install CURL:\n"
        "  - Windows: vcpkg install curl\n"
        "  - Ubuntu: sudo apt-get install libcurl4-openssl-dev\n"
        "  - macOS:  brew install curl\n"
    )
    set(BUILD_COLLECTOR OFF)
endif()

# Find DuckDB (optional, for Layer 1 compression)
message(STATUS "Searching for DuckDB...")
find_package(DuckDB QUIET CONFIG)

# If not found, try to directly include the exports file
if(NOT DuckDB_FOUND AND NOT TARGET duckdb AND NOT TARGET duckdb_static)
    # Try to find DuckDB in vcpkg installation paths
    # Priority order: project local paths first, then system paths
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/duckdb")
        set(DUCKDB_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/duckdb")
        message(STATUS "Trying vcpkg_installed path: ${DUCKDB_CMAKE_DIR}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_install/installed/x64-windows/share/duckdb")
        set(DUCKDB_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_install/installed/x64-windows/share/duckdb")
        message(STATUS "Trying vcpkg_install path: ${DUCKDB_CMAKE_DIR}")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(DUCKDB_CMAKE_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/share/duckdb")
        message(STATUS "Trying VCPKG_ROOT path: ${DUCKDB_CMAKE_DIR}")
    else()
        set(DUCKDB_CMAKE_DIR "C:/vcpkg/installed/x64-windows/share/duckdb")
        message(STATUS "Trying default C:/vcpkg path: ${DUCKDB_CMAKE_DIR}")
    endif()
    
    message(STATUS "Checking for DuckDBExports.cmake at: ${DUCKDB_CMAKE_DIR}/DuckDBExports.cmake")
    if(EXISTS "${DUCKDB_CMAKE_DIR}/DuckDBExports.cmake")
        message(STATUS "Manually including DuckDB exports from ${DUCKDB_CMAKE_DIR}")
        include("${DUCKDB_CMAKE_DIR}/DuckDBExports.cmake")
        set(DuckDB_FOUND TRUE)
    else()
        message(STATUS "DuckDBExports.cmake not found at expected location")
    endif()
endif()

# Check for targets
if(TARGET duckdb)
    message(STATUS "Target 'duckdb' exists")
    set(BUILD_LAYER1_DUCKDB ON)
elseif(TARGET duckdb_static)
    message(STATUS "Target 'duckdb_static' exists")
    set(BUILD_LAYER1_DUCKDB ON)
elseif(DuckDB_FOUND)
    message(STATUS "DuckDB package found")
    set(BUILD_LAYER1_DUCKDB ON)
else()
    set(BUILD_LAYER1_DUCKDB OFF)
endif()

if(BUILD_LAYER1_DUCKDB)
    message(STATUS "DuckDB found, Layer 1 DuckDB storage will be enabled")
else()
    message(WARNING 
        "DuckDB not found. Layer 1 DuckDB compression will be disabled.\n"
        "To enable DuckDB compression, install DuckDB:\n"
        "  - Windows: vcpkg install duckdb\n"
        "  - Ubuntu: Build from source or use package manager\n"
        "  - macOS:  brew install duckdb\n"
    )
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add nlohmann/json include directories
if(TARGET nlohmann_json::nlohmann_json)
    set(JSON_TARGET nlohmann_json::nlohmann_json)
else()
    include_directories(${nlohmann_json_INCLUDE_DIRS})
    set(JSON_LIBRARIES ${nlohmann_json_LIBRARIES})
endif()

# Source files - Layer 0
set(LAYER0_SOURCES
    src/layer0/DataIngestion.cpp
    src/layer0/SchemaManager.cpp
)

# Source files - Layer 1
set(LAYER1_SOURCES
    src/layer1/SessionDetector.cpp
    src/layer1/EngagementCalculator.cpp
    src/layer1/ContentExtractor.cpp
    src/layer1/ContentClassifier.cpp
)

# Add DuckDB implementation if available
if(BUILD_LAYER1_DUCKDB)
    list(APPEND LAYER1_SOURCES
        src/layer1/DuckDBManager.cpp
        src/layer1/CompressionPipeline.cpp
    )
    add_compile_definitions(DUCKDB_ENABLED)
    message(STATUS "Layer 1 DuckDB sources added to build")
endif()

# Source files - Layer 2 (TODO: implement)
set(LAYER2_SOURCES
    # src/layer2/WorkSessionAggregator.cpp
    # src/layer2/DaySessionAggregator.cpp
)

# Common sources
set(COMMON_SOURCES
    src/common/Types.cpp
    src/common/DatabaseConfig.cpp
    src/common/Logger.cpp
    src/common/Utils.cpp
)

# Core library
add_library(perception_db_core STATIC
    ${LAYER0_SOURCES}
    ${LAYER1_SOURCES}
    ${LAYER2_SOURCES}
    ${COMMON_SOURCES}
)

# Link libraries to core
target_link_libraries(perception_db_core
    PUBLIC
        $<$<BOOL:${SQLITE3_TARGET}>:${SQLITE3_TARGET}>
        $<$<BOOL:${SQLITE3_LIBRARIES}>:${SQLITE3_LIBRARIES}>
        $<$<BOOL:${JSON_TARGET}>:${JSON_TARGET}>
        $<$<BOOL:${JSON_LIBRARIES}>:${JSON_LIBRARIES}>
)

# Link OpenSSL if found
if(OpenSSL_FOUND)
    target_link_libraries(perception_db_core
        PUBLIC
            OpenSSL::Crypto
    )
endif()

# Link DuckDB if found
if(BUILD_LAYER1_DUCKDB)
    if(TARGET duckdb)
        target_link_libraries(perception_db_core PUBLIC duckdb)
        message(STATUS "Linking against duckdb target")
    elseif(TARGET duckdb_static)
        target_link_libraries(perception_db_core PUBLIC duckdb_static)
        message(STATUS "Linking against duckdb_static target")
    elseif(DuckDB_LIBRARIES)
        target_link_libraries(perception_db_core PUBLIC ${DuckDB_LIBRARIES})
        target_include_directories(perception_db_core PUBLIC ${DuckDB_INCLUDE_DIRS})
        message(STATUS "Linking against DuckDB libraries: ${DuckDB_LIBRARIES}")
    endif()
endif()

# Main executable - Database service
add_executable(perception_db_service
    src/main.cpp
)

target_link_libraries(perception_db_service
    PRIVATE
        perception_db_core
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests (TODO: implement)
if(BUILD_TESTS)
    # enable_testing()
    # add_subdirectory(tests)
    message(STATUS "Tests not yet implemented")
endif()

# Data Collector (optional, depends on CURL)
if(BUILD_COLLECTOR)
    if(NOT CURL_FOUND)
        message(WARNING 
            "CURL not found. Data collector will be disabled.\n"
            "To enable data collector, install CURL:\n"
            "  - Windows: vcpkg install curl (or it will auto-install from vcpkg.json)\n"
            "  - Ubuntu: sudo apt-get install libcurl4-openssl-dev\n"
            "  - macOS:  brew install curl\n"
        )
        message(STATUS "Data collector will NOT be built")
        set(BUILD_COLLECTOR OFF)
    else()
        message(STATUS "Data collector will be built")
        
        # Define the perception_data_collector target
        add_executable(perception_data_collector
            src/collector/main.cpp
            src/collector/DataCollector.cpp
        )

        target_link_libraries(perception_data_collector
            PRIVATE
                perception_db_core
                CURL::libcurl
        )
        
        # Install data collector
        install(TARGETS perception_data_collector
            RUNTIME DESTINATION bin
        )
    endif()
else()
    message(STATUS "Data collector will NOT be built (BUILD_COLLECTOR=OFF)")
endif()

# Install targets
install(TARGETS perception_db_service
    RUNTIME DESTINATION bin
)

install(TARGETS perception_db_core
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration summary
message(STATUS "")
message(STATUS "==================================================")
message(STATUS "Perception Engine Database C++ Configuration")
message(STATUS "==================================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "SQLite3 found: ${SQLite3_FOUND}")
message(STATUS "nlohmann/json found: ${nlohmann_json_FOUND}")
message(STATUS "OpenSSL found: ${OpenSSL_FOUND}")
message(STATUS "CURL found: ${CURL_FOUND}")
message(STATUS "DuckDB found: ${BUILD_LAYER1_DUCKDB}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "Build collector: ${BUILD_COLLECTOR}")
message(STATUS "Build Layer1 DuckDB: ${BUILD_LAYER1_DUCKDB}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "==================================================")
message(STATUS "")
