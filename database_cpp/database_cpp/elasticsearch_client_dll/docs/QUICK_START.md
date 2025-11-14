# Elasticsearch 快速部署指南（5分钟上手）

> 最简单的方式在 Windows 上部署 Elasticsearch

---

## ?? 前置条件

- Windows 10/11 64位系统
- 至少 4GB 可用内存
- 管理员权限

---

## ?? 快速开始

### 步骤 1: 安装 WSL（2分钟）

以管理员身份打开 PowerShell，执行：

```powershell
wsl --install
```

然后重启电脑。

---

### 步骤 2: 安装 Docker Desktop（5分钟）

1. **下载**：访问 https://www.docker.com/products/docker-desktop/ 
2. **安装**：运行安装程序，勾选 "Use WSL 2"
3. **重启**：安装完成后重启电脑
4. **启动**：打开 Docker Desktop 应用

---

### 步骤 3: 启动 Elasticsearch（1分钟）

打开 PowerShell，复制粘贴以下命令：

```powershell
docker run -d `
  --name elasticsearch `
  -p 9200:9200 `
  -p 9300:9300 `
  -e "discovery.type=single-node" `
  -e "xpack.security.enabled=false" `
  -e "ES_JAVA_OPTS=-Xms1g -Xmx1g" `
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

等待约 30 秒让服务启动。

---

### 步骤 4: 验证安装（30秒）

在浏览器中访问：**http://localhost:9200**

如果看到类似以下 JSON 输出，说明成功了！

```json
{
  "name" : "...",
  "cluster_name" : "...",
  "version" : {
    "number" : "8.11.0"
  },
  "tagline" : "You Know, for Search"
}
```

---

## ? 完成！

现在你的 C++ 程序可以连接到 `http://localhost:9200` 了。

```cpp
elasticsearch::ElasticsearchClient client("http://localhost:9200");
```

---

## ?? 常用命令

### 启动/停止服务

```powershell
# 停止
docker stop elasticsearch

# 启动
docker start elasticsearch

# 重启
docker restart elasticsearch

# 查看状态
docker ps

# 查看日志
docker logs elasticsearch
```

### 删除并重新创建

```powershell
# 删除容器
docker rm -f elasticsearch

# 重新运行步骤 3 的命令
```

---

## ?? 故障排除

### 问题 1: "容器无法启动"

**解决方案**：

```powershell
# 在 PowerShell（管理员）中执行
wsl -d docker-desktop sysctl -w vm.max_map_count=262144
```

然后重启容器：

```powershell
docker restart elasticsearch
```

### 问题 2: "端口被占用"

**解决方案**：使用不同端口

```powershell
docker rm -f elasticsearch
docker run -d `
  --name elasticsearch `
  -p 9201:9200 `
  -e "discovery.type=single-node" `
  -e "xpack.security.enabled=false" `
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

然后在代码中使用 `http://localhost:9201`

### 问题 3: "Docker Desktop 未启动"

确保 Docker Desktop 应用正在运行（系统托盘查看图标）。

---

## ?? 使用 Docker Compose（推荐）

### 1. 创建配置文件

在项目目录创建 `docker-compose.yml`：

```yaml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    container_name: elasticsearch
    environment:
      - discovery.type=single-node
      - xpack.security.enabled=false
      - "ES_JAVA_OPTS=-Xms1g -Xmx1g"
    ports:
      - "9200:9200"
    volumes:
      - esdata:/usr/share/elasticsearch/data
    restart: unless-stopped

volumes:
  esdata:
```

### 2. 启动服务

```bash
docker compose up -d
```

### 3. 停止服务

```bash
docker compose down
```

---

## ?? 下一步

- 查看完整文档：`ELASTICSEARCH_DEPLOYMENT_GUIDE.md`
- 运行测试程序验证连接
- 开始使用 C++ 客户端

---

## ?? 需要帮助？

检查详细文档：
- `ELASTICSEARCH_DEPLOYMENT_GUIDE.md` - 完整部署指南
- `APP_NAME_UNIQUE_USAGE.md` - 客户端使用说明

或查看日志：

```powershell
docker logs elasticsearch
```
