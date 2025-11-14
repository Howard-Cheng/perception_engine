# Perception Engine 数据库 C++ 实现架构文档

## 概述

这是一个高性能、分层的数据库架构，用于跨设备上下文收集和压缩。该系统将 Python 实现转换为 C++ 生产就绪系统。

## 📊 当前实现状态

| 组件 | 状态 | 完成度 | 说明 |
|------|------|--------|------|
| **Layer 0** | ✅ 已完成 | 100% | SQLite 原始事件存储 |
| **Layer 1** | 🔨 进行中 | 80% | 会话检测、分析完成，LLM 压缩待实现 |
| **Layer 2** | 📋 未开始 | 0% | 领域聚合层计划中 |
| **Layer 3** | 📋 未开始 | 0% | 向量搜索计划中 |
| **数据收集器** | ✅ 已完成 | 100% | perception_data_collector 已实现 |
| **通用组件** | ✅ 已完成 | 100% | Types, Config, Logger, Utils |

**图例**: ✅ 已完成 | 🔨 进行中 | 📋 未开始

### 可用功能

**✅ 当前可用**:
- 原始事件存储和检索（Layer 0）
- 会话边界检测
- 用户参与度计算
- 高注意力内容提取
- 内容类型分类
- **数据收集器（NEW!）** - 实时从 API 拉取数据
- 基础示例程序

**🚧 待完成**:
- LLM 内容压缩
- DuckDB 集成
- Layer 2 聚合
- 向量搜索

## 核心设计理念

### 分层架构 (Layered Architecture)

```
┌──────────────────────────────────────────────────────────┐
│              Perception Engine Database                   │
│                    C++ Service                            │
└──────────────────────────────────────────────────────────┘
                          |
        ┌─────────────────┼─────────────────┐
        │                 │                 │
   ┌────▼────┐       ┌────▼────┐      ┌────▼────┐
   │ Layer 0 │       │ Layer 1 │      │ Layer 2 │
   │ SQLite  │──────?│ DuckDB  │─────?│ DuckDB  │
   │         │       │         │      │         │
   │ 原始    │       │ 压缩    │      │ 工作/   │
   │ 事件    │       │ 上下文  │      │ 日会话  │
   └─────────┘       └─────────┘      └─────────┘
   24小时缓冲        7天保留           30天保留
   2-5GB/天         200-500MB/天       聚合数据
```

### 三层存储系统

#### Layer 0 - 原始事件存储 (SQLite)
- **目的**: 高频写入缓冲区
- **保留期**: 24小时
- **存储量**: 2-5GB/天/设备
- **技术**: SQLite (嵌入式，ACID 事务)

**存储内容**:
- 完整屏幕内容
- 所有鼠标/键盘交互
- 音频转录（如果可用）
- 相机描述（如果可用）
- 系统指标（电池、CPU、网络、位置）

#### Layer 1 - 压缩上下文 (DuckDB)
- **目的**: 高效分析存储
- **保留期**: 7天
- **存储量**: 200-500MB/天 (90% 压缩率)
- **技术**: DuckDB (列式存储，OLAP 优化)

**存储内容**:
- 会话元数据（开始/结束时间、参与度、分类）
- 压缩内容（LLM 总结，约 10% 原始大小）
- 高注意力内容（复制文本、选中文本）
- 提取的实体（数字、日期、URL、邮箱）
- 结构化元数据（特定内容类型）

#### Layer 2 - 领域聚合 (DuckDB)
- **目的**: 高级摘要
- **保留期**: 30天
- **存储量**: 聚合数据
- **技术**: DuckDB (同一数据库)

**存储内容**:
- 工作会话（项目级聚合）
- 日会话（每日回顾）
- 跨会话实体追踪
- 项目推断

## 代码组织结构

### 目录结构

```
database_cpp/
├── CMakeLists.txt          # 构建配置 ✅
├── README.md               # 项目说明 ✅
├── ARCHITECTURE_CN.md      # 本文档 ✅
├── PROJECT_STATUS.md       # 实现状态和路线图 ✅
│
├── include/                    # 公共头文件
│   ├── common/                # 通用组件 ✅
│   │   ├── Types.h           # 核心类型定义
│   │   ├── DatabaseConfig.h  # 配置结构
│   │   ├── Logger.h          # 日志工具
│   │   └── Utils.h           # 辅助函数
│   │
│   ├── layer0/               # Layer 0 - 原始事件 ✅
│   │   ├── DataIngestion.h  # 事件摄取
│   │   └── SchemaManager.h  # 模式初始化
│   │
│   ├── layer1/               # Layer 1 - 压缩 ✅
│   │   ├── SessionDetector.h        # 会话检测
│   │   ├── EngagementCalculator.h   # 参与度计算
│   │   ├── ContentExtractor.h       # 内容提取
│   │   ├── ContentClassifier.h      # 内容分类
│   │   └── CompressionPipeline.h    # 压缩管道
│   │
│   └── collector/            # 数据收集器 ✅ (NEW!)
│       └── DataCollector.h   # 收集器接口
│
├── src/                      # 实现文件
│   ├── common/              # 通用组件实现 ✅
│   │   ├── Types.cpp
│   │   ├── DatabaseConfig.cpp
│   │   ├── Logger.cpp
│   │   └── Utils.cpp
│   │
│   ├── layer0/              # Layer 0 实现 ✅
│   │   ├── DataIngestion.cpp
│   │   └── SchemaManager.cpp
│   │
│   ├── layer1/              # Layer 1 实现 ✅
│   │   ├── SessionDetector.cpp
│   │   ├── EngagementCalculator.cpp
│   │   ├── ContentExtractor.cpp
│   │   └── ContentClassifier.cpp
│   │
│   ├── collector/           # 数据收集器实现 ✅ (NEW!)
│   │   ├── main.cpp        # 收集器可执行程序入口
│   │   └── DataCollector.cpp # 收集器实现
│   │
│   └── main.cpp             # 主服务入口 ✅
│
├── examples/                # 使用示例 ✅
│   └── basic_ingestion.cpp
│
├── tests/                   # 单元测试 (计划中)
└── CMakeLists.txt          # 构建配置 ✅
```

### 实现状态说明

**✅ 已完成 (第一阶段)**:
- Layer 0: 原始事件存储 (SQLite)
  - 数据摄取 API
  - 数据库模式管理
  
- Layer 1: 会话检测与分析
  - 会话边界检测
  - 参与度计算
  - 内容提取
  - 内容分类
  - 压缩管道框架

- **数据收集器 (NEW!)**:
  - HTTP 客户端实现
  - JSON 解析和事件创建
  - 命令行界面
  - 错误处理和重试逻辑
  - 可执行程序 perception_data_collector

- 通用组件:
  - 类型定义
  - 配置系统
  - 日志系统
  - 工具函数

## 路线图

### 第一阶段（当前 - 已完成 90%）
- [x] Layer 0 实现（原始事件存储）
  - [x] DataIngestion - 事件摄取 API
  - [x] SchemaManager - 数据库模式管理
  - [x] SQLite 事务支持
  
- [x] Layer 1 基础组件
  - [x] SessionDetector - 会话检测
  - [x] EngagementCalculator - 参与度计算
  - [x] ContentExtractor - 内容提取
  - [x] ContentClassifier - 内容分类
  - [x] CompressionPipeline - 压缩管道框架
  
- [x] 通用组件
  - [x] Types - 核心类型定义
  - [x] DatabaseConfig - 配置系统
  - [x] Logger - 日志系统
  - [x] Utils - 工具函数

- [x] **数据收集器 (NEW!)**
  - [x] HTTP 客户端实现（基于 libcurl）
  - [x] JSON 解析和事件创建
  - [x] 自动数据摄取到 Layer 0
  - [x] 命令行界面
  - [x] 错误处理和重试逻辑
  - [x] 可执行程序 perception_data_collector

- [ ] **待完成项**
  - [ ] CompressionPipeline 的 LLM 集成
  - [ ] DuckDB 集成（Layer 1 存储）

### 当前开发优先级

**立即可用 (已完成)**:
1. ✅ Layer 0 数据摄取 - 可以存储原始事件
2. ✅ 会话检测 - 可以分析交互会话
3. ✅ 参与度计算 - 可以评估用户参与度
4. ✅ 内容提取 - 可以提取高注意力内容
5. ✅ **数据收集器** - 可以实时从 API 拉取数据

**下一步 (2-4周)**:
1. 🔨 LLM 集成 - 实现内容压缩
2. 🔨 DuckDB 集成 - Layer 1 存储
3. 🔨 数据保留自动化 - 24 小时清理

**中期目标 (1-3个月)**:
1. 📋 Layer 2 聚合层
2. 📋 完整的压缩管道
3. 📋 查询引擎

**长期目标 (3-6个月)**:
1. 🎯 向量搜索
2. 🎯 知识图谱
3. 🎯 跨设备同步
