# Elasticsearch Deployment and Usage Complete Guide

This documentation set provides a complete solution for deploying Elasticsearch on Windows and using the C++ client.

---

## ?? Quick Navigation

### I want to...

- **Quick 5-minute experience** ¡ú [Quick Start](QUICK_START.md)
- **Complete production deployment** ¡ú [Complete Deployment Guide](ELASTICSEARCH_DEPLOYMENT_GUIDE.md)  
- **Learn C++ API** ¡ú [Client Usage Guide](APP_NAME_UNIQUE_USAGE.md)
- **Use deployment tools** ¡ú [Docker Tools Documentation](../docker/README.md)
- **View all documentation** ¡ú [Documentation Index](INDEX.md)

---

## ?? Documentation List

| Document | Time | Difficulty | Description |
|------|------|------|------|
| [QUICK_START.md](QUICK_START.md) | 5 min | ? | Fastest deployment method |
| [ELASTICSEARCH_DEPLOYMENT_GUIDE.md](ELASTICSEARCH_DEPLOYMENT_GUIDE.md) | 30 min | ??? | Detailed deployment and configuration guide |
| [APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md) | 15 min | ?? | C++ client API usage |
| [docker/README.md](../docker/README.md) | 10 min | ?? | Deployment tools usage guide |
| [INDEX.md](INDEX.md) | - | ? | Complete documentation index |

---

## ?? Three Steps to Start

### Step 1: Install WSL
```powershell
# Administrator PowerShell
wsl --install
# Restart computer
```

### Step 2: Install Docker
1. Download: https://www.docker.com/products/docker-desktop/
2. Install and restart

### Step 3: Start Elasticsearch
```powershell
docker run -d `
  --name elasticsearch `
  -p 9200:9200 `
  -e "discovery.type=single-node" `
  -e "xpack.security.enabled=false" `
  docker.elastic.co/elasticsearch/elasticsearch:8.11.0
```

Verify: Visit http://localhost:9200

---

## ??? Deployment Tools

We provide three deployment tools, choose the one that suits you best:

### 1. Batch Script (Simplest)
```batch
cd docker
elasticsearch-manager.bat
```
- ? Graphical menu
- ? One-click operation
- ? Suitable for quick use

### 2. PowerShell Script (Recommended)
```powershell
cd docker
.\Deploy-Elasticsearch.ps1 -Action start
```
- ? Full-featured
- ? Automation friendly
- ? Suitable for development and debugging

### 3. Docker Compose (Production Grade)
```bash
cd docker
docker compose up -d
```
- ? Flexible configuration
- ? Easy to extend
- ? Suitable for production environment

See: [docker/README.md](../docker/README.md)

---

## ?? C++ Client Usage

### Basic Connection
```cpp
#include "ElasticsearchClient.h"

elasticsearch::ElasticsearchClient client("http://localhost:9200");

if (client.testConnection()) {
    std::cout << "Connection successful!" << std::endl;
}
```

### Insert Data (Auto Deduplication)
```cpp
RawEvent event;
event.appName = "Chrome";  // Used as unique identifier
event.interactionCount = 10;
event.timestamp = std::time(nullptr);

// First insert
client.indexDocument("my_index", event);

// Insert same app_name again - automatically updates instead of creating new entry
event.interactionCount = 20;
client.indexDocument("my_index", event);  // Updates existing data
```

### Query Data
```cpp
// Get directly by app_name
RawEvent chrome = client.getDocumentByAppName("my_index", "Chrome");

// Search query
json query = {{"query", {{"match_all", json::object()}}}};
SearchResult result = client.search("my_index", query.dump());
```

See: [APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md)

---

## ?? Core Features

### Auto Deduplication Mechanism
Uses `app_name` as document ID, same application automatically updates instead of creating duplicates:

```cpp
// First time
event.appName = "Chrome";
client.indexDocument("index", event);  // Create

// Second time (same app_name)
event.appName = "Chrome";  
client.indexDocument("index", event);  // Update, not create new entry
```

### Complete System Information
Supports storing rich event data:
- Mouse events
- System information (CPU, memory, battery, etc.)
- Multimodal data (voice, image descriptions)
- Content classification and domain classification

### High Performance Batch Operations
```cpp
std::vector<RawEvent> events = {...};
client.bulkIndexDocuments("index", events);
```

---

## ?? Service Access

After startup, you can access:

| Service | URL | Description |
|------|------|------|
| Elasticsearch API | http://localhost:9200 | REST API |
| Cluster Health | http://localhost:9200/_cluster/health | Status monitoring |
| Index List | http://localhost:9200/_cat/indices?v | View all indices |
| Kibana (Optional) | http://localhost:5601 | Visualization interface |

---

## ?? Test Examples

Run test programs to verify deployment:

```bash
# Compile and run
cd build
.\es_client_test.exe

# Or run uniqueness test
.\test_app_name_unique.exe
```

Test code located at:
- `tests/es_client_test.cpp`
- `tests/test_app_name_unique.cpp`
- `tests/es_performance_test.cpp`

---

## ?? Command Quick Reference

### Docker Management
```bash
# Start
docker start elasticsearch

# Stop
docker stop elasticsearch

# Restart
docker restart elasticsearch

# View logs
docker logs -f elasticsearch

# View status
docker ps
docker stats elasticsearch
```

### Elasticsearch API
```bash
# Cluster health
curl http://localhost:9200/_cluster/health?pretty

# List indices
curl http://localhost:9200/_cat/indices?v

# Create index
curl -X PUT http://localhost:9200/my-index

# Delete index
curl -X DELETE http://localhost:9200/my-index

# Search
curl http://localhost:9200/my-index/_search?pretty
```

---

## ?? Common Issues

### Container fails to start?
```powershell
# Set vm.max_map_count
wsl -d docker-desktop sysctl -w vm.max_map_count=262144

# View logs
docker logs elasticsearch
```

### Port already in use?
```powershell
# Check usage
netstat -ano | findstr :9200

# Use different port
docker run ... -p 9201:9200 ...
```

### Cannot connect?
1. Ensure container is running: `docker ps`
2. Wait for service to start (about 30 seconds)
3. Check firewall settings
4. Verify address: http://localhost:9200

See "Troubleshooting" sections in respective documents.

---

## ?? Learning Path

### ?? Beginner Level (1 hour)
1. ? Read [QUICK_START.md](QUICK_START.md)
2. ? Deploy Elasticsearch
3. ? Test connection
4. ? Run sample code

### ?? Intermediate Level (3 hours)
1. ? Study [APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md)
2. ? Understand auto deduplication mechanism
3. ? Implement data insertion and queries
4. ? Use batch operations

### ?? Advanced Level (8+ hours)
1. ? Read [Complete Deployment Guide](ELASTICSEARCH_DEPLOYMENT_GUIDE.md)
2. ? Configure production environment
3. ? Optimize performance parameters
4. ? Implement monitoring and backup

---

## ?? Project Structure

```
perception_engine/
©¦
©À©¤©¤ docs/                    # ?? All documentation
©¦   ©À©¤©¤ README.md           # This file
©¦   ©À©¤©¤ INDEX.md            # Documentation index
©¦   ©À©¤©¤ QUICK_START.md      # Quick start
©¦   ©À©¤©¤ ELASTICSEARCH_DEPLOYMENT_GUIDE.md  # Complete guide
©¦   ©¸©¤©¤ APP_NAME_UNIQUE_USAGE.md          # API usage
©¦
©À©¤©¤ docker/                  # ?? Deployment tools
©¦   ©À©¤©¤ README.md           # Tools documentation
©¦   ©À©¤©¤ docker-compose.yml  # Compose configuration
©¦   ©À©¤©¤ Deploy-Elasticsearch.ps1  # PowerShell script
©¦   ©¸©¤©¤ elasticsearch-manager.bat # Batch script
©¦
©À©¤©¤ include/                 # ?? Header files
©¦   ©À©¤©¤ ElasticsearchClient.h
©¦   ©¸©¤©¤ ElasticsearchTypes.h
©¦
©À©¤©¤ src/                     # ?? Source code
©¦   ©¸©¤©¤ ElasticsearchClient.cpp
©¦
©¸©¤©¤ tests/                   # ?? Test code
    ©À©¤©¤ test_app_name_unique.cpp
    ©À©¤©¤ es_client_test.cpp
    ©¸©¤©¤ es_performance_test.cpp
```

---

## ?? External Resources

- [Elasticsearch Official Documentation](https://www.elastic.co/guide/en/elasticsearch/reference/current/index.html)
- [Docker Official Documentation](https://docs.docker.com/)
- [WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)
- [nlohmann/json Documentation](https://json.nlohmann.me/)
- [libcurl Documentation](https://curl.se/libcurl/)

---

## ?? Best Practices

### Development Environment
- ? Disable security authentication (for easier debugging)
- ? Use appropriate memory configuration (1-2GB)
- ? Regularly clean test data
- ? Use Docker volumes for data persistence

### Production Environment
- ? Enable X-Pack Security
- ? Configure SSL/TLS
- ? Regular data backups
- ? Monitor resource usage
- ? Use load balancing

### Code Practices
- ? Use app_name as unique identifier
- ? Batch operations improve performance
- ? Proper exception handling
- ? Test connection before executing operations

---

## ?? Getting Help

### Problem Diagnosis Process

1. **Check service status**
   ```powershell
   docker ps
   .\docker\Deploy-Elasticsearch.ps1 -Action status
   ```

2. **View logs**
   ```bash
   docker logs elasticsearch
   ```

3. **Run tests**
   ```powershell
   .\docker\Deploy-Elasticsearch.ps1 -Action test
   ```

4. **Consult documentation**
   - Check troubleshooting sections in respective documents
   - Refer to [INDEX.md](INDEX.md) to find relevant documentation

5. **Community support**
   - Check GitHub Issues
   - Elasticsearch official forums

---

## ?? Changelog

### 2024-12 
- ? Added app_name uniqueness mechanism
- ?? Complete deployment documentation
- ??? Three deployment tools
- ?? Complete test examples
- ?? Bilingual documentation support

---

## ?? Get Started

Choose your starting point:

- **I'm a beginner** ¡ú [QUICK_START.md](QUICK_START.md)
- **I want to dive deep** ¡ú [ELASTICSEARCH_DEPLOYMENT_GUIDE.md](ELASTICSEARCH_DEPLOYMENT_GUIDE.md)
- **I want to develop applications** ¡ú [APP_NAME_UNIQUE_USAGE.md](APP_NAME_UNIQUE_USAGE.md)
- **I want to see everything** ¡ú [INDEX.md](INDEX.md)

---

**Enjoy using it! Feel free to consult documentation or submit Issues if you have questions. ??**

*Perception Engine Team*
