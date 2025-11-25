# ?? Elasticsearch 9.2.1 开发部署指南

## ?? 概述

这是一套为开发环境优化的 Elasticsearch 9.2.1 部署脚本，特点：

- ? **版本:** Elasticsearch 9.2.1
- ? **绑定地址:** 127.0.0.1 (localhost only)
- ? **协议:** HTTP (无 SSL)
- ? **认证:** 禁用 (无需用户名密码)
- ? **配置:** 自动化，开箱即用

## ? 快速开始

### 步骤 1: 部署 Elasticsearch

**双击运行:**
```
deploy_elasticsearch_dev.bat
```

**或者 PowerShell:**
```powershell
.\deploy_elasticsearch_dev.ps1
```

这个脚本会：
1. 检测 Java 环境 (需要 Java 17+)
2. 下载 Elasticsearch 9.2.1 (~400 MB)
3. 解压到当前目录
4. 自动配置为开发模式
   - 绑定到 localhost (127.0.0.1)
   - 禁用安全功能
   - 单节点模式

**时间:** 5-10 分钟（取决于网络速度）

### 步骤 2: 启动 Elasticsearch

**双击运行:**
```
start_elasticsearch_dev.bat
```

**或者 PowerShell:**
```powershell
.\start_elasticsearch_dev.ps1
```

**启动时间:** 30-60 秒（首次启动）

### 步骤 3: 测试连接

**双击运行:**
```
test_elasticsearch_dev.ps1
```

**或者在浏览器中打开:**
```
http://localhost:9200
```

**或者 PowerShell:**
```powershell
Invoke-WebRequest http://localhost:9200
```

**期望结果:**
```json
{
  "name" : "node-1",
  "cluster_name" : "elasticsearch-dev",
  "version" : {
    "number" : "9.2.1"
  },
  "tagline" : "You Know, for Search"
}
```

---

## ?? 文件说明

| 文件 | 用途 |
|------|------|
| `deploy_elasticsearch_dev.ps1` | 部署脚本（主要） |
| `deploy_elasticsearch_dev.bat` | 部署脚本（双击运行） |
| `start_elasticsearch_dev.ps1` | 启动脚本 |
| `start_elasticsearch_dev.bat` | 启动脚本（双击运行） |
| `stop_elasticsearch_dev.ps1` | 停止脚本 |
| `test_elasticsearch_dev.ps1` | 测试脚本 |

---

## ?? 配置说明

### 自动生成的配置 (elasticsearch.yml)

```yaml
# Cluster settings
cluster.name: elasticsearch-dev
node.name: node-1

# Network settings - LOCALHOST ONLY
network.host: 127.0.0.1
http.port: 9200

# Discovery settings (single node)
discovery.type: single-node

# Security - DISABLED for development
xpack.security.enabled: false
xpack.security.enrollment.enabled: false
xpack.security.http.ssl.enabled: false
xpack.security.transport.ssl.enabled: false
```

### 关键配置解释

- **`network.host: 127.0.0.1`** - 只绑定到 localhost，无法从外网访问
- **`discovery.type: single-node`** - 单节点模式，无需集群配置
- **`xpack.security.enabled: false`** - 禁用安全，无需认证

---

## ?? C++ 应用集成

### 更新 config.ini

```ini
[Database]
elasticsearch_url=http://localhost:9200
elasticsearch_index=perception_context
# 不需要 username/password
```

### 启动应用

```powershell
cd D:\PerceiptionEngine_Howard\perception_engine\windows_code\buildnew
.\PerceptionEngine.exe --console
```

### 期望日志

```
[INFO] ? Elasticsearch initialized - auto storage every 5 seconds
```

---

## ? 验证清单

部署完成后，确认以下内容：

- [ ] Java 17+ 已安装
- [ ] Elasticsearch 9.2.1 已下载并解压
- [ ] 配置文件已更新（localhost 绑定）
- [ ] Elasticsearch 启动成功
- [ ] 端口 9200 监听中
- [ ] HTTP 连接成功 (http://localhost:9200)
- [ ] 集群健康状态为 green 或 yellow
- [ ] C++ 应用可以连接

---

## ?? 故障排除

### 问题 1: Java 未检测到

**症状:**
```
? Java not detected
```

**解决:**
```powershell
# 检查 Java 版本
java -version

# 如果未安装，下载 Java 17+
# https://adoptium.net/
```

### 问题 2: 端口 9200 被占用

**症状:**
```
Port 9200 already in use
```

**解决:**
```powershell
# 查看占用进程
Get-NetTCPConnection -LocalPort 9200

# 停止旧的 ES 实例
.\stop_elasticsearch_dev.ps1

# 或停止占用端口的其他程序
```

### 问题 3: 下载失败

**症状:**
```
Download failed: Connection timeout
```

**解决:**
1. 检查网络连接
2. 使用浏览器手动下载：
   ```
   https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-9.2.1-windows-x86_64.zip
   ```
3. 将 ZIP 文件放到脚本目录
4. 重新运行脚本（会自动检测已下载的文件）

### 问题 4: 启动后无法连接

**检查步骤:**

```powershell
# 1. 检查进程
Get-Process java | Where-Object { $_.WorkingSet64 -gt 100MB }

# 2. 检查端口
Get-NetTCPConnection -LocalPort 9200

# 3. 查看日志
Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 50

# 4. 检查配置
Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml | Select-String "network.host"
```

**期望看到:**
```
network.host: 127.0.0.1
```

### 问题 5: 绑定到错误的地址

**症状:**
日志显示 `publish_address {198.18.0.1:9200}` 而不是 `127.0.0.1`

**解决:**
```powershell
# 1. 停止 ES
.\stop_elasticsearch_dev.ps1

# 2. 重新部署（会重新生成配置）
.\deploy_elasticsearch_dev.ps1

# 3. 确认配置
Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml | Select-String "network"

# 4. 重新启动
.\start_elasticsearch_dev.ps1
```

---

## ?? 性能指标

- **内存使用:** 约 1-2 GB (默认)
- **磁盘空间:** 约 500 MB (安装) + 数据大小
- **启动时间:** 30-60 秒
- **响应时间:** < 100ms (localhost)

---

## ?? 重要提示

### 仅用于开发环境

这个配置**不适合生产环境**，因为：

- ? 安全功能已禁用
- ? 无认证机制
- ? 只能 localhost 访问
- ? 单节点模式（无高可用）

### 生产环境建议

对于生产环境，应该：

- ? 启用 HTTPS/SSL
- ? 配置认证 (用户名/密码)
- ? 使用集群模式
- ? 配置防火墙规则
- ? 定期备份数据
- ? 监控和日志分析

---

## ?? 常用命令

### 管理命令

```powershell
# 启动
.\start_elasticsearch_dev.ps1

# 停止
.\stop_elasticsearch_dev.ps1

# 测试
.\test_elasticsearch_dev.ps1

# 查看日志
Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 50 -Wait
```

### API 测试

```powershell
# 基本信息
Invoke-WebRequest http://localhost:9200

# 集群健康
Invoke-WebRequest http://localhost:9200/_cluster/health

# 列出索引
Invoke-WebRequest http://localhost:9200/_cat/indices?v

# 集群统计
Invoke-WebRequest http://localhost:9200/_cluster/stats
```

### 数据操作

```powershell
# 创建文档
$body = '{"message":"Hello ES 9.2.1","timestamp":"2024-01-XX"}' 
Invoke-WebRequest -Method POST http://localhost:9200/test/_doc -ContentType "application/json" -Body $body

# 搜索
Invoke-WebRequest -Method POST http://localhost:9200/test/_search -ContentType "application/json" -Body '{"query":{"match_all":{}}}'

# 删除索引
Invoke-WebRequest -Method DELETE http://localhost:9200/test
```

---

## ?? 学习资源

- [Elasticsearch 官方文档](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/index.html)
- [REST API 参考](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/rest-apis.html)
- [查询 DSL](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/query-dsl.html)

---

## ?? 版本历史

### Version 2.0 (2024-01-XX)
- ? 升级到 Elasticsearch 9.2.1
- ? 强制绑定到 localhost (127.0.0.1)
- ? 自动化配置生成
- ? 改进的进程检测
- ? 完整的测试脚本

### Version 1.0
- 初始版本 (Elasticsearch 8.12.1)

---

## ?? 获取帮助

如果遇到问题：

1. **查看日志:**
   ```powershell
   Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 100
   ```

2. **运行诊断:**
   ```powershell
   .\test_elasticsearch_dev.ps1
   ```

3. **检查配置:**
   ```powershell
   Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml
   ```

4. **完全重新部署:**
   ```powershell
   .\stop_elasticsearch_dev.ps1
   Remove-Item .\elasticsearch-9.2.1 -Recurse -Force
   .\deploy_elasticsearch_dev.ps1
   ```

---

**最后更新:** 2024-01-XX  
**Elasticsearch 版本:** 9.2.1  
**状态:** ? 生产就绪（开发配置）
