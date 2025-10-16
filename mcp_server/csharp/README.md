# Perception Engine MCP Server (C# Implementation)

A C# implementation of the Model Context Protocol (MCP) server that provides the **same functionality** as the Python version (`perception_context_server.py`). This server acts as a bridge between the Perception Engine and Claude Desktop, exposing real-time system context through MCP tools.

## Features

This C# MCP Server provides identical functionality to the Python version:

- ✅ Real-time system context retrieval
- ✅ Active application and window information
- ✅ Voice transcriptions (speech-to-text)
- ✅ Camera vision descriptions
- ✅ System metrics (CPU, memory, battery)
- ✅ Network status
- ✅ Recent applications history
- ✅ Active app content (text from current window)
- ✅ Performance metrics (latencies)

## Technology Stack

- **.NET 8.0** - Modern cross-platform framework
- **ModelContextProtocol SDK** - Official C# MCP SDK ([GitHub](https://github.com/modelcontextprotocol/csharp-sdk))
- **Microsoft.Extensions.Hosting** - Dependency injection and hosted services
- **HttpClient** - Communication with Perception Engine

## Prerequisites

1. **.NET 8.0 SDK** or later
   - Download: https://dotnet.microsoft.com/download/dotnet/8.0
   - Verify: `dotnet --version`

2. **Perception Engine** running locally
   - Must be accessible at `http://localhost:8777`
   - Test: Open `http://localhost:8777/context` in your browser

## Quick Start

### Build and Run

```powershell
# Navigate to the C# project directory
cd mcp_server\csharp

# Restore dependencies and build
dotnet restore
dotnet build

# Run the server (for testing)
dotnet run
```

### Publish as Executable

```powershell
# Publish as single-file (requires .NET runtime)
dotnet publish -c Release -r win-x64 --self-contained false -o ./publish

# Or publish as self-contained (includes .NET runtime)
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o ./publish-standalone
```

### Using Helper Scripts

**Windows Batch:**
```powershell
.\run_mcp_server.bat
```

**PowerShell (Interactive Menu):**
```powershell
.\build.ps1
```

## Claude Desktop Configuration

Edit your Claude Desktop config file at: `%APPDATA%\Claude\claude_desktop_config.json`

### Option 1: Development Mode (dotnet run)

```json
{
  "mcpServers": {
    "perception-engine": {
      "command": "dotnet",
      "args": [
        "run",
        "--project",
        "d:\\PerceiptionEngine_Howard\\perception_engine\\mcp_server\\csharp\\PerceptionMcpBridge.csproj"
      ]
    }
  }
}
```

### Option 2: Published Executable (Recommended)

```json
{
  "mcpServers": {
    "perception-engine": {
      "command": "d:\\PerceiptionEngine_Howard\\perception_engine\\mcp_server\\csharp\\publish\\PerceptionMcpBridge.exe",
      "args": []
    }
  }
}
```

**Note:** Update paths to match your installation. Restart Claude Desktop after editing.


## Code Architecture

### Program.cs Structure

```
Program.Main()
  ├── Configure HttpClient (connect to Perception Engine)
  ├── Configure MCP Server (server info, tools)
  ├── Use Stdio transport
  └── Run hosted service

PerceptionTools
  └── GetPerceptionContextAsync()  // The MCP tool

PerceptionContextClient
  ├── GetFormattedContextAsync()  // Fetch and format context
  └── FormatContext()             // Format JSON to Markdown

StdioMcpHostedService
  └── ExecuteAsync()  // Run MCP server loop
```

### Key Implementation Details

The C# implementation provides **identical functionality** to the Python version:

1. **Tool Registration:** Uses `[McpServerTool]` attribute instead of `@app.list_tools()` decorator
2. **HTTP Communication:** Uses dependency-injected `HttpClient` with 5-second timeout
3. **Output Formatting:** Implements the same Markdown formatting logic as Python:
   - Fused Context Summary
   - Active Application
   - Active App Content (truncated to 500 characters)
   - Voice Transcription (with latency)
   - Camera Vision (with latency)
   - System Status (CPU, Memory, Battery, Network)
   - Recent Applications (top 5)
   - Performance Metrics
   - Timestamp

## Advantages Over Python

1. **Better Performance** - Compiled language, faster startup, lower runtime overhead
2. **Type Safety** - Compile-time type checking reduces runtime errors
3. **Lower Resource Usage** - Less memory and CPU usage compared to Python
4. **Easy Distribution** - Can be compiled to single-file executable, no runtime installation needed
5. **Windows Native** - Better integration with Windows systems

## Troubleshooting

### Issue: "Connection refused"

**Cause:** Perception Engine is not running

**Solution:**
```powershell
cd d:\PerceiptionEngine_Howard\perception_engine\windows_code
.\start_perception_engine.bat
```

### Issue: "dotnet is not recognized"

**Cause:** .NET SDK not installed or not in PATH

**Solution:**
1. Install .NET 8.0 SDK
2. Restart terminal
3. Verify: `dotnet --version`

### Issue: Claude Desktop doesn't recognize the tool

**Cause:** Config file format error

**Solution:**
1. Check JSON syntax
2. Use double backslashes `\\` or forward slashes `/` in paths
3. Restart Claude Desktop

### Issue: MCP Server exits immediately

**Cause:** stdio transport requires launching by Claude Desktop

**Solution:**
- Don't test by running directly in terminal
- Must be launched through Claude Desktop config
- Use MCP Inspector for standalone testing

## Development

### Adding New Tools

```csharp
internal sealed class PerceptionTools
{
    [Description("Your tool description")]
    [McpServerTool(Name = "your_tool_name")]
    public async Task<CallToolResult> YourNewToolAsync(
        [Description("Parameter description")] string parameter,
        CancellationToken cancellationToken)
    {
        // Implementation
        return new CallToolResult
        {
            Content = new List<ContentBlock>
            {
                new TextContentBlock { Text = "Result" }
            }
        };
    }
}
```

### Modifying Output Format

Edit the `FormatContext` method in `Program.cs`:

```csharp
private static string FormatContext(string rawJson)
{
    using var document = JsonDocument.Parse(rawJson);
    var root = document.RootElement;
    
    // Extract fields
    var myField = root.TryGetProperty("myField", out var prop) 
        ? prop.GetString() 
        : "default";
    
    // Format output
    return $"# Title\n\n**Field:** {myField}";
}
```

## Files

- `Program.cs` - Main implementation (all code in one file)
- `PerceptionMcpBridge.csproj` - Project configuration
- `README.md` - English documentation (this file)
- `README_CN.md` - Chinese documentation
- `run_mcp_server.bat` - Quick start batch script
- `build.ps1` - Interactive build/publish PowerShell script
- `claude_desktop_config_example.json` - Configuration examples

## References

- [MCP C# SDK Official Documentation](https://github.com/modelcontextprotocol/csharp-sdk)
- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [.NET 8.0 Documentation](https://learn.microsoft.com/dotnet/core/whats-new/dotnet-8)

## Version History

- **0.1.0** (2025-01-15)
  - Initial release
  - Full implementation of Python version features
  - Support for all context fields with formatting

## License

Same as Perception Engine project

## Author

Howard Cheng

---

**Note:** This C# version provides the **exact same functionality** as `perception_context_server.py` and can be used interchangeably.


## Notes

- The example uses `HttpClient.GetStringAsync` with a 5-second timeout. Adjust accordingly for your environment.
- If Perception Engine is unavailable or returns errors, the tool returns an MCP error (`isError = true`) with the exception message.
- Extend `FormatContext` to match the exact JSON structure you expect from Perception Engine.
- The project references **ModelContextProtocol 0.4.0-preview.2**, which is currently the latest SDK release. If NuGet prompts for pre-release consent during restore, accept it.
