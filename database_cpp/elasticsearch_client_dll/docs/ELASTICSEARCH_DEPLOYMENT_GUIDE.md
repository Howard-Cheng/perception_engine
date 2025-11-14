# Elasticsearch 服务部署指南

本文档详细介绍如何在 Windows 系统上使用 Docker 部署 Elasticsearch 服务。

---

## 目录

1. [安装 WSL (Windows Subsystem for Linux)](#1-安装-wsl-windows-subsystem-for-linux)
2. [安装 Docker Desktop](#2-安装-docker-desktop)
3. [部署 Elasticsearch](#3-部署-elasticsearch)
4. [验证安装](#4-验证安装)
5. [常用操作](#5-常用操作)
6. [故障排除](#6-故障排除)

---

## 1. 安装 WSL (Windows Subsystem for Linux)

### 1.1 检查 Windows 版本

确保你的 Windows 版本支持 WSL 2：
- Windows 10 版本 2004 及更高版本（内部版本 19041 及更高版本）
- Windows 11 任意版本

检查方法：
1. 按 `Win + R`，输入 `winver`，按回车
2. 查看版本号

### 1.2 启用 WSL

**方法一：使用 PowerShell（推荐）**

1. 以**管理员身份**打开 PowerShell
   - 按 `Win + X`，选择 "Windows PowerShell (管理员)" 或 "终端 (管理员)"

2. 执行以下命令安装 WSL：
```powershell
wsl --install
```

3. 重启计算机

**方法二：手动启用功能**

1. 以管理员身份打开 PowerShell

2. 启用 WSL 功能：
```powershell
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
```

3. 启用虚拟机平台：
```powershell
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart
```

4. 重启计算机

5. 下载并安装 WSL2 Linux 内核更新包：
   - 访问：https://aka.ms/wsl2kernel
   - 下载并运行安装程序

6. 设置 WSL 2 为默认版本：
```powershell
wsl --set-default-version 2
```

### 1.3 安装 Linux 发行版

1. 打开 Microsoft Store

2. 搜索 "Ubuntu" 并安装（推荐 Ubuntu 22.04 LTS）

3. 启动 Ubuntu，首次运行会要求创建用户名和密码

4. 验证安装：
```bash
wsl --list --verbose
```

输出示例：
```
  NAME            STATE           VERSION
* Ubuntu-22.04    Running         2
```

---

## 2. 安装 Docker Desktop

### 2.1 下载 Docker Desktop

1. 访问 Docker 官网：https://www.docker.com/products/docker-desktop/

2. 下载 **Docker Desktop for Windows**

3. 系统要求：
   - Windows 10 64-bit: Pro, Enterprise, or Education (Build 19041 or higher)
   - Windows 11 64-bit
   - 启用 WSL 2
   - 至少 4GB RAM

### 2.2 安装 Docker Desktop

1. 运行下载的安装程序 `Docker Desktop Installer.exe`

2. 安装过程中确保勾选：
   - ? **Use WSL 2 instead of Hyper-V** (推荐)
   - ? **Add shortcut to desktop**

3. 点击 "Ok" 开始安装

4. 安装完成后重启计算机

### 2.3 配置 Docker Desktop

1. 启动 Docker Desktop

2. 首次启动可能需要接受服务协议

3. 打开设置（Settings）：
   - 点击右上角齿轮图标 ??

4. **General 设置**：
   - ? Use the WSL 2 based engine
   - ? Start Docker Desktop when you log in

5. **Resources → WSL Integration**：
   - ? Enable integration with my default WSL distro
   - ? Ubuntu-22.04（或你安装的发行版）

6. 点击 "Apply & Restart"

### 2.4 验证 Docker 安装

打开 PowerShell 或 WSL 终端，执行：

```bash
# 检查 Docker 版本
docker --version
# 输出示例: Docker version 24.0.7, build afdd53b

# 检查 Docker Compose 版本
docker compose version
# 输出示例: Docker Compose version v2.23.0

# 运行测试容器
docker run hello-world
```

如果看到 "Hello from Docker!" 消息，说明安装成功。

---

## 3. 部署 Elasticsearch

### 3.1 方法一：使用 Docker 命令（单节点，开发环境）

#### 3.1.1 创建网络

```bash
docker network create elastic
```

#### 3.1.2 启动 Elasticsearch

```bash
docker run -d \
  --name elasticsearch \
  --net elastic \
  -p 9200:9200 \
  -p 9300:9300 \
  -e "discovery.type=single-node" \
  -e "xpack.security.enabled=false" \
  -e "ES_JAVA_OPTS=-Xms512m -Xmx512m" \
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

**参数说明**：
- `-d`: 后台运行
- `--name elasticsearch`: 容器名称
- `--net elastic`: 使用自定义网络
- `-p 9200:9200`: HTTP API 端口映射
- `-p 9300:9300`: 节点间通信端口
- `-e "discovery.type=single-node"`: 单节点模式
- `-e "xpack.security.enabled=false"`: 禁用安全认证（仅开发环境）
- `-e "ES_JAVA_OPTS=-Xms512m -Xmx512m"`: JVM 内存设置

#### 3.1.3 Windows PowerShell 格式

```powershell
docker run -d `
  --name elasticsearch `
  --net elastic `
  -p 9200:9200 `
  -p 9300:9300 `
  -e "discovery.type=single-node" `
  -e "xpack.security.enabled=false" `
  -e "ES_JAVA_OPTS=-Xms512m -Xmx512m" `
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

### 3.2 方法二：使用 Docker Compose（推荐）

#### 3.2.1 创建项目目录

```bash
# 在 WSL 或 PowerShell 中执行
mkdir -p ~/elasticsearch-docker
cd ~/elasticsearch-docker
```

#### 3.2.2 创建 docker-compose.yml

创建文件 `docker-compose.yml`：

```yaml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    container_name: elasticsearch
    environment:
      - node.name=es-node-1
      - cluster.name=es-docker-cluster
      - discovery.type=single-node
      - bootstrap.memory_lock=true
      - "ES_JAVA_OPTS=-Xms1g -Xmx1g"
      - xpack.security.enabled=false
      - xpack.security.http.ssl.enabled=false
    ulimits:
      memlock:
        soft: -1
        hard: -1
    volumes:
      - esdata:/usr/share/elasticsearch/data
    ports:
      - "9200:9200"
      - "9300:9300"
    networks:
      - elastic
    restart: unless-stopped

  # 可选：Kibana 可视化界面
  kibana:
    image: docker.elastic.co/kibana/kibana:8.11.0
    container_name: kibana
    environment:
      - ELASTICSEARCH_HOSTS=http://elasticsearch:9200
      - xpack.security.enabled=false
    ports:
      - "5601:5601"
    networks:
      - elastic
    depends_on:
      - elasticsearch
    restart: unless-stopped

volumes:
  esdata:
    driver: local

networks:
  elastic:
    driver: bridge
```

#### 3.2.3 启动服务

```bash
# 启动所有服务
docker compose up -d

# 查看日志
docker compose logs -f elasticsearch

# 等待 Elasticsearch 启动（约 30-60 秒）
```

### 3.3 生产环境配置（带安全认证）

创建 `docker-compose-prod.yml`：

```yaml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    container_name: elasticsearch
    environment:
      - node.name=es-node-1
      - cluster.name=es-production-cluster
      - discovery.type=single-node
      - bootstrap.memory_lock=true
      - "ES_JAVA_OPTS=-Xms2g -Xmx2g"
      - xpack.security.enabled=true
      - xpack.security.http.ssl.enabled=true
      - xpack.security.http.ssl.key=certs/elasticsearch.key
      - xpack.security.http.ssl.certificate=certs/elasticsearch.crt
      - xpack.security.http.ssl.certificate_authorities=certs/ca.crt
      - ELASTIC_PASSWORD=${ELASTIC_PASSWORD}
    ulimits:
      memlock:
        soft: -1
        hard: -1
    volumes:
      - esdata:/usr/share/elasticsearch/data
      - certs:/usr/share/elasticsearch/config/certs
    ports:
      - "9200:9200"
      - "9300:9300"
    networks:
      - elastic
    restart: unless-stopped

volumes:
  esdata:
    driver: local
  certs:
    driver: local

networks:
  elastic:
    driver: bridge
```

启动：
```bash
export ELASTIC_PASSWORD="your_secure_password"
docker compose -f docker-compose-prod.yml up -d
```

---

## 4. 验证安装

### 4.1 检查容器状态

```bash
# 查看运行中的容器
docker ps

# 输出示例：
# CONTAINER ID   IMAGE                                                  STATUS          PORTS
# abc123def456   docker.elastic.co/elasticsearch/elasticsearch:8.11.0   Up 2 minutes    0.0.0.0:9200->9200/tcp
```

### 4.2 测试 Elasticsearch API

**方法一：使用 curl（WSL/Linux）**

```bash
curl http://localhost:9200
```

**方法二：使用 PowerShell**

```powershell
Invoke-WebRequest -Uri http://localhost:9200 | Select-Object -Expand Content
```

**方法三：使用浏览器**

打开浏览器访问：http://localhost:9200

**预期输出**：

```json
{
  "name" : "es-node-1",
  "cluster_name" : "es-docker-cluster",
  "cluster_uuid" : "xyz123...",
  "version" : {
    "number" : "8.11.0",
    "build_flavor" : "default",
    "build_type" : "docker",
    "build_hash" : "...",
    "build_date" : "2023-11-04T10:04:57.184859352Z",
    "build_snapshot" : false,
    "lucene_version" : "9.8.0",
    "minimum_wire_compatibility_version" : "7.17.0",
    "minimum_index_compatibility_version" : "7.0.0"
  },
  "tagline" : "You Know, for Search"
}
```

### 4.3 检查集群健康状态

```bash
curl http://localhost:9200/_cluster/health?pretty
```

或使用 PowerShell：

```powershell
Invoke-WebRequest -Uri "http://localhost:9200/_cluster/health?pretty" | Select-Object -Expand Content
```

**预期输出**：

```json
{
  "cluster_name" : "es-docker-cluster",
  "status" : "green",
  "timed_out" : false,
  "number_of_nodes" : 1,
  "number_of_data_nodes" : 1,
  "active_primary_shards" : 0,
  "active_shards" : 0,
  "relocating_shards" : 0,
  "initializing_shards" : 0,
  "unassigned_shards" : 0,
  "delayed_unassigned_shards" : 0,
  "number_of_pending_tasks" : 0,
  "number_of_in_flight_fetch" : 0,
  "task_max_waiting_in_queue_millis" : 0,
  "active_shards_percent_as_number" : 100.0
}
```

### 4.4 测试 C++ 客户端连接

编译并运行测试程序：

```bash
cd /path/to/your/project/build
./es_client_test
```

或者创建简单测试：

```cpp
#include "ElasticsearchClient.h"
#include <iostream>

int main() {
    elasticsearch::ElasticsearchClient client("http://localhost:9200");
    
    if (client.testConnection()) {
        std::cout << "? 成功连接到 Elasticsearch!" << std::endl;
        std::cout << client.getClusterInfo() << std::endl;
    } else {
        std::cerr << "? 无法连接到 Elasticsearch" << std::endl;
        return 1;
    }
    
    return 0;
}
```

---

## 5. 常用操作

### 5.1 容器管理

```bash
# 启动容器
docker start elasticsearch

# 停止容器
docker stop elasticsearch

# 重启容器
docker restart elasticsearch

# 删除容器（会丢失数据，除非使用了持久化卷）
docker rm -f elasticsearch

# 查看容器日志
docker logs elasticsearch

# 实时查看日志
docker logs -f elasticsearch

# 进入容器
docker exec -it elasticsearch bash
```

### 5.2 Docker Compose 操作

```bash
# 启动所有服务
docker compose up -d

# 停止所有服务
docker compose down

# 停止并删除卷（会删除所有数据！）
docker compose down -v

# 查看服务状态
docker compose ps

# 查看日志
docker compose logs -f

# 重启服务
docker compose restart

# 更新镜像并重启
docker compose pull
docker compose up -d
```

### 5.3 数据备份

```bash
# 创建备份目录
mkdir -p ~/elasticsearch-backup

# 备份数据卷
docker run --rm \
  -v esdata:/data \
  -v ~/elasticsearch-backup:/backup \
  ubuntu tar czf /backup/esdata-backup-$(date +%Y%m%d).tar.gz /data

# 恢复数据
docker run --rm \
  -v esdata:/data \
  -v ~/elasticsearch-backup:/backup \
  ubuntu tar xzf /backup/esdata-backup-20231201.tar.gz -C /
```

### 5.4 性能调优

**调整 JVM 内存**（根据可用 RAM）：

```yaml
environment:
  - "ES_JAVA_OPTS=-Xms4g -Xmx4g"  # 4GB heap
```

**增加文件描述符限制**：

```yaml
ulimits:
  nofile:
    soft: 65536
    hard: 65536
  memlock:
    soft: -1
    hard: -1
```

### 5.5 监控

**使用 Kibana**（如果已安装）：

访问：http://localhost:5601

**查看节点统计信息**：

```bash
curl http://localhost:9200/_nodes/stats?pretty
```

**查看索引信息**：

```bash
curl http://localhost:9200/_cat/indices?v
```

---

## 6. 故障排除

### 6.1 容器无法启动

**检查日志**：
```bash
docker logs elasticsearch
```

**常见错误及解决方案**：

#### 错误 1: "max virtual memory areas vm.max_map_count [65530] is too low"

**解决方案（WSL）**：

```bash
# 在 WSL 终端中执行
sudo sysctl -w vm.max_map_count=262144

# 永久设置
echo "vm.max_map_count=262144" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

**解决方案（Windows）**：

1. 以管理员身份运行 PowerShell
2. 执行：
```powershell
wsl -d docker-desktop sysctl -w vm.max_map_count=262144
```

#### 错误 2: 内存不足

**减少 JVM heap 大小**：

```yaml
environment:
  - "ES_JAVA_OPTS=-Xms512m -Xmx512m"
```

#### 错误 3: 端口被占用

**检查端口占用**：

```powershell
# Windows
netstat -ano | findstr :9200

# Linux/WSL
sudo lsof -i :9200
```

**更改端口**：

```yaml
ports:
  - "9201:9200"  # 使用 9201 代替 9200
```

### 6.2 无法连接到 Elasticsearch

**检查防火墙**：

```powershell
# Windows 防火墙允许端口
New-NetFirewallRule -DisplayName "Elasticsearch" -Direction Inbound -Protocol TCP -LocalPort 9200 -Action Allow
```

**检查容器网络**：

```bash
docker network inspect elastic
```

**检查服务是否正在监听**：

```bash
docker exec elasticsearch curl http://localhost:9200
```

### 6.3 性能问题

**检查资源使用**：

```bash
docker stats elasticsearch
```

**优化建议**：

1. 增加 Docker Desktop 分配的 CPU 和内存
   - 设置 → Resources → Advanced
   - 推荐至少 4GB RAM，2 CPU cores

2. 使用 SSD 存储数据卷

3. 调整 Elasticsearch 配置：
```yaml
environment:
  - "indices.memory.index_buffer_size=30%"
  - "thread_pool.write.queue_size=1000"
```

### 6.4 数据丢失

**使用持久化卷**：

确保 docker-compose.yml 中有：

```yaml
volumes:
  - esdata:/usr/share/elasticsearch/data
```

**定期备份**（参见 5.3 节）

### 6.5 WSL 相关问题

**WSL 未启动**：

```powershell
# 重启 WSL
wsl --shutdown
wsl
```

**WSL 版本问题**：

```powershell
# 检查版本
wsl --list --verbose

# 转换为 WSL 2
wsl --set-version Ubuntu-22.04 2
```

---

## 7. 配置 C++ 客户端连接

### 7.1 基本配置

```cpp
#include "ElasticsearchClient.h"

// 本地开发环境
elasticsearch::ElasticsearchClient client("http://localhost:9200");

// 远程服务器
elasticsearch::ElasticsearchClient client("http://192.168.1.100:9200");

// 自定义端口
elasticsearch::ElasticsearchClient client("http://localhost:9201");
```

### 7.2 验证连接

```cpp
if (!client.testConnection()) {
    std::cerr << "错误: 无法连接到 Elasticsearch" << std::endl;
    std::cerr << "请确保:" << std::endl;
    std::cerr << "1. Docker 容器正在运行: docker ps" << std::endl;
    std::cerr << "2. 端口 9200 未被占用" << std::endl;
    std::cerr << "3. 防火墙允许连接" << std::endl;
    return 1;
}

std::cout << "成功连接到 Elasticsearch!" << std::endl;
```

---

## 8. 快速启动脚本

### 8.1 Windows 批处理脚本

创建 `start-elasticsearch.bat`：

```batch
@echo off
echo ========================================
echo Starting Elasticsearch with Docker
echo ========================================

echo.
echo Checking if container exists...
docker ps -a --filter name=elasticsearch --format "{{.Names}}" > nul 2>&1

if %ERRORLEVEL% EQU 0 (
    echo Container exists, starting...
    docker start elasticsearch
) else (
    echo Creating new container...
    docker run -d ^
      --name elasticsearch ^
      -p 9200:9200 ^
      -p 9300:9300 ^
      -e "discovery.type=single-node" ^
      -e "xpack.security.enabled=false" ^
      -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" ^
      docker.elastic.co/elasticsearch/elasticsearch:8.11.0
)

echo.
echo Waiting for Elasticsearch to start...
timeout /t 10 /nobreak > nul

echo.
echo Testing connection...
curl http://localhost:9200

echo.
echo ========================================
echo Elasticsearch is ready!
echo API: http://localhost:9200
echo ========================================
pause
```

### 8.2 PowerShell 脚本

创建 `Start-Elasticsearch.ps1`：

```powershell
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Starting Elasticsearch with Docker" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$containerName = "elasticsearch"

# 检查容器是否存在
$exists = docker ps -a --filter name=$containerName --format "{{.Names}}"

if ($exists) {
    Write-Host "Container exists, starting..." -ForegroundColor Yellow
    docker start $containerName
} else {
    Write-Host "Creating new container..." -ForegroundColor Yellow
    docker run -d `
      --name $containerName `
      -p 9200:9200 `
      -p 9300:9300 `
      -e "discovery.type=single-node" `
      -e "xpack.security.enabled=false" `
      -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" `
      docker.elastic.co/elasticsearch/elasticsearch:8.11.0
}

Write-Host "`nWaiting for Elasticsearch to start..." -ForegroundColor Yellow
Start-Sleep -Seconds 15

Write-Host "`nTesting connection..." -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://localhost:9200" -UseBasicParsing
    Write-Host $response.Content -ForegroundColor Green
    
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host "Elasticsearch is ready!" -ForegroundColor Green
    Write-Host "API: http://localhost:9200" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
} catch {
    Write-Host "Failed to connect. Check logs: docker logs elasticsearch" -ForegroundColor Red
}
```

运行：
```powershell
powershell -ExecutionPolicy Bypass -File Start-Elasticsearch.ps1
```

---

## 9. 参考资源

### 官方文档

- Docker 官方文档: https://docs.docker.com/
- Elasticsearch 官方文档: https://www.elastic.co/guide/en/elasticsearch/reference/current/index.html
- Docker Hub Elasticsearch: https://hub.docker.com/_/elasticsearch

### 有用的链接

- WSL 文档: https://docs.microsoft.com/en-us/windows/wsl/
- Docker Compose 文档: https://docs.docker.com/compose/
- Elasticsearch REST API: https://www.elastic.co/guide/en/elasticsearch/reference/current/rest-apis.html

### 常用命令速查

```bash
# Docker 基础
docker ps                    # 查看运行中的容器
docker ps -a                 # 查看所有容器
docker logs <container>      # 查看日志
docker exec -it <container> bash  # 进入容器

# Elasticsearch API
GET  /_cluster/health        # 集群健康
GET  /_cat/indices?v         # 列出索引
GET  /<index>/_search        # 搜索
PUT  /<index>/_doc/<id>      # 创建/更新文档
DELETE /<index>              # 删除索引
```

---

## 10. 下一步

安装完成后，你可以：

1. ? 运行项目中的测试程序验证连接
2. ? 创建第一个索引
3. ? 插入测试数据
4. ? 执行搜索查询
5. ? 配置 Kibana 进行可视化（可选）

**快速测试**：

```bash
cd D:\PerceiptionEngine_Howard\perception_engine\database_cpp\database_cpp\elasticsearch_client_dll\build\
.\es_client_test.exe
```

祝你使用愉快！ ??
