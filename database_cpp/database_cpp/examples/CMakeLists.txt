# Add examples subdirectory
if(BUILD_EXAMPLES)
    # Basic ingestion example
    add_executable(basic_ingestion_example
        basic_ingestion.cpp
    )
    
    target_link_libraries(basic_ingestion_example
        PRIVATE
            perception_db_core
    )
    
    # Set output directory
    set_target_properties(basic_ingestion_example
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # Install example
    install(TARGETS basic_ingestion_example
        RUNTIME DESTINATION examples
    )
    
    # Layer 1 integration test (only if DuckDB is enabled)
    if(BUILD_LAYER1_DUCKDB)
        add_executable(layer1_integration_test
            layer1_integration_test.cpp
        )
        
        target_link_libraries(layer1_integration_test
            PRIVATE
                perception_db_core
        )
        
        # Set output directory
        set_target_properties(layer1_integration_test
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # Install example
        install(TARGETS layer1_integration_test
            RUNTIME DESTINATION examples
        )
        
        message(STATUS "Building Layer 1 DuckDB integration test")
    endif()
    
    message(STATUS "Building examples")
endif()
