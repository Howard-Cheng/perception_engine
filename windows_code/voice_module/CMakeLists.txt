# CMakeLists.txt
# Real-time Speech Recognition Module - CMake Build Configuration
#

cmake_minimum_required(VERSION 3.14)
project(SpeechRecognitionModule VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Microsoft Speech SDK path - using third-party directory
set(SPEECH_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party/microsoft_speech_sdk")

# Check SDK path
if(NOT EXISTS "${SPEECH_SDK_DIR}")
    message(FATAL_ERROR "Microsoft Speech SDK not found at ${SPEECH_SDK_DIR}!\n"
        "Please ensure the SDK is properly installed in the third-party directory.\n")
endif()

message(STATUS "Using Speech SDK at: ${SPEECH_SDK_DIR}")

# Set header file paths - third-party structure
set(SPEECH_SDK_INCLUDE 
    "${SPEECH_SDK_DIR}/include/cxx_api"
    "${SPEECH_SDK_DIR}/include/c_api"
)

# Set library and binary file paths
set(SPEECH_SDK_LIB_DIR "${SPEECH_SDK_DIR}/lib")
set(SPEECH_SDK_BIN_DIR "${SPEECH_SDK_DIR}/dlls")

# Verify DLL files exist
set(CORE_DLL "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.core.dll")
set(AUDIO_DLL "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.audio.sys.dll")

if(NOT EXISTS "${CORE_DLL}")
    message(WARNING "Core DLL not found: ${CORE_DLL}")
endif()

if(NOT EXISTS "${AUDIO_DLL}")
    message(WARNING "Audio extension DLL not found: ${AUDIO_DLL}")
    message(WARNING "TTS functionality may not work without this DLL")
endif()

# =============================================
# Build SpeechRecognitionModule as DLL
# =============================================

# DLL source files
set(DLL_SOURCES
    SpeechRecognition.cpp
    SpeechHelper.cpp
)

# DLL header files
set(DLL_HEADERS
    SpeechRecognition.h
    SpeechHelper.h
    SpeechRecognitionExport.h
)

# Create shared library (DLL)
add_library(${PROJECT_NAME} SHARED ${DLL_SOURCES} ${DLL_HEADERS})

# Define export macro when building the DLL
target_compile_definitions(${PROJECT_NAME} PRIVATE SPEECHRECOGNITION_EXPORTS)

# Include directories for DLL
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SPEECH_SDK_INCLUDE}
)

# Public include directories (for consumers of the DLL)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link library directories for DLL
target_link_directories(${PROJECT_NAME} PRIVATE
    ${SPEECH_SDK_LIB_DIR}
)

# Link Speech SDK libraries to DLL
target_link_libraries(${PROJECT_NAME} PRIVATE
    Microsoft.CognitiveServices.Speech.core
)

# Set DLL output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# =============================================
# Build Test Program
# =============================================

# Test program source files
set(TEST_SOURCES
    main.cpp
)

# Create test executable
add_executable(SpeechRecognitionTest ${TEST_SOURCES})

# Include directories for test program
target_include_directories(SpeechRecognitionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SPEECH_SDK_INCLUDE}
)

# Link the DLL to test program
target_link_libraries(SpeechRecognitionTest PRIVATE
    ${PROJECT_NAME}
)

# Set test program output directory
set_target_properties(SpeechRecognitionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# =============================================
# Windows specific settings - Copy DLLs
# =============================================
if(WIN32)
    # Copy all required DLLs to output directory
    set(REQUIRED_DLLS
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.core.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.audio.sys.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.codec.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.embedded.sr.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.embedded.sr.runtime.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.embedded.tts.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.kws.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.kws.ort.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.onnxruntime.dll"
        "${SPEECH_SDK_BIN_DIR}/Microsoft.CognitiveServices.Speech.extension.telemetry.dll"
    )
    
    foreach(DLL_FILE ${REQUIRED_DLLS})
        if(EXISTS "${DLL_FILE}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_FILE}"
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>
                COMMENT "Copying ${DLL_FILE}..."
            )
        else()
            message(WARNING "DLL not found: ${DLL_FILE}")
        endif()
    endforeach()
endif()

# Print configuration information
message(STATUS "Speech SDK Directory: ${SPEECH_SDK_DIR}")
message(STATUS "Speech SDK Include: ${SPEECH_SDK_INCLUDE}")
message(STATUS "Speech SDK Lib Directory: ${SPEECH_SDK_LIB_DIR}")
message(STATUS "Speech SDK Bin Directory: ${SPEECH_SDK_BIN_DIR}")
message(STATUS "DLL output: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "Test executable output: ${CMAKE_BINARY_DIR}/bin")
