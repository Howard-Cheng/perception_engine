cmake_minimum_required(VERSION 3.15)

# revise_database Tool
# ====================
# Utility to reset summarized flag in Elasticsearch

# Source files
set(REVISE_DATABASE_SOURCES
    ReviseDatabase.cpp
)

# Create executable
add_executable(revise_database ${REVISE_DATABASE_SOURCES})

# Include directories
target_include_directories(revise_database PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../database_client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/include
)

# Link libraries
target_link_libraries(revise_database PRIVATE
    # Database client (Elasticsearch)
    database_client
    
    # nlohmann_json for JSON handling
    nlohmann_json::nlohmann_json
    
    # Windows APIs
    ws2_32      # Winsock for networking
)

# Compiler flags
if(MSVC)
    target_compile_options(revise_database PRIVATE
        /W3                 # Warning level 3
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable __cplusplus macro
        /EHsc               # Exception handling
    )
    
    target_compile_definitions(revise_database PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Post-build: Copy dependencies
add_custom_command(TARGET revise_database POST_BUILD
    # Copy to bin directory (same as other executables)
    COMMAND ${CMAKE_COMMAND} -E echo "revise_database built successfully"
    
    COMMENT "revise_database - Reset Summarized Flag Tool"
)

# Install target (optional)
install(TARGETS revise_database
    RUNTIME DESTINATION bin
)
