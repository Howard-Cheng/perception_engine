# 📘 Elasticsearch 9.2.1 Development Deployment Guide

## 🎯 Overview

This is an Elasticsearch 9.2.1 deployment script optimized for development environments. Key features:

- 📦 **Version:** Elasticsearch 9.2.1
- 🔒 **Bind Address:** 127.0.0.1 (localhost only)
- 📡 **Protocol:** HTTP (no SSL)
- 🔓 **Authentication:** Disabled (no username/password)
- ⚡ **Automation:** Auto-download and configuration

## 🚀 Quick Start

### Step 1: Deploy Elasticsearch

**Double-click to run:**
`
deploy_elasticsearch_dev.bat
`

**Using PowerShell:**
`powershell
.\deploy_elasticsearch_dev.ps1
`

This script will:
1. Check Java version (requires Java 17+)
2. Download Elasticsearch 9.2.1 (~400 MB)
3. Extract to current directory
4. Auto-configure for development mode
   - Bind to localhost (127.0.0.1)
   - Disable security settings
   - Single-node mode

**Time:** 5-10 minutes (depending on network speed)

### Step 2: Start Elasticsearch

**Double-click to run:**
`
start_elasticsearch_dev.bat
`

**Using PowerShell:**
`powershell
.\start_elasticsearch_dev.ps1
`

**Startup time:** 30-60 seconds (first time)

### Step 3: Verify Connection

**Double-click to run:**
`
test_elasticsearch_dev.ps1
`

**Or open in browser:**
`
http://localhost:9200
`

**Using PowerShell:**
`powershell
Invoke-WebRequest http://localhost:9200
`

**Expected output:**
`json
{
  "name" : "node-1",
  "cluster_name" : "elasticsearch-dev",
  "version" : {
    "number" : "9.2.1"
  },
  "tagline" : "You Know, for Search"
}
`

---

## 📋 File Description

| File | Purpose |
|------|---------|
| `deploy_elasticsearch_dev.ps1` | Deployment script (main) |
| `deploy_elasticsearch_dev.bat` | Deployment script (double-click to run) |
| `start_elasticsearch_dev.ps1` | Start script |
| `start_elasticsearch_dev.bat` | Start script (double-click to run) |
| `stop_elasticsearch_dev.ps1` | Stop script |
| `test_elasticsearch_dev.ps1` | Test script |

---

## ⚙️ Configuration

### Auto-generated Configuration (elasticsearch.yml)

`yaml
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
`

### Key Configuration Explanation

- **`network.host: 127.0.0.1`** - Bind to localhost only, cannot be accessed from network
- **`discovery.type: single-node`** - Single-node mode, no cluster configuration
- **`xpack.security.enabled: false`** - Disable security, no authentication required

---

## 🔗 C++ Application Integration

### Configure config.ini

`ini
[Database]
elasticsearch_url=http://localhost:9200
elasticsearch_index=perception_context
# No username/password needed
`

### Start Application

`powershell
cd D:\PerceiptionEngine_Howard\perception_engine\windows_code\buildnew
.\PerceptionEngine.exe --console
`

### Check Logs

`
[INFO] ✓ Elasticsearch initialized - auto storage every 5 seconds
`

---

## ✅ Verification Checklist

After deployment, confirm the following:

- [ ] Java 17+ installed
- [ ] Elasticsearch 9.2.1 downloaded and extracted
- [ ] Configuration file updated (localhost binding)
- [ ] Elasticsearch started successfully
- [ ] Port 9200 listening
- [ ] HTTP connection successful (http://localhost:9200)
- [ ] Cluster health status is green or yellow
- [ ] C++ application can connect

---

## 🔧 Troubleshooting

### Issue 1: Java Not Detected

**Symptom:**
`
❌ Java not detected
`

**Solution:**
`powershell
# Check Java version
java -version

# If not installed, download Java 17+
# https://adoptium.net/
`

### Issue 2: Port 9200 Already in Use

**Symptom:**
`
Port 9200 already in use
`

**Solution:**
`powershell
# Check which process is using the port
Get-NetTCPConnection -LocalPort 9200

# Stop old ES instance
.\stop_elasticsearch_dev.ps1

# Or stop other process using port 9200
`

### Issue 3: Download Failed

**Symptom:**
`
Download failed: Connection timeout
`

**Solution:**
1. Check network connection
2. Manually download using browser:
   `
   https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-9.2.1-windows-x86_64.zip
   `
3. Place ZIP file in script directory
4. Re-run script, it will automatically detect and use downloaded file

### Issue 4: Service Not Responding

**Troubleshooting steps:**

`powershell
# 1. Check process
Get-Process java | Where-Object { $_.WorkingSet64 -gt 100MB }

# 2. Check port
Get-NetTCPConnection -LocalPort 9200

# 3. View logs
Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 50

# 4. Check configuration
Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml | Select-String "network.host"
`

**Expected output:**
`
network.host: 127.0.0.1
`

### Issue 5: Bound to Wrong Address

**Symptom:**
Logs show `publish_address {198.18.0.1:9200}` instead of `127.0.0.1`

**Solution:**
`powershell
# 1. Stop ES
.\stop_elasticsearch_dev.ps1

# 2. Re-deploy (will regenerate configuration)
.\deploy_elasticsearch_dev.ps1

# 3. Verify configuration
Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml | Select-String "network"

# 4. Restart
.\start_elasticsearch_dev.ps1
`

---

## 📊 Performance Metrics

- **Memory usage:** ~1-2 GB (default)
- **Disk space:** ~500 MB (installation) + data size
- **Startup time:** 30-60 seconds
- **Response time:** < 100ms (localhost)

---

## ⚠️ Important Notes

### For Development Use Only

This configuration is **NOT suitable for production** because:

- 🔓 Security disabled
- 🔓 No authentication
- 🔓 Localhost only access
- 🔓 Single-node mode (no high availability)

### For Production Deployment

You should enable:

- 🔒 HTTPS/SSL
- 🔒 User authentication (username/password)
- 🔒 Use cluster mode
- 🔒 Configure firewall rules
- 🔒 Enable backup rotation
- 🔒 Set up log monitoring

---

## 📝 Common Commands

### Service Management

`powershell
# Start
.\start_elasticsearch_dev.ps1

# Stop
.\stop_elasticsearch_dev.ps1

# Test
.\test_elasticsearch_dev.ps1

# View logs
Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 50 -Wait
`

### API Examples

`powershell
# Cluster info
Invoke-WebRequest http://localhost:9200

# Cluster health
Invoke-WebRequest http://localhost:9200/_cluster/health

# List indices
Invoke-WebRequest http://localhost:9200/_cat/indices?v

# Cluster statistics
Invoke-WebRequest http://localhost:9200/_cluster/stats
`

### Data Operations

`powershell
# Create document
$body = '{\"message\":\"Hello ES 9.2.1\",\"timestamp\":\"2024-01-XX\"}' 
Invoke-WebRequest -Method POST http://localhost:9200/test/_doc -ContentType "application/json" -Body $body

# Search
Invoke-WebRequest -Method POST http://localhost:9200/test/_search -ContentType "application/json" -Body '{\"query\":{\"match_all\":{}}}'

# Delete index
Invoke-WebRequest -Method DELETE http://localhost:9200/test
`

---

## 📚 Learning Resources

- [Elasticsearch Official Documentation](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/index.html)
- [REST API Reference](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/rest-apis.html)
- [Query DSL](https://www.elastic.co/guide/en/elasticsearch/reference/9.2/query-dsl.html)

---

## 📅 Version History

### Version 2.0 (2024-01-XX)
- 🆕 Upgrade to Elasticsearch 9.2.1
- 🔒 Force bind to localhost (127.0.0.1)
- ⚡ Automated download and configuration
- 📝 Improved progress monitoring
- 🧪 Enhanced test scripts

### Version 1.0
- Initial version (Elasticsearch 8.12.1)

---

## 🆘 Getting Help

If you encounter issues:

1. **View logs:**
   `powershell
   Get-Content .\elasticsearch-9.2.1\logs\elasticsearch.log -Tail 100
   `

2. **Run tests:**
   `powershell
   .\test_elasticsearch_dev.ps1
   `

3. **Check configuration:**
   `powershell
   Get-Content .\elasticsearch-9.2.1\config\elasticsearch.yml
   `

4. **Complete clean reinstall:**
   `powershell
   .\stop_elasticsearch_dev.ps1
   Remove-Item .\elasticsearch-9.2.1 -Recurse -Force
   .\deploy_elasticsearch_dev.ps1
   `

---

**Last Updated:** 2024-01-XX  
**Elasticsearch Version:** 9.2.1  
**Status:** ✅ Ready for development use (not for production)
