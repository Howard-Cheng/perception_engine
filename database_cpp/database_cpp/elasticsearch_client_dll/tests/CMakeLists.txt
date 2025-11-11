# tests/CMakeLists.txt

# Test executable
add_executable(es_client_test
    es_client_test.cpp
)

target_link_libraries(es_client_test
    PRIVATE
        elasticsearch_client
)

set_target_properties(es_client_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# C API test
add_executable(es_c_api_test
    es_c_api_test.cpp
)

target_link_libraries(es_c_api_test
    PRIVATE
        elasticsearch_client
)

set_target_properties(es_c_api_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Performance test
add_executable(es_performance_test
    es_performance_test.cpp
)

target_link_libraries(es_performance_test
    PRIVATE
        elasticsearch_client
)

set_target_properties(es_performance_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
