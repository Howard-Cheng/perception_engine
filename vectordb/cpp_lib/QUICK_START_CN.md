# 快速开始指南

## 1. 启动 Qdrant 服务器

### 1.1 使用 Docker
先打开docker desktop，然后执行以下命令：
```bash
docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant
```

这将启动 Qdrant 服务器：
- HTTP API: `http://localhost:6333`
- gRPC API: `localhost:6334`
- Web 控制台: `http://localhost:6333/dashboard`

### 1.2 验证 Qdrant 是否运行
在浏览器中打开 `http://localhost:6333/dashboard` 访问 Qdrant Web UI。

## 2. 下载 e5-small 模型

### 1.1 安装 optimum-onnx
```bash
pip install optimum-onnx
```

### 1.2 导出模型
```bash
cd perception_engine/models/e5-small
optimum-cli export onnx --model intfloat/multilingual-e5-small .
```

这将在 `models/e5-small/` 文件夹中创建 `model.onnx`。

## 3. 构建示例程序

### 3.1 前置要求
- CMake 3.15+
- Visual Studio 2019+（Windows）或 GCC 7+（Linux）
- vcpkg（用于依赖项）

### 3.2 使用脚本构建
```bash
cd perception_engine/vectordb/cpp_lib
.\build.bat
```

### 3.3 手动使用 CMake 构建
```bash
cd perception_engine/vectordb/cpp_lib
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

模型文件将在构建过程中自动复制到 `build/bin/Release/models/e5-small/model.onnx`。

## 4. 运行示例

### 4.1 运行示例
```bash
cd perception_engine/vectordb/cpp_lib/build/bin/Release
.\vectordb_example.exe
```

### 4.2 功能说明
- 使用 E5-small 模型初始化 VectorStore
- 存储 5 个示例文本及其嵌入向量
- 执行语义搜索
- 显示搜索结果及相似度分数

## 5. 在 Qdrant 控制台中查看数据

运行示例后，您可以在 Qdrant Web 控制台中查看集合和数据：
- 在浏览器中打开 `http://localhost:6333/dashboard`
- 浏览集合、查看点和检查元数据

## 6. 在代码中使用

```cpp
#include "VectorStore.h"
#include <windows.h>
#include <filesystem>

// 获取可执行文件目录
std::filesystem::path getExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return std::filesystem::path(exePath).parent_path();
}

// 初始化（连接到 Qdrant 服务器）
std::filesystem::path exeDir = getExeDirectory();
auto qdrantConfig = QdrantClient::Config::remote("http://localhost:6333");
VectorStore store("my_collection", 
                  (exeDir / "models" / "e5-small" / "model.onnx").string(),
                  qdrantConfig);
store.initialize();

// 存储文本
store.storeText("Your text here", {{"key", "value"}});

// 搜索
auto results = store.search("query text", 10);
```

## 注意事项

- **Qdrant 服务器**：运行示例前确保 Qdrant 正在运行。使用 `docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant` 启动。
- **Web 控制台**：访问 `http://localhost:6333/dashboard` 查看集合和数据。
- **E5 模型前缀**：E5-small 模型需要文本前缀：查询使用 `"query: "`，文档使用 `"passage: "`。VectorStore 会自动添加前缀（如果缺失）。
- **模型文件**：模型文件约 450MB，请确保有足够的磁盘空间。

