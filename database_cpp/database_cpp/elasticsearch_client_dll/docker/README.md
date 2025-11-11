# Elasticsearch Docker Deployment Tools

This directory contains tools for quickly deploying and managing Elasticsearch services.

---

## ?? File Description

| File | Purpose | Use Case |
|------|------|----------|
| `docker-compose.yml` | Docker Compose configuration file | Production environment, long-term use |
| `Deploy-Elasticsearch.ps1` | PowerShell automation script | Windows users, automated management |
| `elasticsearch-manager.bat` | Batch interactive script | Windows users, simple operations |

---

## ?? Quick Start

### Method 1: Using Batch Script (Recommended for Beginners)

1. **Double-click to run** `elasticsearch-manager.bat`

2. Select option:
   - `1` - Start Elasticsearch
   - `4` - View Status
   - `6` - Test Connection

3. Done!

### Method 2: Using PowerShell Script

```powershell
# Start service
.\Deploy-Elasticsearch.ps1 -Action start

# View status
.\Deploy-Elasticsearch.ps1 -Action status

# Test connection
.\Deploy-Elasticsearch.ps1 -Action test

# Start service (including Kibana)
.\Deploy-Elasticsearch.ps1 -Action start -WithKibana

# View real-time logs
.\Deploy-Elasticsearch.ps1 -Action logs -Follow
```

### Method 3: Using Docker Compose

```bash
# Start services
docker compose up -d

# View status
docker compose ps

# View logs
docker compose logs -f elasticsearch

# Stop services
docker compose down
```

---

## ?? Detailed Usage Instructions

### Docker Compose Configuration

`docker-compose.yml` provides complete production-grade configuration:

- **Elasticsearch**
  - Ports: 9200 (HTTP API), 9300 (node communication)
  - Memory: 1GB JVM heap
  - Persistent storage
  - Auto-restart

- **Kibana** (optional)
  - Port: 5601
  - Visualization interface
  - Data exploration tool

**Start all services**:
```bash
docker compose up -d
```

**Start Elasticsearch only**:
```bash
docker compose up -d elasticsearch
```

**View logs**:
```bash
# All services
docker compose logs -f

# Elasticsearch only
docker compose logs -f elasticsearch

# Kibana only
docker compose logs -f kibana
```

**Stop services**:
```bash
# Stop but keep data
docker compose down

# Stop and delete all data
docker compose down -v
```

---

### PowerShell Script Details

`Deploy-Elasticsearch.ps1` provides comprehensive automated management features.

#### Basic Operations

```powershell
# Start service
.\Deploy-Elasticsearch.ps1 -Action start

# Stop service
.\Deploy-Elasticsearch.ps1 -Action stop

# Restart service
.\Deploy-Elasticsearch.ps1 -Action restart

# View status
.\Deploy-Elasticsearch.ps1 -Action status
```

#### Advanced Features

```powershell
# View logs (last 50 lines)
.\Deploy-Elasticsearch.ps1 -Action logs

# View logs in real-time
.\Deploy-Elasticsearch.ps1 -Action logs -Follow

# Test connection and functionality
.\Deploy-Elasticsearch.ps1 -Action test

# Clean all resources (including data!)
.\Deploy-Elasticsearch.ps1 -Action clean
```

#### Start with Kibana

```powershell
.\Deploy-Elasticsearch.ps1 -Action start -WithKibana
```

Kibana will be available at http://localhost:5601

---

### Batch Script Usage

`elasticsearch-manager.bat` provides an interactive menu interface:

1. **Start Elasticsearch** - Create and start container
2. **Stop Elasticsearch** - Stop container (data retained)
3. **Restart Elasticsearch** - Restart service
4. **View Status** - Display container status and resource usage
5. **View Logs** - Display recent logs
6. **Test Connection** - Verify service is running properly
7. **Clean All Data** - Delete container and data
8. **Exit** - Exit program

---

## ?? Configuration Instructions

### Modify Port

**docker-compose.yml**:
```yaml
ports:
  - "9201:9200"  # Change 9200 to 9201
```

**PowerShell script**:
Modify at top of file:
```powershell
$ES_PORT = 9201
```

**Batch script**:
Modify at top of file:
```batch
set ES_PORT=9201
```

### Adjust Memory

**docker-compose.yml**:
```yaml
environment:
  - "ES_JAVA_OPTS=-Xms2g -Xmx2g"  # Change to 2GB
```

**Single command**:
```powershell
docker run ... -e "ES_JAVA_OPTS=-Xms2g -Xmx2g" ...
```

### Enable Security Authentication

Modify `docker-compose.yml`:
```yaml
environment:
  - xpack.security.enabled=true
  - ELASTIC_PASSWORD=your_password
```

Use in C++ code:
```cpp
// Need to add authentication support to client
```

---

## ?? Service Access URLs

After successful startup, access via the following URLs:

| Service | URL | Description |
|------|------|------|
| Elasticsearch API | http://localhost:9200 | REST API interface |
| Cluster Health | http://localhost:9200/_cluster/health | Cluster health status |
| Indices | http://localhost:9200/_cat/indices?v | Index list |
| Kibana | http://localhost:5601 | Visualization interface (if enabled) |

---

## ?? Test Connection

### Using curl (WSL/Linux)

```bash
# Basic connection test
curl http://localhost:9200

# Cluster health
curl http://localhost:9200/_cluster/health?pretty

# Create index
curl -X PUT http://localhost:9200/test-index

# View all indices
curl http://localhost:9200/_cat/indices?v
```

### Using PowerShell

```powershell
# Basic connection test
Invoke-WebRequest -Uri http://localhost:9200 | Select-Object -Expand Content

# Cluster health
Invoke-WebRequest -Uri "http://localhost:9200/_cluster/health?pretty" | Select-Object -Expand Content
```

### Using Browser

Direct access: http://localhost:9200

### Using C++ Client

```cpp
#include "ElasticsearchClient.h"

elasticsearch::ElasticsearchClient client("http://localhost:9200");

if (client.testConnection()) {
    std::cout << "Connection successful!" << std::endl;
    std::cout << client.getClusterInfo() << std::endl;
}
```

---

## ?? Common Tasks

### View All Indices

```bash
curl http://localhost:9200/_cat/indices?v
```

### Create Index

```bash
curl -X PUT http://localhost:9200/my-index
```

### Delete Index

```bash
curl -X DELETE http://localhost:9200/my-index
```

### Query Data

```bash
curl http://localhost:9200/my-index/_search?pretty
```

### Backup Data

```powershell
# Using PowerShell
docker run --rm `
  -v elasticsearch_data:/data `
  -v ${PWD}/backup:/backup `
  ubuntu tar czf /backup/es-backup-$(Get-Date -Format 'yyyyMMdd').tar.gz /data
```

### Restore Data

```powershell
docker run --rm `
  -v elasticsearch_data:/data `
  -v ${PWD}/backup:/backup `
  ubuntu tar xzf /backup/es-backup-20231201.tar.gz -C /
```

---

## ?? Troubleshooting

### Container Fails to Start

**Check logs**:
```bash
docker logs elasticsearch
```

**Common errors**:

1. **vm.max_map_count too low**
   ```powershell
   wsl -d docker-desktop sysctl -w vm.max_map_count=262144
   ```

2. **Port already in use**
   ```powershell
   netstat -ano | findstr :9200
   ```
   Modify port in docker-compose.yml

3. **Insufficient memory**
   Lower JVM heap:
   ```yaml
   - "ES_JAVA_OPTS=-Xms512m -Xmx512m"
   ```

### Cannot Connect

1. Check if container is running:
   ```bash
   docker ps
   ```

2. Check firewall:
   ```powershell
   New-NetFirewallRule -DisplayName "Elasticsearch" -Direction Inbound -Protocol TCP -LocalPort 9200 -Action Allow
   ```

3. Wait for service to fully start (about 30-60 seconds)

### Performance Issues

1. Increase Docker Desktop resources:
   - Settings ¡ú Resources
   - Recommended: 4GB RAM, 2 CPU cores

2. Optimize Elasticsearch configuration

3. Use SSD storage

---

## ?? Security Recommendations

### Development Environment (Current Configuration)

- ? No authentication required, convenient for development
- ?? For local development only
- ? Do not expose to public internet

### Production Environment

Security features should be enabled:

1. Enable X-Pack Security
2. Set strong passwords
3. Enable SSL/TLS
4. Configure firewall
5. Regular backups

---

## ?? Related Documentation

- [Complete Deployment Guide](../docs/ELASTICSEARCH_DEPLOYMENT_GUIDE.md)
- [Quick Start](../docs/QUICK_START.md)
- [Client Usage Guide](../docs/APP_NAME_UNIQUE_USAGE.md)

---

## ?? Tips

- Use Docker Compose for long-term operation
- Use PowerShell script for development and debugging
- Use batch script for quick operations
- Regular backups of important data
- Monitor resource usage

---

## ?? Getting Help

If you have issues:

1. View logs: `docker logs elasticsearch`
2. Check status: `docker ps`
3. View complete documentation
4. Use test functionality to verify configuration

---

**Happy Searching! ??**
