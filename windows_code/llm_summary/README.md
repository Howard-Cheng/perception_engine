# LLM Summary C++ Module

C++ implementation of the LLM Summary module using llama.cpp for local LLM inference.

## Features

- ✅ Load local GGUF format models
- ✅ Text generation and conversation
- ✅ Text summarization
- ✅ Q&A functionality
- ✅ Database reading (SQLite and optional DuckDB)
- ✅ GPU acceleration support
- ✅ Multi-threaded inference

## Dependencies

### Required

- **CMake** >= 3.20
- **C++17** compatible compiler
- **llama.cpp** - Main inference engine
  - Clone and build from: https://github.com/ggerganov/llama.cpp

### Optional

- **SQLite3** - For SQLite database support (usually system-provided)
- **DuckDB** - For DuckDB database support (optional)

## Building

### 1. Install llama.cpp

```bash
# Clone llama.cpp
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp

# Create build directory
mkdir build && cd build

# Windows with CUDA (without CURL)
cmake .. -DLLAMA_CUDA=ON -DLLAMA_CURL=OFF
cmake --build . --config Release

# Windows CPU only (without CURL)
cmake .. -DLLAMA_CURL=OFF
cmake --build . --config Release

# Linux/Mac with CUDA
cmake .. -DLLAMA_CUDA=ON -DLLAMA_CURL=OFF
cmake --build . --config Release

# Note: CURL is only needed for downloading models from URLs
# If you already have local model files, you can disable it with -DLLAMA_CURL=OFF
```

**Optional: Enable CURL support**

If you need to download models from URLs, install CURL:

**Windows (vcpkg):**
```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install CURL
.\vcpkg install curl:x64-windows

# Build llama.cpp with CURL
cd ..\llama.cpp\build
cmake .. -DLLAMA_CUDA=ON -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

**Linux:**
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install libcurl-devel

# Build llama.cpp
cmake .. -DLLAMA_CUDA=ON
cmake --build . --config Release
```

**macOS:**
```bash
brew install curl

cmake .. -DLLAMA_CUDA=OFF  # CUDA not available on macOS
cmake --build . --config Release
```

### 2. Build LLM Summary C++ Module

```bash
cd perception_engine/llm_summary/cpp

# Create build directory
mkdir build && cd build

# Configure
cmake .. \
    -DLLAMA_CPP_DIR=/path/to/llama.cpp/build \
    -DUSE_SQLITE=ON \
    -DUSE_DUCKDB=OFF \
    -DBUILD_EXAMPLES=ON

# Build
cmake --build . --config Release

# Run example
./example_usage
```

### Build Options

- `LLAMA_CPP_DIR` - Path to llama.cpp installation
- `USE_SQLITE` - Enable SQLite support (default: ON)
- `USE_DUCKDB` - Enable DuckDB support (default: OFF)
- `BUILD_EXAMPLES` - Build example programs (default: ON)

## Usage

### Basic Example

```cpp
#include "LLMClient.h"

using namespace perception;

int main() {
    // Create client with default configuration
    LLMClient client;
    
    // Generate text
    std::string response = client.generate(
        "What is artificial intelligence?",
        0.7f,  // temperature
        200    // max_tokens
    );
    
    std::cout << response << std::endl;
    
    return 0;
}
```

### Custom Configuration

```cpp
#include "LLMClient.h"

LLMConfig config;
config.model_path = "/path/to/your/model.gguf";
config.n_ctx = 4096;           // Larger context window
config.n_gpu_layers = 35;      // GPU acceleration
config.n_threads = 8;          // Number of CPU threads
config.verbose = true;

LLMClient client(config);
```

### Text Summarization

```cpp
LLMClient client;

std::string long_text = "Your long text here...";
std::string summary = client.summarize(long_text, 200);

std::cout << "Summary: " << summary << std::endl;
```

### Question Answering

```cpp
LLMClient client;

std::string context = "Machine learning is a subset of AI...";
std::string question = "What is machine learning?";

std::string answer = client.answerQuestion(question, context);
std::cout << "Answer: " << answer << std::endl;
```

### Multi-turn Chat

```cpp
LLMClient client;

std::vector<ChatMessage> messages = {
    {"user", "What is Python?"},
    {"assistant", "Python is a programming language."},
    {"user", "What are its advantages?"}
};

std::string response = client.chat(messages, 0.7f, 200);
std::cout << response << std::endl;
```

### Reading from Database

```cpp
LLMClient client;

// Read from SQLite
std::filesystem::path db_path = "path/to/database.db";
auto records = client.readFromDatabase(
    db_path,
    "table_name",  // optional
    10             // limit
);

for (const auto& record : records) {
    std::cout << "ID: " << record.id << "\n";
    std::cout << "Summary: " << record.summary << "\n";
}
```

### Processing Database with LLM

```cpp
LLMClient client;

std::filesystem::path db_path = "path/to/database.db";
auto records = client.processDatabaseContent(
    db_path,
    "summarize",  // operation
    5             // limit
);

for (const auto& record : records) {
    std::cout << "LLM Summary: " 
              << record.metadata.at("llm_summary") << "\n";
}
```

## API Reference

### LLMConfig Structure

```cpp
struct LLMConfig {
    std::filesystem::path model_path;  // Path to GGUF model
    int n_ctx = 2048;                  // Context window size
    int n_threads = -1;                // CPU threads (-1 = auto)
    int n_gpu_layers = 35;             // GPU layers (0 = CPU only)
    bool verbose = true;               // Verbose output
    
    // Sampling parameters
    float temperature = 0.7f;
    int max_tokens = 512;
    float top_p = 0.9f;
    int top_k = 40;
    float repeat_penalty = 1.1f;
};
```

### LLMClient Methods

#### Constructor
```cpp
LLMClient(const LLMConfig& config = LLMConfig{});
```

#### Text Generation
```cpp
std::string generate(
    const std::string& prompt,
    float temperature = 0.7f,
    int max_tokens = 512
);
```

#### Chat
```cpp
std::string chat(
    const std::vector<ChatMessage>& messages,
    float temperature = 0.7f,
    int max_tokens = 512
);
```

#### Summarization
```cpp
std::string summarize(
    const std::string& text,
    int max_tokens = 200
);
```

#### Question Answering
```cpp
std::string answerQuestion(
    const std::string& question,
    const std::optional<std::string>& context = std::nullopt
);
```

#### Database Operations
```cpp
std::vector<DatabaseRecord> readFromDatabase(
    const std::filesystem::path& database_path,
    const std::optional<std::string>& table_name = std::nullopt,
    const std::optional<int>& limit = std::nullopt
);

std::vector<DatabaseRecord> processDatabaseContent(
    const std::filesystem::path& database_path,
    const std::string& operation = "summarize",
    const std::optional<int>& limit = std::nullopt
);
```

## GPU Acceleration

### CUDA Support

To enable CUDA acceleration, build llama.cpp with CUDA support:

```bash
cmake .. -DLLAMA_CUDA=ON
```

Then configure LLM client:

```cpp
LLMConfig config;
config.n_gpu_layers = 35;  // Number of layers to offload to GPU
                           // -1 = all layers
                           //  0 = CPU only

LLMClient client(config);
```

## Performance Tips

1. **GPU Acceleration**: Use `n_gpu_layers > 0` for significant speedup
2. **Thread Count**: Set `n_threads` to match your CPU cores
3. **Context Size**: Larger `n_ctx` requires more memory
4. **Batch Processing**: Process multiple requests in a loop for efficiency

## Troubleshooting

### Model not found
```
Error: Model file not found: /path/to/model.gguf
```
- Ensure model file is downloaded
- Check model path configuration
- Use `getDefaultModelPath()` for default location

### llama.cpp not found
```
CMake Error: llama.cpp not found
```
- Install llama.cpp first
- Set `LLAMA_CPP_DIR` in CMake configuration

### Database errors
```
Error: SQLite support not compiled
```
- Rebuild with `-DUSE_SQLITE=ON`
- Ensure SQLite3 development libraries are installed

## Examples

Complete working examples are in [example_usage.cpp](example_usage.cpp).

Run the example:
```bash
cd build
./example_usage
```

## License

Same as parent project.

## Contributing

Contributions welcome! Please follow the existing code style and add tests for new features.
