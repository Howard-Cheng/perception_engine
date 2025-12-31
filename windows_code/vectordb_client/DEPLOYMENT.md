# Qdrant 部署指南

## Docker 数据存储位置

### 默认情况（数据会丢失）
```bash
docker run -p 6333:6333 -p 6334:6334 qdrant/qdrant
```
数据存储在容器内部，容器删除后数据会丢失。

### 持久化存储（推荐）

#### 方式 1：使用 Docker Volume
```bash
docker run -p 6333:6333 -p 6334:6334 \
  -v qdrant_storage:/qdrant/storage \
  qdrant/qdrant
```
数据存储在 Docker volume `qdrant_storage` 中，即使容器删除，数据也会保留。

查看 volume 位置：
```bash
docker volume inspect qdrant_storage
```

#### 方式 2：使用 Bind Mount（指定本地目录）
```bash
# Windows
docker run -p 6333:6333 -p 6334:6334 \
  -v C:/qdrant_data:/qdrant/storage \
  qdrant/qdrant

# Linux/Mac
docker run -p 6333:6333 -p 6334:6334 \
  -v /path/to/qdrant_data:/qdrant/storage \
  qdrant/qdrant
```
数据直接存储在指定的本地目录中。

## 生产环境部署

### 1. Docker Compose（推荐）

创建 `docker-compose.yml`：

```yaml
version: '3.8'

services:
  qdrant:
    image: qdrant/qdrant:latest
    ports:
      - "6333:6333"  # REST API
      - "6334:6334"  # gRPC API
    volumes:
      - qdrant_storage:/qdrant/storage
    restart: unless-stopped
    environment:
      - QDRANT__SERVICE__GRPC_PORT=6334

volumes:
  qdrant_storage:
```

启动：
```bash
docker-compose up -d
```

停止：
```bash
docker-compose down
```

### 2. 直接运行二进制文件

#### 步骤 1：下载 Qdrant 二进制
- 从 https://github.com/qdrant/qdrant/releases 下载
- Windows: `qdrant-x.x.x-windows-amd64.exe`
- Linux: `qdrant-x.x.x-linux-amd64`

#### 步骤 2：启动服务

**Windows（前台运行）:**
```powershell
.\qdrant.exe
```

**Windows（后台运行）:**
```powershell
Start-Process -FilePath ".\qdrant.exe" -WindowStyle Hidden
```

**Linux（前台运行）:**
```bash
./qdrant
```

**Linux（后台运行）:**
```bash
./qdrant > qdrant.log 2>&1 &
```

#### 步骤 3：验证服务启动

**方法 1：检查健康状态（推荐）**
```powershell
# PowerShell
1. 检查根路径
Invoke-WebRequest -Uri "http://localhost:6333/" -UseBasicParsing
2. 检查集合接口
Invoke-WebRequest -Uri "http://localhost:6333/collections" -UseBasicParsing

# 或使用 curl
curl http://localhost:6333/health
```
**预期输出**：`{"title":"healthz","version":"1.x.x"}`，状态码 200

**方法 2：检查端口监听**
```powershell
# Windows PowerShell
Get-NetTCPConnection -LocalPort 6333,6334 -State Listen

# Linux
netstat -tlnp | grep -E '6333|6334'
# 或
ss -tlnp | grep -E '6333|6334'
```
**预期输出**：显示端口 6333（REST API）和 6334（gRPC）正在监听

**方法 3：检查进程**
```powershell
# Windows PowerShell
Get-Process qdrant

# Linux
ps aux | grep qdrant
```

**方法 4：访问 Web UI**
打开浏览器访问：
- **管理界面**: http://localhost:6333/dashboard
- **REST API**: http://localhost:6333

**方法 5：测试 API 调用**
```powershell
# 列出所有集合
Invoke-RestMethod -Uri "http://localhost:6333/collections" -Method Get

# 或使用 curl
curl http://localhost:6333/collections
```

#### 步骤 4：配置数据目录（可选）

创建 `config.yaml`：
```yaml
storage:
  storage_path: ./qdrant_data
```

运行：
```bash
# Windows
.\qdrant.exe --config-path config.yaml

# Linux
./qdrant --config-path config.yaml
```

#### 停止服务

**Windows:**
```powershell
# 查找进程 ID
Get-Process qdrant | Select-Object Id

# 停止进程
Stop-Process -Name qdrant
# 或使用 PID
Stop-Process -Id <进程ID>
```

**Linux:**
```bash
# 查找进程 ID
ps aux | grep qdrant

# 停止进程
killall qdrant
# 或使用 PID
kill <进程ID>
```

### 3. Systemd 服务（Linux）

创建 `/etc/systemd/system/qdrant.service`：

```ini
[Unit]
Description=Qdrant Vector Database
After=network.target

[Service]
Type=simple
User=qdrant
WorkingDirectory=/opt/qdrant
ExecStart=/opt/qdrant/qdrant
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

启动：
```bash
sudo systemctl enable qdrant
sudo systemctl start qdrant
```

### 4. Kubernetes 部署

使用 Helm Chart：
```bash
helm repo add qdrant https://qdrant.to/helm
helm install qdrant qdrant/qdrant
```

## 打包和分发

### Docker 镜像

构建包含 Qdrant 的应用镜像：

```dockerfile
FROM qdrant/qdrant:latest as qdrant
FROM your-app-image:latest

# Copy Qdrant binary (if needed)
# COPY --from=qdrant /usr/bin/qdrant /usr/local/bin/

# Or use Qdrant as a service
# Your app connects to qdrant:6333
```

### 二进制文件打包

1. 下载 Qdrant 二进制文件
2. 与你的应用一起打包
3. 提供启动脚本：

**Windows (`start_qdrant.bat`):**
```batch
@echo off
start "Qdrant" qdrant.exe
timeout /t 3
echo Qdrant started on http://localhost:6333
```

**Linux (`start_qdrant.sh`):**
```bash
#!/bin/bash
./qdrant &
sleep 3
echo "Qdrant started on http://localhost:6333"
```

## 配置应用连接

在代码中配置连接：

```cpp
// 连接到本地 Qdrant 服务器
auto config = QdrantClient::Config::remote("http://localhost:6333");
QdrantClient client(config);

// 或连接到远程服务器
auto config = QdrantClient::Config::remote("http://your-server:6333", "your-api-key");
QdrantClient client(config);
```

## 数据备份

### 备份数据目录

```bash
# Docker volume
docker run --rm -v qdrant_storage:/data -v $(pwd):/backup \
  alpine tar czf /backup/qdrant_backup.tar.gz -C /data .

# Bind mount
tar czf qdrant_backup.tar.gz /path/to/qdrant_data
```

### 恢复数据

```bash
# Docker volume
docker run --rm -v qdrant_storage:/data -v $(pwd):/backup \
  alpine tar xzf /backup/qdrant_backup.tar.gz -C /data

# Bind mount
tar xzf qdrant_backup.tar.gz -C /path/to/qdrant_data
```

## 性能优化

### 资源配置

```yaml
# docker-compose.yml
services:
  qdrant:
    image: qdrant/qdrant:latest
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
        reservations:
          cpus: '2'
          memory: 4G
```

### 存储优化

- 使用 SSD 存储
- 配置足够的磁盘空间
- 定期清理不需要的集合

## 监控和日志

### 健康检查

```bash
curl http://localhost:6333/health
```

### 查看日志

```bash
# Docker
docker logs qdrant

# Systemd
journalctl -u qdrant -f
```

## 安全配置

### API Key 认证

1. 设置环境变量：
```bash
export QDRANT_API_KEY=your-secret-key
```

2. 在代码中使用：
```cpp
auto config = QdrantClient::Config::remote(
    "http://localhost:6333",
    "your-secret-key"
);
```

### HTTPS

配置反向代理（Nginx/Traefik）提供 HTTPS 访问。

