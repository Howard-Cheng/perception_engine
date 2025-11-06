cmake_minimum_required(VERSION 3.20)
project(MeetingAssistant VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform check
if(NOT WIN32)
    message(FATAL_ERROR "This project is Windows-only")
endif()

# Ensure x64 build
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project requires x64 build. Please configure with -A x64")
endif()

################################################################################
# MicrophoneMonitor Library (DLL)
################################################################################

add_library(MicrophoneMonitor SHARED
    # Headers
    include/MicrophoneMonitor/MicrophoneMonitor.h
    include/MicrophoneMonitor/MicrophoneMonitorDLL.h
    include/MicrophoneMonitor/Logger.h
    
    # Sources
    src/MicrophoneMonitor/MicrophoneMonitor.cpp
    src/MicrophoneMonitor/MicrophoneMonitor_AudioDetection.cpp
    src/MicrophoneMonitor/MicrophoneMonitorDLL.cpp
    src/MicrophoneMonitor/Logger.cpp
)

target_include_directories(MicrophoneMonitor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/MicrophoneMonitor>
        $<INSTALL_INTERFACE:include/MicrophoneMonitor>
)

target_compile_definitions(MicrophoneMonitor PRIVATE
    BUILDING_DLL
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(MicrophoneMonitor PRIVATE
    ole32.lib
    psapi.lib
)

if(MSVC)
    target_compile_options(MicrophoneMonitor PRIVATE
        /W3
        /std:c++17
    )
endif()

################################################################################
# MeetingAssistant Application (EXE)
################################################################################

# Define MSFTCore SDK path (use relative path from CMakeLists.txt location)
set(MSFTCORE_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third-party/MSFT")
set(MSFTCORE_LIB_PATH "${MSFTCORE_SDK_PATH}/lib")
set(MSFTCORE_INCLUDE_PATH "${MSFTCORE_SDK_PATH}/include")

# Check if MSFTCore SDK exists
if(NOT EXISTS "${MSFTCORE_LIB_PATH}/MSFTCore.lib")
    message(WARNING "MSFTCore.lib not found at: ${MSFTCORE_LIB_PATH}/MSFTCore.lib")
    message(WARNING "Please ensure MSFTCore SDK is placed in: ${MSFTCORE_SDK_PATH}")
endif()

add_executable(MeetingAssistant
    # Headers
    include/MeetingAssistant/MeetingStateMachine.h
    include/MeetingAssistant/NotificationService.h
    include/MeetingAssistant/PayAttentionBridge.h
    include/MeetingAssistant/NamedPipeServer.h
    
    # Sources
    src/MeetingAssistant/MeetingStateMachine.cpp
    src/MeetingAssistant/NotificationService.cpp
    src/MeetingAssistant/PayAttentionBridge.cpp
    src/MeetingAssistant/NamedPipeServer.cpp
    src/main.cpp
)

target_include_directories(MeetingAssistant PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MeetingAssistant
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MicrophoneMonitor
    ${MSFTCORE_INCLUDE_PATH}
)

target_link_libraries(MeetingAssistant PRIVATE
    MicrophoneMonitor
    windowsapp.lib  # For Windows Runtime (WinRT)
    ${MSFTCORE_LIB_PATH}/MSFTCore.lib  # Link MSFTCore SDK
)

target_compile_definitions(MeetingAssistant PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(MeetingAssistant PRIVATE
        /W3
        /std:c++17
    )
    
    set_target_properties(MeetingAssistant PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

################################################################################
# ConnectMeetingAssistant Library (DLL)
################################################################################

add_library(ConnectMeetingAssistant SHARED
    # Headers
    include/ConnectMeetingAssistant/ConnectMeetingAssistant.h
    
    # Sources
    src/ConnectMeetingAssistant/ConnectMeetingAssistant.cpp
)

target_include_directories(ConnectMeetingAssistant
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/ConnectMeetingAssistant>
        $<INSTALL_INTERFACE:include/ConnectMeetingAssistant>
)

target_compile_definitions(ConnectMeetingAssistant PRIVATE
    BUILDING_CONNECT_DLL
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(ConnectMeetingAssistant PRIVATE
        /W3
        /std:c++17
    )
endif()

################################################################################
# ConnectMeetingAssistantTest Application (EXE)
################################################################################

add_executable(ConnectMeetingAssistantTest
    examples/ConnectMeetingAssistantTest/main.cpp
)

target_include_directories(ConnectMeetingAssistantTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ConnectMeetingAssistant
)

target_link_libraries(ConnectMeetingAssistantTest PRIVATE
    ConnectMeetingAssistant
)

target_compile_definitions(ConnectMeetingAssistantTest PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(ConnectMeetingAssistantTest PRIVATE
        /W3
        /std:c++17
    )
    
    set_target_properties(ConnectMeetingAssistantTest PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

################################################################################
# Post-build: Copy DLL to executable directory
################################################################################

# Copy MicrophoneMonitor.dll to executable directory
add_custom_command(TARGET MeetingAssistant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:MicrophoneMonitor>
        $<TARGET_FILE_DIR:MeetingAssistant>
    COMMENT "Copying MicrophoneMonitor.dll to executable directory"
)

# Copy MSFTCore.dll and its dependencies to executable directory
add_custom_command(TARGET MeetingAssistant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${MSFTCORE_LIB_PATH}
        $<TARGET_FILE_DIR:MeetingAssistant>
    COMMENT "Copying MSFTCore.dll and dependencies to executable directory"
)

# Copy ConnectMeetingAssistant.dll to test application directory
add_custom_command(TARGET ConnectMeetingAssistantTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ConnectMeetingAssistant>
        $<TARGET_FILE_DIR:ConnectMeetingAssistantTest>
    COMMENT "Copying ConnectMeetingAssistant.dll to test application directory"
)

# Build completion message
add_custom_command(TARGET MeetingAssistant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "========================================================================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Build Complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================================================================"
    COMMAND ${CMAKE_COMMAND} -E echo "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Files generated:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - MeetingAssistant.exe (Main application with pipe server)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - MicrophoneMonitor.dll"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ConnectMeetingAssistant.dll (Client library)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - ConnectMeetingAssistantTest.exe (Test application)"
    COMMAND ${CMAKE_COMMAND} -E echo "  - MSFTCore.dll and all dependencies"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  1. Run MeetingAssistant.exe first (starts pipe server)"
    COMMAND ${CMAKE_COMMAND} -E echo "  2. Run ConnectMeetingAssistantTest.exe (connects to pipe)"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================================================================"
)

################################################################################
# Installation (optional)
################################################################################

install(TARGETS MicrophoneMonitor MeetingAssistant ConnectMeetingAssistant ConnectMeetingAssistantTest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
