using System.Text.Json;
using System.Text.Json.Serialization;
using System.ComponentModel;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using ModelContextProtocol;
using ModelContextProtocol.Protocol;
using ModelContextProtocol.Server;
using Serilog;
using Serilog.Events;

namespace PerceptionMcpBridge;

internal static class Program
{
    private const string PerceptionEngineServerName = "perception-engine-context";
    private static readonly Uri PerceptionEngineBaseUri = new("http://localhost:8777/", UriKind.Absolute);

    private static async Task Main(string[] args)
    {
        // Configure Serilog before building the host
        var logDirectory = Path.Combine(AppContext.BaseDirectory, "logs");
        Directory.CreateDirectory(logDirectory);
        
        var logFilePath = Path.Combine(logDirectory, "mcp-server-.txt");
        
        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Information()
            .MinimumLevel.Override("Microsoft", LogEventLevel.Warning)
            .MinimumLevel.Override("System", LogEventLevel.Warning)
            .MinimumLevel.Override("ModelContextProtocol", LogEventLevel.Warning)
            .Enrich.FromLogContext()
            .Enrich.WithThreadId()
            .WriteTo.File(
                logFilePath,
                rollingInterval: RollingInterval.Day,
                outputTemplate: "[{Timestamp:yyyy-MM-dd HH:mm:ss.fff}] [{Level:u3}] [{SourceContext}] {Message:lj}{NewLine}{Exception}",
                retainedFileCountLimit: 7)
            .CreateLogger();

        try
        {
            Log.Information("=== MCP Server Starting ===");
            Log.Information("Server Name: {ServerName}", PerceptionEngineServerName);
            Log.Information("Perception Engine URI: {EngineUri}", PerceptionEngineBaseUri);
            Log.Information("Log Directory: {LogDirectory}", logDirectory);
            
            var builder = Host.CreateApplicationBuilder(args);
            builder.Services.AddSerilog();

            builder.Services.AddHttpClient<PerceptionContextClient>(client =>
            {
                client.BaseAddress = PerceptionEngineBaseUri;
                client.Timeout = TimeSpan.FromSeconds(5);
            });

            Log.Information("Registering MCP Server with tools...");
            
            builder.Services
                .AddMcpServer(options =>
                {
                    options.ServerInfo = new Implementation
                    {
                        Name = PerceptionEngineServerName,
                        Version = "0.1.0"
                    };
                    options.ServerInstructions = "Proxy perception context data from the native engine.";
                    
                    Log.Information("MCP Server configured: {Name} v{Version}", 
                        options.ServerInfo.Name, 
                        options.ServerInfo.Version);
                })
                .WithTools<PerceptionTools>()
                .WithStdioServerTransport();

            Log.Information("MCP Server registration completed successfully");
            Log.Information("Tools registered: get_perception_context, query_perception_history");
            Log.Information("Transport: stdio (stdin/stdout)");

            using var host = builder.Build();
            
            Log.Information("=== MCP Server Started Successfully ===");
            Log.Information("Waiting for MCP client connections via stdio...");
            
            await host.RunAsync();
        }
        catch (Exception ex)
        {
            Log.Fatal(ex, "MCP Server failed to start");
            throw;
        }
        finally
        {
            Log.Information("=== MCP Server Shutting Down ===");
            await Log.CloseAndFlushAsync();
        }
    }
}
    
internal sealed class PerceptionTools
{
    private readonly PerceptionContextClient _contextClient;
    private readonly ILogger<PerceptionTools> _logger;

    public PerceptionTools(PerceptionContextClient contextClient, ILogger<PerceptionTools> logger)
    {
        _contextClient = contextClient;
        _logger = logger;
    }

    [Description("Get real-time system context from Perception Engine including: " +
        "Active application and window, Voice transcriptions (speech-to-text), " +
        "Camera vision descriptions, System metrics (CPU, memory, battery), " +
        "Network status, Recent applications history, " +
        "Active app content (text from current window), Performance metrics (latencies). " +
        "This provides a comprehensive snapshot of what the user is currently doing on their computer.")]
    [McpServerTool(
        Name = "get_perception_context",
        Title = "Get Perception Context",
        Idempotent = true,
        ReadOnly = true)]
    public async Task<CallToolResult> GetPerceptionContextAsync(CancellationToken cancellationToken)
    {
        _logger.LogInformationWithCaller("Tool 'get_perception_context' called");
        
        try
        {
            _logger.LogDebugWithCaller("Requesting context from Perception Engine");
            
            var formatted = await _contextClient.GetFormattedContextAsync(cancellationToken);

            _logger.LogInformationWithCaller("Successfully retrieved perception context, length: {Length} chars", 
                formatted.Length);

            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock { Text = formatted }
                }
            };
        }
        catch (HttpRequestException ex)
        {
            _logger.LogErrorWithCaller(ex, 
                "Perception Engine HTTP request failed. Is the service running?");
            
            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock
                    {
                        Text = "Error: Perception Engine service timeout. Is the service running?"
                    }
                },
                IsError = true
            };
        }
        catch (Exception ex)
        {
            _logger.LogErrorWithCaller(ex, 
                "Unexpected error in get_perception_context");
            
            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock
                    {
                        Text = $"Error getting perception context: {ex.Message}"
                    }
                },
                IsError = true
            };
        }
    }

    [Description("Query historical perception events by keyword. Search through past screen content, " +
        "application names, and other captured data to find relevant events within a time range. " +
        "Returns matching events with timestamps, application info, screen content, and system metrics.")]
    [McpServerTool(
        Name = "query_perception_history",
        Title = "Query Perception History",
        Idempotent = true,
        ReadOnly = true)]
    public async Task<CallToolResult> QueryPerceptionHistoryAsync(
        [Description("Keyword to search for in screen content and application names")] string keyword,
        [Description("Number of hours to look back in history (e.g., 24 for last 24 hours), default=24")] int hours,
        [Description("Maximum number of results to return")] int maxcount,
        CancellationToken cancellationToken)
    {
        _logger.LogInformationWithCaller("Tool 'query_perception_history' called with keyword='{Keyword}', hours={Hours}, maxcount={MaxCount}", 
            keyword, hours);
        
        try
        {
            _logger.LogDebugWithCaller("Querying perception history from Perception Engine");
            
            var formatted = await _contextClient.QueryPerceptionHistoryAsync(keyword, hours, maxcount, cancellationToken);

            _logger.LogInformationWithCaller("Successfully retrieved perception history, length: {Length} chars", 
                formatted.Length);

            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock { Text = formatted }
                }
            };
        }
        catch (HttpRequestException ex)
        {
            _logger.LogErrorWithCaller(ex, 
                "Perception Engine HTTP request failed. Is the service running?");
            
            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock
                    {
                        Text = "Error: Perception Engine service timeout. Is the service running?"
                    }
                },
                IsError = true
            };
        }
        catch (Exception ex)
        {
            _logger.LogErrorWithCaller(ex, 
                "Unexpected error in query_perception_history");
            
            return new CallToolResult
            {
                Content = new List<ContentBlock>
                {
                    new TextContentBlock
                    {
                        Text = $"Error querying perception history: {ex.Message}"
                    }
                },
                IsError = true
            };
        }
    }
}

internal sealed class PerceptionContextClient
{
    private readonly HttpClient _httpClient;
    private readonly ILogger<PerceptionContextClient> _logger;

    public PerceptionContextClient(HttpClient httpClient, ILogger<PerceptionContextClient> logger)
    {
        _httpClient = httpClient;
        _logger = logger;
    }

    public async Task<string> GetFormattedContextAsync(CancellationToken cancellationToken)
    {
        _logger.LogDebugWithCaller("Fetching context from {Uri}", _httpClient.BaseAddress + "context");
        
        var json = await _httpClient.GetStringAsync("context", cancellationToken);
        
        _logger.LogDebugWithCaller("Received JSON response, length: {Length} bytes", json.Length);
        
        return FormatContext(json);
    }

    public async Task<string> QueryPerceptionHistoryAsync(string keyword, int hours, int maxcount, CancellationToken cancellationToken)
    {
        var queryUrl = $"query?keyword={Uri.EscapeDataString(keyword)}&hours={hours}&maxcount={maxcount}";
        _logger.LogDebugWithCaller("Fetching query results from {Uri}", _httpClient.BaseAddress + queryUrl);
        
        var json = await _httpClient.GetStringAsync(queryUrl, cancellationToken);
        
        _logger.LogDebugWithCaller("Received JSON response, length: {Length} bytes", json.Length);
        
        return FormatQueryResults(json);
    }

    private string FormatContext(string rawJson)
    {
        try
        {
            _logger.LogDebugWithCaller("Parsing and formatting context JSON");
            
            using var document = JsonDocument.Parse(rawJson, new JsonDocumentOptions
            {
                AllowTrailingCommas = true
            });

            var root = document.RootElement;
            var lines = new List<string> { "# Perception Engine Context", "" };

            // Fused Context Summary
            if (root.TryGetProperty("fusedContext", out var fusedProp))
            {
                var fusedContext = fusedProp.GetString();
                if (!string.IsNullOrWhiteSpace(fusedContext))
                {
                    lines.Add($"**Summary:** {fusedContext}");
                    lines.Add("");
                }
            }

            // Active Application
            if (root.TryGetProperty("activeApp", out var activeAppProp))
            {
                var activeApp = activeAppProp.GetString();
                if (!string.IsNullOrWhiteSpace(activeApp))
                {
                    lines.Add($"**Active Application:** {activeApp}");
                }
            }

            // Active App Content (if available)
            if (root.TryGetProperty("activeAppContent", out var activeAppContentProp))
            {
                var content = activeAppContentProp.GetString();
                if (!string.IsNullOrWhiteSpace(content))
                {
                    if (content.Length > 500)
                    {
                        content = content.Substring(0, 500) + "...";
                    }
                    lines.Add("**Active App Content:**");
                    lines.Add($"```\n{content}\n```");
                    lines.Add("");
                }
            }

            // Voice Transcription
            if (root.TryGetProperty("voiceTranscription", out var voiceProp))
            {
                var voice = voiceProp.GetString();
                if (!string.IsNullOrWhiteSpace(voice))
                {
                    lines.Add($"**Voice:** \"{voice}\"");
                    if (root.TryGetProperty("voiceLatency", out var voiceLatencyProp))
                    {
                        lines.Add($"  (Latency: {voiceLatencyProp.GetDouble():F0}ms)");
                    }
                    lines.Add("");
                }
            }

            // Camera Vision
            if (root.TryGetProperty("cameraDescription", out var cameraProp))
            {
                var camera = cameraProp.GetString();
                if (!string.IsNullOrWhiteSpace(camera))
                {
                    lines.Add($"**Camera Vision:** {camera}");
                    if (root.TryGetProperty("cameraLatency", out var cameraLatencyProp))
                    {
                        var latencySec = cameraLatencyProp.GetDouble() / 1000.0;
                        lines.Add($"  (Latency: {latencySec:F1}s)");
                    }
                    lines.Add("");
                }
            }

            // System Metrics
            lines.Add("## System Status");
            if (root.TryGetProperty("cpuUsage", out var cpuProp))
            {
                lines.Add($"- **CPU:** {cpuProp.GetDouble():F1}%");
            }
            if (root.TryGetProperty("memoryUsage", out var memProp))
            {
                var memUsed = root.TryGetProperty("memoryUsedGB", out var memUsedProp) ? memUsedProp.GetDouble() : 0;
                var memTotal = root.TryGetProperty("totalMemoryGB", out var memTotalProp) ? memTotalProp.GetDouble() : 0;
                lines.Add($"- **Memory:** {memUsed:F1} / {memTotal:F1} GB ({memProp.GetDouble():F1}%)");
            }
            if (root.TryGetProperty("battery", out var batteryProp))
            {
                var battery = batteryProp.GetString();
                var charging = root.TryGetProperty("isCharging", out var chargingProp) && chargingProp.GetBoolean() ? " (Charging)" : "";
                lines.Add($"- **Battery:** {battery}%{charging}");
            }
            if (root.TryGetProperty("networkConnected", out var networkProp))
            {
                var networkType = root.TryGetProperty("networkType", out var networkTypeProp) ? networkTypeProp.GetString() : "Unknown";
                var status = networkProp.GetBoolean() ? $"Connected ({networkType})" : "Offline";
                lines.Add($"- **Network:** {status}");
            }

            // Recent Apps
            if (root.TryGetProperty("RecentPeriodActiveApps", out var recentAppsProp) && recentAppsProp.ValueKind == JsonValueKind.Array)
            {
                var recentApps = recentAppsProp.EnumerateArray().Take(5).ToList();
                if (recentApps.Count > 0)
                {
                    lines.Add("");
                    lines.Add("## Recent Applications");
                    foreach (var app in recentApps)
                    {
                        var appName = app.TryGetProperty("appName", out var appNameProp) ? appNameProp.GetString() : "Unknown";
                        var duration = app.TryGetProperty("durationSeconds", out var durationProp) ? durationProp.GetInt32() : 0;
                        lines.Add($"- {appName} ({duration}s)");
                    }
                }
            }

            // Performance Metrics
            lines.Add("");
            lines.Add("## Performance Metrics");
            if (root.TryGetProperty("contextUpdateLatency", out var contextLatencyProp))
            {
                lines.Add($"- Context Update: {contextLatencyProp.GetDouble():F0}ms");
            }
            if (root.TryGetProperty("voiceLatency", out var perfVoiceLatencyProp))
            {
                lines.Add($"- Voice Processing: {perfVoiceLatencyProp.GetDouble():F0}ms");
            }

            // Timestamp
            if (root.TryGetProperty("timestamp", out var timestampProp))
            {
                var timestamp = timestampProp.GetString();
                if (!string.IsNullOrWhiteSpace(timestamp))
                {
                    lines.Add("");
                    lines.Add($"*Updated: {timestamp}*");
                }
            }

            return string.Join("\n", lines);
        }
        catch (Exception ex)
        {
            _logger.LogWarningWithCaller(ex, "Failed to parse context JSON, returning raw data");
            return "# Perception Engine Context\n\nRaw JSON:\n" + rawJson;
        }
    }

    private string FormatQueryResults(string rawJson)
    {
        try
        {
            _logger.LogDebugWithCaller("Parsing and formatting query results JSON");
            
            using var document = JsonDocument.Parse(rawJson, new JsonDocumentOptions
            {
                AllowTrailingCommas = true
            });

            var root = document.RootElement;
            var lines = new List<string> { "# Perception History Query Results", "" };

            // Summary
            if (root.TryGetProperty("totalHits", out var totalHitsProp))
            {
                var totalHits = totalHitsProp.GetInt32();
                lines.Add($"**Total Hits:** {totalHits}");
            }

            if (root.TryGetProperty("searchTimeMs", out var searchTimeProp))
            {
                var searchTime = searchTimeProp.GetInt32();
                lines.Add($"**Search Time:** {searchTime}ms");
            }

            lines.Add("");

            // Results
            if (root.TryGetProperty("results", out var resultsProp) && resultsProp.ValueKind == JsonValueKind.Array)
            {
                var results = resultsProp.EnumerateArray().ToList();
                
                if (results.Count == 0)
                {
                    lines.Add("No results found.");
                }
                else
                {
                    lines.Add($"## Events ({results.Count} results)");
                    lines.Add("");

                    for (int i = 0; i < results.Count; i++)
                    {
                        var result = results[i];
                        
                        lines.Add($"### Event {i + 1}");
                        
                        // Event ID
                        if (result.TryGetProperty("eventId", out var eventIdProp))
                        {
                            lines.Add($"**Event ID:** {eventIdProp.GetString()}");
                        }

                        // Timestamp
                        if (result.TryGetProperty("timestamp", out var timestampProp))
                        {
                            var timestamp = timestampProp.GetInt64();
                            var dateTime = DateTimeOffset.FromUnixTimeSeconds(timestamp).ToLocalTime();
                            lines.Add($"**Timestamp:** {dateTime:yyyy-MM-dd HH:mm:ss}");
                        }

                        // Device ID
                        if (result.TryGetProperty("deviceId", out var deviceIdProp))
                        {
                            lines.Add($"**Device ID:** {deviceIdProp.GetString()}");
                        }

                        // App Name
                        if (result.TryGetProperty("appName", out var appNameProp))
                        {
                            var appName = appNameProp.GetString();
                            if (!string.IsNullOrWhiteSpace(appName))
                            {
                                lines.Add($"**Application:** {appName}");
                            }
                        }

                        // Screen Content
                        if (result.TryGetProperty("screenContent", out var screenContentProp))
                        {
                            var screenContent = screenContentProp.GetString();
                            if (!string.IsNullOrWhiteSpace(screenContent))
                            {
                                // Truncate if too long
                                if (screenContent.Length > 500)
                                {
                                    screenContent = screenContent.Substring(0, 500) + "...";
                                }
                                lines.Add("**Screen Content:**");
                                lines.Add($"```\n{screenContent}\n```");
                            }
                        }

                        // System Info
                        if (result.TryGetProperty("systemInfo", out var systemInfoProp))
                        {
                            lines.Add("**System Info:**");
                            
                            if (systemInfoProp.TryGetProperty("cpuUsage", out var cpuProp))
                            {
                                lines.Add($"  - CPU: {cpuProp.GetDouble():F1}%");
                            }
                            
                            if (systemInfoProp.TryGetProperty("memoryUsage", out var memProp))
                            {
                                lines.Add($"  - Memory: {memProp.GetDouble():F1}%");
                            }
                            
                            if (systemInfoProp.TryGetProperty("batteryPercent", out var batteryProp))
                            {
                                var battery = batteryProp.GetInt32();
                                var charging = systemInfoProp.TryGetProperty("isCharging", out var chargingProp) && chargingProp.GetBoolean() 
                                    ? " (Charging)" : "";
                                lines.Add($"  - Battery: {battery}%{charging}");
                            }
                            
                            if (systemInfoProp.TryGetProperty("networkType", out var networkProp))
                            {
                                lines.Add($"  - Network: {networkProp.GetString()}");
                            }
                        }

                        lines.Add("");
                    }
                }
            }
            else
            {
                lines.Add("No results found.");
            }

            return string.Join("\n", lines);
        }
        catch (Exception ex)
        {
            _logger.LogWarningWithCaller(ex, "Failed to parse query results JSON, returning raw data");
            return "# Perception History Query Results\n\nRaw JSON:\n" + rawJson;
        }
    }
}
