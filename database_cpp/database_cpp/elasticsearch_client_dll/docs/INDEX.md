# Elasticsearch C++ 客户端文档索引

欢迎使用 Elasticsearch C++ 客户端！本文档索引帮助你快速找到所需的资源。

---

## ?? 文档目录

### ?? 快速开始

1. **[5分钟快速入门](QUICK_START.md)**
   - 最简单的部署方式
   - 仅需 3 个步骤即可运行
   - 适合：初次使用者

2. **[完整部署指南](ELASTICSEARCH_DEPLOYMENT_GUIDE.md)**
   - WSL、Docker 详细安装步骤
   - Elasticsearch 多种部署方式
   - 故障排除和优化建议
   - 适合：需要详细说明的用户

### ?? 客户端使用

3. **[App Name 唯一性使用说明](APP_NAME_UNIQUE_USAGE.md)**
   - 使用 app_name 作为文档 ID
   - 自动去重机制
   - API 使用示例
   - 数据迁移指南
   - 适合：C++ 开发者

### ??? 部署工具

4. **[Docker 工具使用说明](../docker/README.md)**
   - Docker Compose 配置
   - PowerShell 自动化脚本
   - 批处理交互式脚本
   - 常用操作命令
   - 适合：DevOps、运维人员

---

## ?? 按场景选择文档

### 场景 1: 我是新手，想快速体验

**推荐路径**:
1. 阅读 [QUICK_START.md](QUICK_START.md)
2. 执行安装命令
3. 运行测试程序

**预计时间**: 10-15 分钟

---

### 场景 2: 我要在 Windows 上部署生产环境

**推荐路径**:
1. 阅读 [ELASTICSEARCH_DEPLOYMENT_GUIDE.md](ELASTICSEARCH_DEPLOYMENT_GUIDE.md)
2. 使用 [docker/docker-compose.yml](../docker/docker-compose.yml)
3. 配置安全认证（见部署指南第 3.3 节）
4. 设置定期备份

**预计时间**: 30-60 分钟

---

### 场景 3: 我要开发 C++ 应用连接 Elasticsearch

**推荐路径**:
1. 快速部署 ES：[QUICK_START.md](QUICK_START.md)
2. 学习客户端 API：[APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md)
3. 查看头文件：`../include/ElasticsearchClient.h`
4. 运行示例：`../tests/test_app_name_unique.cpp`

**预计时间**: 20-30 分钟

---

### 场景 4: 我要自动化管理 Elasticsearch 服务

**推荐路径**:
1. Windows 用户：
   - 使用 [Deploy-Elasticsearch.ps1](../docker/Deploy-Elasticsearch.ps1)
   - 或使用 [elasticsearch-manager.bat](../docker/elasticsearch-manager.bat)
2. Linux/Mac 用户：
   - 使用 [docker-compose.yml](../docker/docker-compose.yml)
3. 阅读工具文档：[docker/README.md](../docker/README.md)

**预计时间**: 5-10 分钟

---

## ?? 文档详情

### QUICK_START.md
**内容**：
- WSL 一键安装命令
- Docker Desktop 安装步骤
- Elasticsearch 快速启动
- 基础故障排除

**适合人群**：
- 初学者
- 需要快速验证环境的开发者

---

### ELASTICSEARCH_DEPLOYMENT_GUIDE.md
**内容**：
- 详细的 WSL 安装指南（手动和自动）
- Docker Desktop 完整配置
- 单节点和集群部署
- 生产环境安全配置
- Kibana 可视化工具
- 性能调优
- 完整的故障排除
- 备份恢复策略

**适合人群**：
- 系统管理员
- DevOps 工程师
- 需要生产级部署的用户

---

### APP_NAME_UNIQUE_USAGE.md
**内容**：
- app_name 作为唯一标识符的设计
- 自动更新机制说明
- C++ API 使用示例
- 数据迁移方案
- 查询和操作示例
- 最佳实践

**适合人群**：
- C++ 开发者
- 使用本客户端的应用开发者

---

### docker/README.md
**内容**：
- Docker Compose 详细说明
- PowerShell 脚本使用指南
- 批处理脚本操作说明
- 配置修改方法
- 常见任务命令
- 安全建议

**适合人群**：
- 使用 Docker 部署的用户
- 需要自动化管理的运维人员

---

## ?? 配置文件

| 文件 | 位置 | 说明 |
|------|------|------|
| `docker-compose.yml` | `../docker/` | Docker Compose 配置 |
| `Deploy-Elasticsearch.ps1` | `../docker/` | PowerShell 自动化脚本 |
| `elasticsearch-manager.bat` | `../docker/` | 批处理交互式管理工具 |

---

## ?? 示例代码

| 文件 | 位置 | 说明 |
|------|------|------|
| `test_app_name_unique.cpp` | `../tests/` | App Name 唯一性测试 |
| `es_client_test.cpp` | `../tests/` | 基础客户端测试 |
| `es_performance_test.cpp` | `../tests/` | 性能测试 |

---

## ?? API 参考

### 核心类

**ElasticsearchClient** (`../include/ElasticsearchClient.h`)

主要方法：
- `initializeIndex()` - 创建索引
- `indexDocument()` - 插入/更新文档
- `bulkIndexDocuments()` - 批量操作
- `search()` - 搜索文档
- `getDocumentByAppName()` - 根据 app_name 获取文档
- `deleteDocumentByAppName()` - 根据 app_name 删除文档
- `testConnection()` - 测试连接

### 数据类型

**RawEvent** (`../include/ElasticsearchTypes.h`)

主要字段：
- `eventId` - 事件 ID
- `appName` - 应用名称（用作文档 ID）
- `timestamp` - 时间戳
- `deviceId` - 设备 ID
- `mouseEvents` - 鼠标事件列表
- `systemInfo` - 系统信息

---

## ?? 快速命令参考

### 启动服务

```powershell
# 方法 1: PowerShell 脚本
.\docker\Deploy-Elasticsearch.ps1 -Action start

# 方法 2: Docker Compose
cd docker
docker compose up -d

# 方法 3: 直接运行
docker run -d --name elasticsearch -p 9200:9200 -e "discovery.type=single-node" docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

### 测试连接

```powershell
# PowerShell
Invoke-WebRequest -Uri http://localhost:9200

# 或使用脚本
.\docker\Deploy-Elasticsearch.ps1 -Action test
```

### 查看状态

```powershell
.\docker\Deploy-Elasticsearch.ps1 -Action status
```

### 停止服务

```powershell
# 使用脚本
.\docker\Deploy-Elasticsearch.ps1 -Action stop

# 或 Docker Compose
docker compose down
```

---

## ?? 常见问题

### Q: 我应该先看哪个文档？

**A**: 如果是第一次使用，建议按顺序：
1. [QUICK_START.md](QUICK_START.md) - 快速部署
2. [APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md) - 学习 API
3. [ELASTICSEARCH_DEPLOYMENT_GUIDE.md](ELASTICSEARCH_DEPLOYMENT_GUIDE.md) - 深入了解

### Q: 如何选择部署方式？

**A**: 
- **快速测试**：使用 `QUICK_START.md` 中的单行命令
- **开发环境**：使用 PowerShell 脚本或批处理工具
- **生产环境**：使用 Docker Compose + 安全配置

### Q: 文档在哪里更新？

**A**: 所有文档位于 `docs/` 目录，配置文件位于 `docker/` 目录

### Q: 如何获取帮助？

**A**:
1. 查看相应文档的"故障排除"章节
2. 运行诊断命令：`.\docker\Deploy-Elasticsearch.ps1 -Action test`
3. 查看日志：`docker logs elasticsearch`

---

## ?? 学习路径

### 初级（1-2 小时）
1. ? 安装 Docker 和 WSL
2. ? 部署 Elasticsearch
3. ? 测试基本连接
4. ? 运行示例代码

### 中级（2-4 小时）
1. ? 理解 app_name 唯一性机制
2. ? 学习查询和索引 API
3. ? 配置生产环境参数
4. ? 实现自己的应用

### 高级（4+ 小时）
1. ? 优化性能参数
2. ? 实现集群部署
3. ? 配置安全认证
4. ? 设置监控和备份

---

## ?? 完整文件列表

```
perception_engine/
├── docs/
│   ├── INDEX.md                           # 本文件
│   ├── QUICK_START.md                     # 快速入门
│   ├── ELASTICSEARCH_DEPLOYMENT_GUIDE.md  # 完整部署指南
│   └── APP_NAME_UNIQUE_USAGE.md           # 客户端使用说明
│
├── docker/
│   ├── README.md                          # Docker 工具说明
│   ├── docker-compose.yml                 # Docker Compose 配置
│   ├── Deploy-Elasticsearch.ps1           # PowerShell 脚本
│   └── elasticsearch-manager.bat          # 批处理脚本
│
├── include/
│   ├── ElasticsearchClient.h              # 客户端头文件
│   └── ElasticsearchTypes.h               # 类型定义
│
├── src/
│   └── ElasticsearchClient.cpp            # 客户端实现
│
└── tests/
    ├── test_app_name_unique.cpp           # 唯一性测试
    ├── es_client_test.cpp                 # 基础测试
    └── es_performance_test.cpp            # 性能测试
```

---

## ?? 外部资源

- [Elasticsearch 官方文档](https://www.elastic.co/guide/en/elasticsearch/reference/current/index.html)
- [Docker 官方文档](https://docs.docker.com/)
- [WSL 官方文档](https://docs.microsoft.com/en-us/windows/wsl/)

---

## ?? 技术支持

遇到问题？按以下顺序尝试：

1. ?? 查看对应文档的"故障排除"章节
2. ?? 运行诊断：`Deploy-Elasticsearch.ps1 -Action test`
3. ?? 查看日志：`docker logs elasticsearch`
4. ?? 查看 GitHub Issues

---

**祝你使用愉快！Happy Coding! ??**

*最后更新: 2024*
