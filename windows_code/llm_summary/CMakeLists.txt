cmake_minimum_required(VERSION 3.20)
project(llm_summary_cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(USE_DUCKDB "Enable DuckDB support" OFF)
option(USE_SQLITE "Enable SQLite support" ON)
option(BUILD_EXAMPLES "Build example programs" ON)

# ============================================================================
# Dependencies
# ============================================================================

# Use pre-built llama.cpp from subdirectory
set(LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp")
set(LLAMA_CPP_INCLUDE_DIR "${LLAMA_CPP_DIR}/include")
set(GGML_INCLUDE_DIR "${LLAMA_CPP_DIR}/ggml/include")
set(LLAMA_COMMON_DIR "${LLAMA_CPP_DIR}/common")
set(LLAMA_CPP_BUILD_DIR "${LLAMA_CPP_DIR}/build")

# Find the llama library (handles both Debug and Release builds)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_library(LLAMA_CPP_LIBRARY
        NAMES llama
        PATHS 
            "${LLAMA_CPP_BUILD_DIR}/src/Debug"
            "${LLAMA_CPP_BUILD_DIR}/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(LLAMA_COMMON_LIBRARY
        NAMES common
        PATHS 
            "${LLAMA_CPP_BUILD_DIR}/common/Debug"
            "${LLAMA_CPP_BUILD_DIR}/common/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_LIBRARY
        NAMES ggml
        PATHS 
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Debug"
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_BASE_LIBRARY
        NAMES ggml-base
        PATHS 
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Debug"
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_CPU_LIBRARY
        NAMES ggml-cpu
        PATHS 
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Debug"
            "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
else()
    find_library(LLAMA_CPP_LIBRARY
        NAMES llama
        PATHS "${LLAMA_CPP_BUILD_DIR}/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(LLAMA_COMMON_LIBRARY
        NAMES common
        PATHS "${LLAMA_CPP_BUILD_DIR}/common/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_LIBRARY
        NAMES ggml
        PATHS "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_BASE_LIBRARY
        NAMES ggml-base
        PATHS "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(GGML_CPU_LIBRARY
        NAMES ggml-cpu
        PATHS "${LLAMA_CPP_BUILD_DIR}/ggml/src/Release"
        NO_DEFAULT_PATH
        REQUIRED
    )
endif()

if(LLAMA_CPP_LIBRARY AND LLAMA_COMMON_LIBRARY AND GGML_LIBRARY AND GGML_BASE_LIBRARY AND GGML_CPU_LIBRARY AND EXISTS "${LLAMA_CPP_INCLUDE_DIR}/llama.h")
    add_library(llama SHARED IMPORTED)
    set_target_properties(llama PROPERTIES
        IMPORTED_LOCATION "${LLAMA_CPP_BUILD_DIR}/bin/$<CONFIG>/llama.dll"
        IMPORTED_IMPLIB "${LLAMA_CPP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LLAMA_CPP_INCLUDE_DIR};${GGML_INCLUDE_DIR};${LLAMA_COMMON_DIR}"
    )
    
    add_library(ggml SHARED IMPORTED)
    set_target_properties(ggml PROPERTIES
        IMPORTED_LOCATION "${LLAMA_CPP_BUILD_DIR}/bin/$<CONFIG>/ggml.dll"
        IMPORTED_IMPLIB "${GGML_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GGML_INCLUDE_DIR}"
    )
    
    add_library(ggml_base SHARED IMPORTED)
    set_target_properties(ggml_base PROPERTIES
        IMPORTED_LOCATION "${LLAMA_CPP_BUILD_DIR}/bin/$<CONFIG>/ggml-base.dll"
        IMPORTED_IMPLIB "${GGML_BASE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GGML_INCLUDE_DIR}"
    )
    
    add_library(ggml_cpu SHARED IMPORTED)
    set_target_properties(ggml_cpu PROPERTIES
        IMPORTED_LOCATION "${LLAMA_CPP_BUILD_DIR}/bin/$<CONFIG>/ggml-cpu.dll"
        IMPORTED_IMPLIB "${GGML_CPU_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GGML_INCLUDE_DIR}"
    )
    
    add_library(llama_common STATIC IMPORTED)
    set_target_properties(llama_common PROPERTIES
        IMPORTED_LOCATION "${LLAMA_COMMON_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LLAMA_COMMON_DIR}"
        INTERFACE_LINK_LIBRARIES "ggml;ggml_base;ggml_cpu"
    )
    
    message(STATUS "Found pre-built llama.cpp: ${LLAMA_CPP_LIBRARY}")
    message(STATUS "Found pre-built llama common: ${LLAMA_COMMON_LIBRARY}")
    message(STATUS "Found pre-built ggml: ${GGML_LIBRARY}")
    message(STATUS "Found pre-built ggml-base: ${GGML_BASE_LIBRARY}")
    message(STATUS "Found pre-built ggml-cpu: ${GGML_CPU_LIBRARY}")
    message(STATUS "llama.cpp include dir: ${LLAMA_CPP_INCLUDE_DIR}")
    message(STATUS "ggml include dir: ${GGML_INCLUDE_DIR}")
    message(STATUS "llama common dir: ${LLAMA_COMMON_DIR}")
else()
    message(FATAL_ERROR "Pre-built llama.cpp not found in ${LLAMA_CPP_BUILD_DIR}. Please build llama.cpp first.")
endif()

# SQLite (usually system-provided)
if(USE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        add_compile_definitions(USE_SQLITE)
        message(STATUS "SQLite3 found: ${SQLite3_LIBRARIES}")
    else()
        message(WARNING "SQLite3 not found. Building without SQLite support.")
        set(USE_SQLITE OFF)
    endif()
endif()

# DuckDB
if(USE_DUCKDB)
    find_package(duckdb QUIET)
    if(NOT duckdb_FOUND)
        message(STATUS "DuckDB not found, will try pkg-config")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(DUCKDB REQUIRED duckdb)
    endif()
    add_compile_definitions(USE_DUCKDB)
endif()

# ============================================================================
# Library
# ============================================================================

add_library(llm_summary_cpp SHARED
    LLMClient.cpp
)

target_compile_definitions(llm_summary_cpp
    PRIVATE
        LLM_SUMMARY_EXPORTS
)

target_include_directories(llm_summary_cpp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(llm_summary_cpp
    PRIVATE
        llama_common
        llama
        ggml_base
        ggml_cpu
        ggml
)

if(USE_SQLITE)
    target_link_libraries(llm_summary_cpp PUBLIC SQLite::SQLite3)
endif()

if(USE_DUCKDB)
    target_link_libraries(llm_summary_cpp PUBLIC duckdb)
endif()

# ============================================================================
# Examples
# ============================================================================

if(BUILD_EXAMPLES)
    add_executable(llm_summary_example example_usage.cpp)
    target_link_libraries(llm_summary_example PRIVATE llm_summary_cpp)
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS llm_summary_cpp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES LLMClient.h
    DESTINATION include/perception
)

# ============================================================================
# Print Configuration
# ============================================================================

message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  SQLite support: ${USE_SQLITE}")
message(STATUS "  DuckDB support: ${USE_DUCKDB}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "")

