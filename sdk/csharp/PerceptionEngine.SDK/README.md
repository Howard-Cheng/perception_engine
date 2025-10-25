# Perception Engine SDK for C#

A simple, strongly-typed C# SDK for interacting with the Perception Engine HTTP API.

## Installation

```bash
dotnet add package PerceptionEngine.SDK
```

Or add to your `.csproj`:

```xml
<ItemGroup>
  <PackageReference Include="PerceptionEngine.SDK" Version="1.0.0" />
</ItemGroup>
```

## Quick Start

```csharp
using PerceptionEngine.SDK;

// Create client
var client = new PerceptionEngineClient("http://localhost:8777");

// Get current context
var context = await client.GetContextAsync();

// Access strongly-typed properties
Console.WriteLine($"Active App: {context.ActiveApp}");
Console.WriteLine($"CPU Usage: {context.CpuUsage}%");

// Check for voice data (null in screen-only mode)
if (context.VoiceTranscription != null)
{
    Console.WriteLine($"User said: \"{context.VoiceTranscription}\"");
}

// Dispose when done
client.Dispose();
```

## Features

- ✅ **Strongly-typed models** - No more JSON parsing!
- ✅ **Async/await support** - Modern C# async patterns
- ✅ **Automatic mode detection** - Detects Full vs Screen-Only mode
- ✅ **Health checks** - Verify engine is running
- ✅ **Timeout handling** - Configurable request timeouts
- ✅ **Exception handling** - Clear error messages

## API Reference

### PerceptionEngineClient

**Constructor:**
```csharp
var client = new PerceptionEngineClient(
    baseUrl: "http://localhost:8777",  // Default
    timeout: TimeSpan.FromSeconds(10)   // Default
);
```

**Methods:**

#### GetContextAsync()
Gets the current perception context.

```csharp
PerceptionContext context = await client.GetContextAsync();
```

**Returns:** `PerceptionContext` with all current context data.

**Throws:** `PerceptionEngineException` if request fails.

---

#### IsHealthyAsync()
Checks if the Perception Engine is running.

```csharp
bool isRunning = await client.IsHealthyAsync();
if (isRunning)
{
    Console.WriteLine("Engine is running!");
}
```

**Returns:** `true` if engine responds, `false` otherwise.

---

#### WaitForHealthyAsync()
Waits for the engine to become available (useful during startup).

```csharp
bool ready = await client.WaitForHealthyAsync(
    timeout: TimeSpan.FromSeconds(30),      // Default
    pollInterval: TimeSpan.FromSeconds(1)   // Default
);
```

**Returns:** `true` if engine became available, `false` if timeout.

---

#### GetEngineModeAsync()
Detects whether the engine is running in Full or Screen-Only mode.

```csharp
EngineMode mode = await client.GetEngineModeAsync();

if (mode == EngineMode.ScreenOnly)
{
    Console.WriteLine("Running in lightweight screen-only mode");
}
else
{
    Console.WriteLine("Running in full mode with audio/camera");
}
```

**Returns:** `EngineMode.Full` or `EngineMode.ScreenOnly`.

---

### PerceptionContext

The main data model returned by `GetContextAsync()`.

**Properties:**

| Property | Type | Description |
|----------|------|-------------|
| `ActiveApp` | `string` | Currently active application (e.g., "chrome.exe") |
| `CpuUsage` | `double?` | CPU usage percentage (0-100) |
| `MemoryUsage` | `double?` | Memory usage percentage (0-100) |
| `MemoryUsedGB` | `double?` | Memory used in GB |
| `TotalMemoryGB` | `double?` | Total memory in GB |
| `Battery` | `int?` | Battery percentage (null if no battery) |
| `IsCharging` | `bool` | Whether device is charging |
| `NetworkConnected` | `bool` | Whether connected to network |
| `NetworkType` | `string` | Network type (WiFi, Ethernet, etc.) |
| `LocationLat` | `double?` | GPS latitude (if available) |
| `LocationLon` | `double?` | GPS longitude (if available) |
| `LocationValid` | `bool` | Whether location data is valid |
| `VoiceTranscription` | `string?` | Voice transcription (null in screen-only mode) |
| `VoiceLatency` | `double?` | Voice processing latency in ms |
| `CameraDescription` | `string?` | Camera scene description (null in screen-only mode) |
| `CameraLatency` | `int?` | Camera processing latency in ms |
| `ContextUpdateLatency` | `double?` | Context update latency in ms |
| `ActiveAppContent` | `string?` | Content from active app window |
| `RecentPeriodActiveApps` | `List<ActiveAppRecord>` | Recently used applications |
| `FusedContext` | `string` | Human-readable context summary |
| `Timestamp` | `string` | ISO 8601 timestamp |

**Helper Methods:**

```csharp
// Check engine mode
EngineMode mode = context.GetEngineMode();

// Check if screen-only
bool isLightweight = context.IsScreenOnlyMode();
```

## Complete Examples

### Example 1: Basic Usage

```csharp
using PerceptionEngine.SDK;

var client = new PerceptionEngineClient();

try
{
    var context = await client.GetContextAsync();

    Console.WriteLine($"Active: {context.ActiveApp}");
    Console.WriteLine($"CPU: {context.CpuUsage}%");
    Console.WriteLine($"Memory: {context.MemoryUsage}%");
}
catch (PerceptionEngineException ex)
{
    Console.WriteLine($"Error: {ex.Message}");
}
finally
{
    client.Dispose();
}
```

### Example 2: Continuous Monitoring

```csharp
using PerceptionEngine.SDK;

var client = new PerceptionEngineClient();

// Wait for engine to start
if (!await client.WaitForHealthyAsync())
{
    Console.WriteLine("Engine failed to start!");
    return;
}

// Monitor every 2 seconds
while (true)
{
    var context = await client.GetContextAsync();

    Console.Clear();
    Console.WriteLine($"Active App: {context.ActiveApp}");
    Console.WriteLine($"CPU: {context.CpuUsage:F1}%");
    Console.WriteLine($"Memory: {context.MemoryUsage:F1}%");

    // Show voice if available
    if (!string.IsNullOrEmpty(context.VoiceTranscription))
    {
        Console.WriteLine($"\nUser said: \"{context.VoiceTranscription}\"");
    }

    // Show recent apps
    Console.WriteLine("\nRecent Apps:");
    foreach (var app in context.RecentPeriodActiveApps.Take(5))
    {
        Console.WriteLine($"  {app.AppName}: {app.TotalSeconds}s");
    }

    await Task.Delay(2000);
}
```

### Example 3: Handling Both Modes

```csharp
using PerceptionEngine.SDK;

var client = new PerceptionEngineClient();
var context = await client.GetContextAsync();

// Detect mode
EngineMode mode = context.GetEngineMode();
Console.WriteLine($"Engine Mode: {mode}");

if (mode == EngineMode.ScreenOnly)
{
    // Only use screen data
    Console.WriteLine($"Active: {context.ActiveApp}");
    Console.WriteLine($"CPU: {context.CpuUsage}%");
}
else
{
    // Use all features
    Console.WriteLine($"Active: {context.ActiveApp}");
    Console.WriteLine($"Voice: {context.VoiceTranscription ?? "No speech"}");
    Console.WriteLine($"Camera: {context.CameraDescription ?? "No input"}");
}
```

### Example 4: With Dependency Injection (ASP.NET Core)

```csharp
// In Startup.cs or Program.cs
services.AddSingleton<PerceptionEngineClient>(sp =>
    new PerceptionEngineClient("http://localhost:8777"));

// In your service/controller
public class MyService
{
    private readonly PerceptionEngineClient _perceptionClient;

    public MyService(PerceptionEngineClient perceptionClient)
    {
        _perceptionClient = perceptionClient;
    }

    public async Task<string> GetUserActivity()
    {
        var context = await _perceptionClient.GetContextAsync();
        return $"{context.ActiveApp} - {context.FusedContext}";
    }
}
```

## Error Handling

The SDK throws `PerceptionEngineException` when requests fail:

```csharp
try
{
    var context = await client.GetContextAsync();
}
catch (PerceptionEngineException ex)
{
    // Common errors:
    // - "Failed to connect to Perception Engine at http://localhost:8777. Is it running?"
    // - "Request to Perception Engine timed out"
    // - "Failed to parse response from Perception Engine"

    Console.WriteLine($"Error: {ex.Message}");
}
```

## Best Practices

1. **Dispose the client when done:**
   ```csharp
   using var client = new PerceptionEngineClient();
   // ... use client
   // Automatically disposed
   ```

2. **Check health before making requests:**
   ```csharp
   if (await client.IsHealthyAsync())
   {
       var context = await client.GetContextAsync();
   }
   ```

3. **Handle both engine modes:**
   ```csharp
   if (context.IsScreenOnlyMode())
   {
       // Don't try to access voice/camera data
   }
   ```

4. **Use cancellation tokens for long-running operations:**
   ```csharp
   var cts = new CancellationTokenSource();
   var context = await client.GetContextAsync(cts.Token);
   ```

## Requirements

- **.NET 6.0 or later**
- **Perception Engine running** on the specified URL (default: http://localhost:8777)

## License

MIT License - See LICENSE file for details

## Support

For issues or questions, visit: https://github.com/your-repo/perception-engine/issues
