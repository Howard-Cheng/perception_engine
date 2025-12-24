# PerceptionEngine Web Query Interface

## ?? Overview

这是一个用于查询 PerceptionEngine 历史数据的网页界面。

## ?? Configuration

### Server Connection
- **Server URL**: `http://localhost:8777`
- **Port**: `8777` (默认端口)
- **API Endpoint**: `/query`

### Connection Issues

如果显示 "Connection failed - Server may be offline"，请检查：

1. **确认 PerceptionEngine 正在运行**
   ```bash
   # 启动 console 模式
   PerceptionEngine.exe --console
   
   # 或启动为服务
   PerceptionEngine.exe --start
   ```

2. **检查端口是否被占用**
   ```bash
   netstat -ano | findstr :8777
   ```

3. **确认数据库已初始化**
   - PerceptionEngine 必须成功连接到 PostgreSQL
   - 查看日志中是否有 "PostgreSQL initialized" 消息

## ?? Usage

### 查询参数

1. **Keyword (关键词)**
   - 搜索的关键词
   - 支持中文和英文
   - 使用模糊匹配

2. **Hours (小时数)**
   - 查询过去 N 小时的数据
   - 默认值: 24 小时

3. **Max Count (最大数量)**
   - 返回结果的最大数量
   - 默认值: 100

### 按钮功能

- **?? Send Query**: 发送查询请求
- **??? Clear Results**: 清除结果显示区域

### 快捷键

- **Enter**: 发送查询请求

## ?? Features

### UI Features
- ? 实时连接状态指示
- ? 自动 JSON 格式化和语法高亮
- ? 滚动条样式定制
- ? 错误信息友好提示
- ? 加载状态动画

### Technical Features
- ? CORS 支持
- ? URL 编码
- ? 输入验证
- ? 异常处理
- ? 30秒自动重连检测

## ?? API Format

### Request Format
```
GET /query?keyword={keyword}&hours={hours}&max={maxResults}
```

### Response Format
```json
{
  "totalHits": 100,
  "searchTimeMs": 15,
  "results": [
    {
      "eventId": "device_123456_1234567890_123",
      "timestamp": 1234567890,
      "deviceId": "device_123456",
      "appName": "chrome.exe",
      "windowTitle": "Example Page",
      "screenContent": "...",
      "mouseEvents": [...]
    }
  ]
}
```

## ?? Troubleshooting

### Common Issues

1. **"Connection failed"**
   - 确保 PerceptionEngine 正在运行
   - 检查端口 8777 是否开放
   - 查看防火墙设置

2. **"Database not available"**
   - PostgreSQL 未运行
   - 数据库连接字符串配置错误
   - 表未正确初始化

3. **"Query failed"**
   - 关键词为空
   - 参数格式错误
   - 服务器内部错误（查看日志）

### Debug Mode

在浏览器中按 F12 打开开发者工具，查看：
- Network 标签页：HTTP 请求详情
- Console 标签页：JavaScript 错误信息

## ?? Related Files

- `perception_query.html` - 主网页文件
- `../src/core/PerceptionEngine.cpp` - 后端实现
- `../src/communication/HttpServer.cpp` - HTTP 服务器
- `../src/context/ContextCollector.cpp` - 数据收集器

## ?? Security Notes

- Server 仅绑定到 `127.0.0.1` (本地访问)
- 不支持远程连接
- 无需认证（本地开发用途）

## ?? License

Part of PerceptionEngine Project
