cmake_minimum_required(VERSION 3.15)
project(TestASR VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# ASR Library Configuration
# ============================================================================

set(ASR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/asr")
set(ASR_INCLUDE_DIR "${ASR_DIR}/include")
set(ASR_LIB_DIR "${ASR_DIR}/lib")

# Check if ASR library exists
if(NOT EXISTS "${ASR_INCLUDE_DIR}/MSFTCore.h")
    message(FATAL_ERROR "MSFTCore.h not found at ${ASR_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${ASR_LIB_DIR}/MSFTCore.lib")
    message(FATAL_ERROR "MSFTCore.lib not found at ${ASR_LIB_DIR}")
endif()

message(STATUS "ASR Library found at: ${ASR_DIR}")

# ============================================================================
# Source Files
# ============================================================================

set(TEST_ASR_SOURCES
    src/TestASRMain.cpp
)

# ============================================================================
# Executable
# ============================================================================

add_executable(TestASR ${TEST_ASR_SOURCES})

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(TestASR PRIVATE
    ${ASR_INCLUDE_DIR}
)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(TestASR PRIVATE
    ${ASR_LIB_DIR}/MSFTCore.lib
    # Windows APIs
    ws2_32      # Winsock
    ole32       # COM
)

# ============================================================================
# Compiler Flags
# ============================================================================

if(MSVC)
    target_compile_options(TestASR PRIVATE
        /W3                 # Warning level 3
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable __cplusplus macro
        /EHsc               # Exception handling
    )

    target_compile_definitions(TestASR PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
        _UNICODE
    )
endif()

# ============================================================================
# Post-build: Copy DLL to output directory
# ============================================================================

add_custom_command(TARGET TestASR POST_BUILD
    # Copy MSFTCore.dll to bin/
    COMMAND ${CMAKE_COMMAND} -E echo "Copying MSFTCore.dll to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ASR_LIB_DIR}/MSFTCore.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MSFTCore.dll

    # For MSVC multi-config: also copy to bin/$<CONFIG>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ASR_LIB_DIR}/MSFTCore.dll
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/MSFTCore.dll

    COMMENT "Copying MSFTCore.dll for TestASR"
)

# ============================================================================
# Installation (optional)
# ============================================================================

install(TARGETS TestASR DESTINATION bin)
install(FILES ${ASR_LIB_DIR}/MSFTCore.dll DESTINATION bin)
