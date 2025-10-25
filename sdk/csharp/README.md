# Perception Engine C# SDK

Official C# SDK for integrating with the Perception Engine - a real-time multi-modal AI context monitoring system.

## Quick Start

### 1. Build the SDK

```powershell
cd sdk\csharp
dotnet build PerceptionEngine.SDK.sln --configuration Release
```

### 2. Run the Example

```powershell
# Make sure Perception Engine is running first!
cd ..\..\windows_code
.\start_perception_engine.bat

# Then run the example (in a new terminal)
cd ..\sdk\csharp\Examples\BasicUsage
dotnet run
```

## SDK Structure

```
sdk/csharp/
├── PerceptionEngine.SDK/              # Main SDK library
│   ├── PerceptionEngineClient.cs      # HTTP client
│   ├── Models/                        # Data models
│   │   ├── PerceptionContext.cs
│   │   ├── ActiveAppRecord.cs
│   │   └── EngineMode.cs
│   ├── Exceptions/
│   │   └── PerceptionEngineException.cs
│   └── README.md                      # Full API documentation
│
├── Examples/
│   └── BasicUsage/                    # Example console app
│       ├── Program.cs
│       └── BasicUsage.csproj
│
└── PerceptionEngine.SDK.sln           # Visual Studio solution
```

## Installation (For Other Teams)

Once published to NuGet, other teams can install with:

```powershell
dotnet add package PerceptionEngine.SDK
```

Or manually add to `.csproj`:

```xml
<ItemGroup>
  <PackageReference Include="PerceptionEngine.SDK" Version="1.0.0" />
</ItemGroup>
```

## Building a NuGet Package

To create a `.nupkg` file for distribution:

```powershell
cd sdk\csharp\PerceptionEngine.SDK
dotnet pack --configuration Release
```

The package will be created in:
```
bin\Release\PerceptionEngine.SDK.1.0.0.nupkg
```

### Publishing to NuGet (Optional)

```powershell
# Get API key from nuget.org
dotnet nuget push bin\Release\PerceptionEngine.SDK.1.0.0.nupkg --api-key YOUR_API_KEY --source https://api.nuget.org/v3/index.json
```

### Using Local NuGet Package (For Testing)

```powershell
# Add local package source
dotnet nuget add source "C:\path\to\sdk\csharp\PerceptionEngine.SDK\bin\Release" --name "Local-PerceptionEngine"

# Install from local source
dotnet add package PerceptionEngine.SDK --source "Local-PerceptionEngine"
```

## Usage Example

```csharp
using PerceptionEngine.SDK;

// Create client
using var client = new PerceptionEngineClient("http://localhost:8777");

// Check if engine is running
if (await client.IsHealthyAsync())
{
    // Get current context
    var context = await client.GetContextAsync();

    // Access strongly-typed properties
    Console.WriteLine($"Active App: {context.ActiveApp}");
    Console.WriteLine($"CPU: {context.CpuUsage}%");

    // Check for voice (null in screen-only mode)
    if (context.VoiceTranscription != null)
    {
        Console.WriteLine($"User said: \"{context.VoiceTranscription}\"");
    }

    // Iterate through recent apps
    foreach (var app in context.RecentPeriodActiveApps)
    {
        Console.WriteLine($"{app.AppName}: {app.TotalSeconds}s");
    }
}
```

## API Reference

See `PerceptionEngine.SDK/README.md` for complete API documentation.

### Quick Reference

**PerceptionEngineClient:**
- `GetContextAsync()` - Get current context
- `IsHealthyAsync()` - Check if engine is running
- `WaitForHealthyAsync()` - Wait for engine to start
- `GetEngineModeAsync()` - Get Full vs Screen-Only mode

**PerceptionContext:**
- `ActiveApp` - Current application
- `CpuUsage`, `MemoryUsage` - System metrics
- `VoiceTranscription` - Voice data (null in screen-only mode)
- `CameraDescription` - Camera data (null in screen-only mode)
- `RecentPeriodActiveApps` - Recent app usage
- `GetEngineMode()` - Detect Full vs Screen-Only
- `IsScreenOnlyMode()` - Returns true if screen-only

## Testing

### Prerequisites

1. **Perception Engine must be running:**
   ```powershell
   cd windows_code
   .\start_perception_engine.bat
   ```

2. **Verify it's running:**
   ```powershell
   curl http://localhost:8777/context
   ```

### Run Example

```powershell
cd sdk\csharp\Examples\BasicUsage
dotnet run
```

**Expected Output:**
```
==============================================
  Perception Engine SDK - Basic Usage Example
==============================================

Checking if Perception Engine is running...
✅ Perception Engine is running!

Fetching current context...
Engine Mode: Full

=== System Information ===
Active App:     chrome.exe
CPU Usage:      25.3%
Memory Usage:   65.2%
...
```

## Requirements

- **.NET 6.0 SDK or later**
- **Perception Engine** running on http://localhost:8777

### Install .NET 6.0 SDK

Download from: https://dotnet.microsoft.com/download/dotnet/6.0

Verify installation:
```powershell
dotnet --version
# Should show: 6.0.x or later
```

## Troubleshooting

### Error: "Failed to connect to Perception Engine"

**Cause:** Perception Engine is not running.

**Solution:**
```powershell
cd windows_code
.\start_perception_engine.bat
```

### Error: "The command could not be loaded"

**Cause:** .NET SDK not installed.

**Solution:** Install .NET 6.0 SDK from https://dotnet.microsoft.com/download

### Build Errors

```powershell
# Clean and rebuild
dotnet clean
dotnet build --configuration Release
```

## Contributing

When adding new features to the SDK:

1. Update models in `Models/PerceptionContext.cs`
2. Add XML documentation comments
3. Update `README.md` with examples
4. Test with both Full and Screen-Only modes
5. Increment version in `.csproj`

## License

MIT License - See LICENSE file for details

## Support

For issues or questions:
- GitHub Issues: https://github.com/your-repo/perception-engine/issues
- Email: support@perception-engine.com
