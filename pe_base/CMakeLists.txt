cmake_minimum_required(VERSION 3.14)
project(pe_base)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent
include(FetchContent)

# Import Boost
FetchContent_Declare(
    boost_repo
    URL https://archives.boost.io/release/1.81.0/source/boost_1_81_0.7z
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(boost_repo)

# Define Boost::asio manually since header-only Boost from source 
# does not automatically create imported targets like find_package does.
if(NOT TARGET Boost::asio)
    add_library(Boost::asio INTERFACE IMPORTED)
    target_include_directories(Boost::asio INTERFACE ${boost_repo_SOURCE_DIR})
    # Add other Boost header dependencies if necessary
    target_compile_definitions(Boost::asio INTERFACE BOOST_ASIO_NO_DEPRECATED)
endif()

# Import spdlog
FetchContent_Declare(
    spdlog_repo
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.11.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(spdlog_repo)
if(NOT spdlog_repo_POPULATED)
    FetchContent_Populate(spdlog_repo)
    add_library(spdlog INTERFACE)
    target_include_directories(
        spdlog
        INTERFACE ${spdlog_repo_SOURCE_DIR}/include
    )
endif()

# Source files
set(PE_BASE_SOURCES
    src/logger.cpp
    src/logger/log_message_impl.cpp
    src/logger/log_writer_impl.cpp
    src/task_queue/task_queue.cpp
    src/task_queue/task_queue_boost.cpp
    src/thread_helper.cpp
    src/time_util.cpp
    src/transport/data_buffer.cpp
    src/windows_helper.cpp
)

# Add Library
add_library(pe_base SHARED ${PE_BASE_SOURCES})

# Include Directories
target_include_directories(pe_base
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile Definitions
target_compile_definitions(pe_base
    PRIVATE
        pe_base_EXPORTS
        pe_base_LIB_SHARED
        _WIN32_WINNT=0x0A00
        BOOST_ASIO_NO_DEPRECATED
)

# Link Libraries
target_link_libraries(pe_base
    PRIVATE
        Boost::asio
        spdlog
)

if(WIN32)
    target_link_libraries(pe_base PRIVATE winmm ws2_32)
endif()

# Testing
add_executable(pe_base_test test/main.cpp)
target_link_libraries(pe_base_test PRIVATE pe_base)

