cmake_minimum_required(VERSION 3.10)
project(link_lingua)

# link_lingua static library
set(LINK_LINGUA_SOURCES
    LinkLingua.cpp
    LinguaClientImpl.h
)

set(LINK_LINGUA_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/link_lingua/LinkLingua.h
)

add_library(link_lingua STATIC ${LINK_LINGUA_SOURCES} ${LINK_LINGUA_HEADERS})

# Include directories
target_include_directories(link_lingua PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# C++17 standard
target_compile_features(link_lingua PUBLIC cxx_std_17)

if(MSVC)
    target_compile_definitions(link_lingua PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(link_lingua PRIVATE
        /W3
        /permissive-
        /Zc:__cplusplus
    )
endif()

# Visual Studio source groups
source_group("Source Files" FILES ${LINK_LINGUA_SOURCES})
source_group("Header Files" FILES ${LINK_LINGUA_HEADERS})
