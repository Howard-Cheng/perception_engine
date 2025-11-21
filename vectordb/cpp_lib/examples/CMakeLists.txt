# Example executable
add_executable(vectordb_example
    example_usage.cpp
)

target_link_libraries(vectordb_example
    PRIVATE
        vectordb_cpp
)

# Copy ONNX Runtime DLL to output directory on Windows (if available)
if(WIN32 AND onnxruntime_FOUND AND EXISTS "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
    add_custom_command(TARGET vectordb_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
            $<TARGET_FILE_DIR:vectordb_example>/onnxruntime.dll
        COMMENT "Copying ONNX Runtime DLL to output directory"
    )
endif()

# Copy E5-small model to output directory (same pattern as PerceptionEngine)
# Model is located at: perception_engine/models/e5-small/model.onnx
# Use CMAKE_SOURCE_DIR to get the project root (vectordb/cpp_lib), then go up to perception_engine
add_custom_command(TARGET vectordb_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:vectordb_example>/models/e5-small
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/../../models/e5-small/model.onnx
        $<TARGET_FILE_DIR:vectordb_example>/models/e5-small/model.onnx
    COMMENT "Copying E5-small model to output directory"
)

# Set output directory
set_target_properties(vectordb_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install example
install(TARGETS vectordb_example
    RUNTIME DESTINATION examples
)

