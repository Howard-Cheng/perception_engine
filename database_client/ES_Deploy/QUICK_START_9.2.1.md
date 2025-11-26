# 🚀 Elasticsearch 9.2.1 - Quick 3-Step Deployment

## Features

- 📦 **Version:** Elasticsearch 9.2.1
- 🌐 **Address:** localhost (127.0.0.1) only
- 🔌 **Port:** 9200
- 📡 **Protocol:** HTTP
- 🔓 **Authentication:** None (development mode)

---

## Step 1: Deploy

**Double-click to run:**
`
deploy_elasticsearch_dev.bat
`

This will download, extract and configure Elasticsearch.

**Time:** 5-10 minutes

---

## Step 2: Start

**Double-click to run:**
`
start_elasticsearch_dev.bat
`

**Time:** 30-60 seconds

---

## Step 3: Test

**Open in browser:**
`
http://localhost:9200
`

**Or run test script:**
`
test_elasticsearch_dev.ps1
`

**Expected output:**
`json
{
  "cluster_name" : "elasticsearch-dev",
  "version" : { "number" : "9.2.1" }
}
`

---

## ✅ Done!

Now you can use it in your C++ application:

`ini
# config.ini
[Database]
elasticsearch_url=http://localhost:9200
elasticsearch_index=perception_context
`

Start your application:
`powershell
cd D:\PerceiptionEngine_Howard\perception_engine\windows_code\buildnew
.\PerceptionEngine.exe --console
`

---

## 📚 Detailed Documentation

See `README_ES_9.2.1_DEV.md`

---

## 🛑 Stop

**Double-click to run:**
`
stop_elasticsearch_dev.ps1
`

---

**Configuration:**
- Bind address: 127.0.0.1 ✓
- No authentication: ✓
- HTTP only: ✓
- Development mode: ✓
