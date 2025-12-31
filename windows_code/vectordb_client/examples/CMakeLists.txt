# Example executable
add_executable(vectordb_example
    example_usage.cpp
)

target_link_libraries(vectordb_example
    PRIVATE
        vectordb_client
)

# Set ONNX Runtime path from windows_code/third-party
set(ONNXRUNTIME_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/onnxruntime/lib")

# Copy ONNX Runtime DLL to output directory on Windows (if available)
if(WIN32 AND EXISTS "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
    add_custom_command(TARGET vectordb_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
            $<TARGET_FILE_DIR:vectordb_example>/onnxruntime.dll
        COMMENT "Copying ONNX Runtime DLL to output directory"
    )
    message(STATUS "ONNX Runtime DLL found: ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
else()
    message(WARNING "ONNX Runtime DLL not found at: ${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
endif()

# Copy E5-small model to output directory
# Model is located at: windows_code/models/e5-small/model.onnx
set(MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../models")

if(EXISTS "${MODELS_DIR}/e5-small/model.onnx")
    add_custom_command(TARGET vectordb_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:vectordb_example>/models/e5-small
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MODELS_DIR}/e5-small/model.onnx"
            $<TARGET_FILE_DIR:vectordb_example>/models/e5-small/model.onnx
        COMMENT "Copying E5-small model to output directory"
    )
    message(STATUS "E5-small model found: ${MODELS_DIR}/e5-small/model.onnx")
else()
    message(WARNING "E5-small model not found at: ${MODELS_DIR}/e5-small/model.onnx")
    message(WARNING "vectordb_example will fail at runtime without the model file")
endif()

# Set output directory
set_target_properties(vectordb_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install example
install(TARGETS vectordb_example
    RUNTIME DESTINATION examples
)

