# Quick Start Guide

## 1. Start Qdrant Server

### 1.1 Using Docker
```bash
docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant
```

This will start Qdrant server on:
- HTTP API: `http://localhost:6333`
- gRPC API: `localhost:6334`
- Web Dashboard: `http://localhost:6333/dashboard`

### 1.2 Verify Qdrant is running
Open your browser and visit `http://localhost:6333/dashboard` to access the Qdrant web UI.

## 2. Download the e5-small model

### 1.1 Install optimum-onnx
```bash
pip install optimum-onnx
```

### 1.2 Export model
```bash
cd perception_engine/models/e5-small
optimum-cli export onnx --model intfloat/multilingual-e5-small .
```

This will create `model.onnx` in the `models/e5-small/` folder.

## 3. Build Example Program

### 3.1 Prerequisites
- CMake 3.15+
- Visual Studio 2019+ (Windows) or GCC 7+ (Linux)
- vcpkg (for dependencies)

### 3.2 Build using script
```bash
cd perception_engine/vectordb/cpp_lib
.\build.bat
```

### 3.3 Build using CMake manually
```bash
cd perception_engine/vectordb/cpp_lib
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

The model file will be automatically copied to `build/bin/Release/models/e5-small/model.onnx` during build.

## 4. Run Example

### 4.1 Run the example
```bash
cd perception_engine/vectordb/cpp_lib/build/bin/Release
.\vectordb_example.exe
```

### 4.2 What it does
- Initializes VectorStore with E5-small model
- Stores 5 sample texts with embeddings
- Performs semantic search
- Shows search results with similarity scores

## 5. View Data in Qdrant Dashboard

After running the example, you can view your collections and data in the Qdrant web dashboard:
- Open `http://localhost:6333/dashboard` in your browser
- Browse collections, view points, and inspect metadata

## 6. Usage in Your Code

```cpp
#include "VectorStore.h"
#include <windows.h>
#include <filesystem>

// Get executable directory
std::filesystem::path getExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return std::filesystem::path(exePath).parent_path();
}

// Initialize (connect to Qdrant server)
std::filesystem::path exeDir = getExeDirectory();
auto qdrantConfig = QdrantClient::Config::remote("http://localhost:6333");
VectorStore store("my_collection", 
                  (exeDir / "models" / "e5-small" / "model.onnx").string(),
                  qdrantConfig);
store.initialize();

// Store text
store.storeText("Your text here", {{"key", "value"}});

// Search
auto results = store.search("query text", 10);
```

## Notes

- **Qdrant Server**: Make sure Qdrant is running before running the example. Use `docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant` to start it.
- **Web Dashboard**: Access `http://localhost:6333/dashboard` to view collections and data.
- **E5 Model Prefixes**: E5-small model requires text prefixes: `"query: "` for queries, `"passage: "` for documents. VectorStore automatically adds prefixes if missing.
- **Model File**: Model file is ~450MB, ensure sufficient disk space.