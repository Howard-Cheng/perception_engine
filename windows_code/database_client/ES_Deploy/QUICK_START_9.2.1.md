# ?? Elasticsearch 9.2.1 - 3步快速部署

## 特点

- ? **版本:** Elasticsearch 9.2.1
- ? **地址:** localhost (127.0.0.1) only
- ? **端口:** 9200
- ? **协议:** HTTP
- ? **认证:** 无需 (开发模式)

---

## 第 1 步: 部署

**双击运行:**
```
deploy_elasticsearch_dev.bat
```

这会下载、解压并配置 Elasticsearch。

**时间:** 5-10 分钟

---

## 第 2 步: 启动

**双击运行:**
```
start_elasticsearch_dev.bat
```

**时间:** 30-60 秒

---

## 第 3 步: 测试

**在浏览器打开:**
```
http://localhost:9200
```

**或运行测试脚本:**
```
test_elasticsearch_dev.ps1
```

**期望看到:**
```json
{
  "cluster_name" : "elasticsearch-dev",
  "version" : { "number" : "9.2.1" }
}
```

---

## ? 完成！

现在可以在 C++ 应用中使用：

```ini
# config.ini
[Database]
elasticsearch_url=http://localhost:9200
elasticsearch_index=perception_context
```

启动应用：
```powershell
cd D:\PerceiptionEngine_Howard\perception_engine\windows_code\buildnew
.\PerceptionEngine.exe --console
```

---

## ?? 详细文档

查看 `README_ES_9.2.1_DEV.md`

---

## ?? 停止

**双击运行:**
```
stop_elasticsearch_dev.ps1
```

---

**配置:**
- 绑定地址: 127.0.0.1 ?
- 无需认证: ?
- HTTP only: ?
- 开发模式: ?
