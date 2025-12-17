# VSCode 调试指南

本指南帮助您在 VSCode 中调试 C++ 版本的 LLM Summary 模块。

## 📋 前置要求

### 必需的 VSCode 扩展

1. **C/C++** (ms-vscode.cpptools) - 必需
2. **CMake Tools** (ms-vscode.cmake-tools) - 强烈推荐
3. **C/C++ Extension Pack** (ms-vscode.cpptools-extension-pack) - 推荐

安装方法：
```
Ctrl+P → ext install ms-vscode.cpptools
Ctrl+P → ext install ms-vscode.cmake-tools
```

### 必需的开发工具

- ✅ Visual Studio 2022（包含 C++ 工具）
- ✅ CMake >= 3.20
- ✅ llama.cpp 已安装并构建
- ✅ 模型文件已下载

## 🚀 快速开始

### 1. 打开项目

在 VSCode 中打开 `llm_summary/cpp` 文件夹：

```powershell
cd d:\PerceiptionEngine_Howard\perception_engine\llm_summary\cpp
code .
```

### 2. 配置 CMake

首次打开项目时，VSCode 会提示配置 CMake：

1. 按 `Ctrl+Shift+P`
2. 输入 `CMake: Configure`
3. 选择编译器（推荐：Visual Studio 2022）

或者手动配置：

**方法 A：使用 CMake Tools 扩展**
- 点击底部状态栏的 `Configure` 按钮
- 选择 kit（编译器）
- 等待配置完成

**方法 B：使用终端**
```powershell
mkdir build
cd build
cmake .. -DUSE_SQLITE=ON -DBUILD_EXAMPLES=ON
```

### 3. 构建项目

**方法 A：使用快捷键**
```
Ctrl+Shift+B  # 打开构建任务列表
选择 "Build C++ Project"
```

**方法 B：使用 CMake Tools**
- 点击底部状态栏的 `Build` 按钮
- 或按 `F7`

**方法 C：使用终端**
```powershell
cmake --build build --config Release
```

### 4. 开始调试

**方法 A：使用 F5**
1. 在代码中设置断点（点击行号左侧）
2. 按 `F5` 开始调试
3. 选择调试配置：`(Windows) Launch Example`

**方法 B：使用调试面板**
1. 点击左侧调试图标（或 `Ctrl+Shift+D`）
2. 选择调试配置
3. 点击绿色播放按钮

## 🔧 调试配置说明

### 可用的调试配置

#### 1. (Windows) Launch Example
- **适用于**：Release 版本调试
- **调试器**：MSVC (cppvsdbg)
- **可执行文件**：`build/Release/example_usage.exe`
- **优点**：性能好，接近实际运行
- **缺点**：优化可能影响调试体验

#### 2. (Windows Debug) Launch Example
- **适用于**：Debug 版本调试
- **调试器**：MSVC (cppvsdbg)
- **可执行文件**：`build/Debug/example_usage.exe`
- **优点**：完整调试信息，无优化
- **推荐**：日常开发调试使用

#### 3. (GDB) Launch Example
- **适用于**：使用 GDB 调试
- **调试器**：GDB (cppdbg)
- **需要**：安装 MinGW 或 MSYS2
- **适用场景**：跨平台开发

## 🎯 调试技巧

### 设置断点

1. **普通断点**：点击行号左侧
2. **条件断点**：右键断点 → 编辑断点 → 添加条件
   ```cpp
   i == 5  // 当 i 等于 5 时中断
   ```
3. **日志点**：右键断点 → 添加日志点
   ```
   Value is {variable_name}
   ```

### 监视变量

1. **方法 A**：鼠标悬停在变量上
2. **方法 B**：右键变量 → "添加到监视"
3. **方法 C**：在调试窗口的"监视"面板手动添加

### 调用堆栈

- 在调试面板的"调用堆栈"中查看函数调用链
- 点击堆栈帧可以跳转到对应代码

### 调试控制台

调试时可以在"调试控制台"中：
- 输入变量名查看值
- 执行表达式：`-exec print variable_name`
- 调用函数：`-exec call function()`

## 📝 常见调试场景

### 场景 1：调试文本生成

```cpp
// 在 LLMClient.cpp 的 generate 函数中设置断点
std::string LLMClient::generate(
    const std::string& prompt,
    float temperature,
    int max_tokens
) {
    // 断点：检查输入参数
    if (!model_loaded_) {
        loadModel();  // 断点：检查模型加载
    }
    
    // 断点：检查 token 生成
    for (int i = 0; i < max_tokens; ++i) {
        // ...
    }
}
```

**调试步骤**：
1. 在 `generate` 函数入口设置断点
2. 按 `F5` 启动调试
3. 程序在断点处暂停
4. 按 `F10`（单步跳过）或 `F11`（单步进入）
5. 在监视窗口添加 `prompt`, `temperature`

### 场景 2：调试数据库读取

```cpp
// 在 readFromSQLite 中设置断点
std::vector<DatabaseRecord> LLMClient::readFromSQLite(...) {
    // 断点：检查数据库路径
    sqlite3* db = nullptr;
    if (sqlite3_open(...) != SQLITE_OK) {  // 断点
        // 检查错误
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {  // 断点：检查每一行
        DatabaseRecord record;
        // 断点：检查字段解析
    }
}
```

### 场景 3：调试内存问题

使用内存监视：
1. 在"监视"窗口添加：`&variable_name`
2. 查看指针地址
3. 监视内存分配和释放

## 🛠️ 构建任务说明

可用的构建任务（`Ctrl+Shift+B`）：

1. **Build C++ Project** - 构建 Release 版本
2. **Build C++ Project (Debug)** - 构建 Debug 版本
3. **Configure CMake** - 配置 CMake（Release）
4. **Configure CMake (Debug)** - 配置 CMake（Debug）
5. **Clean Build** - 清理构建目录
6. **Run Example** - 构建并运行示例

## 🐛 常见问题

### 问题 1：找不到 llama.cpp

**症状**：
```
CMake Error: llama.cpp not found
```

**解决方法**：
1. 确保 llama.cpp 已安装
2. 在 `CMakeLists.txt` 中设置 `LLAMA_CPP_DIR`：
   ```cmake
   set(LLAMA_CPP_DIR "C:/path/to/llama.cpp/build")
   ```
3. 或在 CMake 配置时指定：
   ```powershell
   cmake .. -DLLAMA_CPP_DIR=C:/path/to/llama.cpp/build
   ```

### 问题 2：模型文件未找到

**症状**：
```
Model file not found: /path/to/model.gguf
```

**解决方法**：
1. 检查模型路径：修改 `LLMClient.cpp` 中的 `getDefaultModelPath()`
2. 或在代码中指定自定义路径：
   ```cpp
   LLMConfig config;
   config.model_path = "D:/models/your-model.gguf";
   LLMClient client(config);
   ```

### 问题 3：断点不生效

**可能原因**：
- 使用 Release 版本（优化影响调试）
- 调试信息未生成

**解决方法**：
1. 使用 Debug 配置：
   ```powershell
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build . --config Debug
   ```
2. 在 VSCode 中选择 `(Windows Debug) Launch Example` 配置

### 问题 4：IntelliSense 不工作

**解决方法**：
1. 安装 C/C++ 扩展
2. 重新配置 CMake：`Ctrl+Shift+P` → `CMake: Configure`
3. 重新加载窗口：`Ctrl+Shift+P` → `Developer: Reload Window`
4. 检查 `.vscode/c_cpp_properties.json` 中的路径

### 问题 5：编译错误

**常见错误**：

**错误 A**：找不到头文件
```cpp
fatal error: llama.h: No such file or directory
```
**解决**：检查 `c_cpp_properties.json` 中的 `includePath`

**错误 B**：链接错误
```
unresolved external symbol llama_init
```
**解决**：确保 llama.cpp 库正确链接，检查 `CMakeLists.txt`

## 📊 性能分析

### 使用 Visual Studio Profiler

1. 构建 Release 版本
2. 在 VSCode 终端运行：
   ```powershell
   vsperfcmd /start:trace /output:profile.vsp
   .\build\Release\example_usage.exe
   vsperfcmd /stop
   ```
3. 在 Visual Studio 中打开 `profile.vsp`

### 使用内置时间测量

在代码中添加：
```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
// 你的代码
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Time: " << duration.count() << "ms" << std::endl;
```

## 💡 高级技巧

### 1. 多线程调试

查看所有线程：
- 调试面板 → "调用堆栈" → 查看所有线程
- 切换线程：点击不同的线程

### 2. 条件断点示例

```cpp
// 只在特殊条件下中断
i > 100 && temperature > 0.8f

// 字符串匹配
prompt.find("error") != std::string::npos

// 指针检查
ptr != nullptr && ptr->value == 42
```

### 3. 数据可视化

在"监视"窗口使用格式化：
```
array,10          // 查看数组前 10 个元素
*ptr,20           // 查看指针指向的 20 个元素
str.c_str(),s     // 字符串视图
```

### 4. 反汇编视图

调试时：
1. 右键代码 → "打开反汇编"
2. 或 `Ctrl+Shift+P` → "Debug: Open Disassembly View"

## 📚 推荐资源

- [VSCode C++ 调试文档](https://code.visualstudio.com/docs/cpp/cpp-debug)
- [CMake Tools 文档](https://github.com/microsoft/vscode-cmake-tools)
- [llama.cpp 文档](https://github.com/ggerganov/llama.cpp)

## 🎓 学习路径

1. **初学者**：
   - 设置简单断点
   - 单步执行代码
   - 查看变量值

2. **中级**：
   - 使用条件断点
   - 监视表达式
   - 调用堆栈分析

3. **高级**：
   - 多线程调试
   - 内存分析
   - 性能优化

## 🆘 获取帮助

如果遇到问题：
1. 查看 VSCode 输出面板的错误信息
2. 检查 CMake 输出
3. 查看调试控制台的详细信息
4. 参考本项目的 README.md

祝调试愉快！🎉
