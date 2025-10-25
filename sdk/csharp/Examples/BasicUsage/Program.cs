using System;
using System.Linq;
using System.Threading.Tasks;
using PerceptionEngine.SDK;
using PerceptionEngine.SDK.Exceptions;
using PerceptionEngine.SDK.Models;

namespace PerceptionEngine.Examples
{
    class Program
    {
        static async Task Main(string[] args)
        {
            Console.WriteLine("==============================================");
            Console.WriteLine("  Perception Engine SDK - Basic Usage Example");
            Console.WriteLine("==============================================");
            Console.WriteLine();

            // Create client (use 127.0.0.1 instead of localhost to avoid IPv6 issues)
            using var client = new PerceptionEngineClient("http://127.0.0.1:8777");

            // Check if engine is running
            Console.WriteLine("Checking if Perception Engine is running...");
            if (!await client.IsHealthyAsync())
            {
                Console.WriteLine("❌ Perception Engine is not running!");
                Console.WriteLine("Please start it with:");
                Console.WriteLine("  cd windows_code");
                Console.WriteLine("  .\\start_perception_engine.bat");
                return;
            }

            Console.WriteLine("✅ Perception Engine is running!");
            Console.WriteLine();

            try
            {
                // Get current context
                Console.WriteLine("Fetching current context...");
                var context = await client.GetContextAsync();

                // Display mode
                var mode = context.GetEngineMode();
                Console.WriteLine($"Engine Mode: {mode}");
                Console.WriteLine();

                // Display system info
                Console.WriteLine("=== System Information ===");
                Console.WriteLine($"Active App:     {context.ActiveApp}");
                Console.WriteLine($"CPU Usage:      {context.CpuUsage:F1}%");
                Console.WriteLine($"Memory Usage:   {context.MemoryUsage:F1}%");
                Console.WriteLine($"Memory Used:    {context.MemoryUsedGB:F2} GB / {context.TotalMemoryGB:F2} GB");

                if (context.Battery.HasValue)
                {
                    var chargingStatus = context.IsCharging ? "(Charging)" : "";
                    Console.WriteLine($"Battery:        {context.Battery}% {chargingStatus}");
                }

                Console.WriteLine($"Network:        {(context.NetworkConnected ? context.NetworkType : "Disconnected")}");

                if (context.LocationValid)
                {
                    Console.WriteLine($"Location:       {context.LocationLat}, {context.LocationLon}");
                }

                Console.WriteLine();

                // Display perception data (if available)
                if (mode == EngineMode.Full)
                {
                    Console.WriteLine("=== Perception Data ===");

                    // Voice
                    if (!string.IsNullOrEmpty(context.VoiceTranscription))
                    {
                        Console.WriteLine($"Voice:          \"{context.VoiceTranscription}\"");
                        Console.WriteLine($"Voice Latency:  {context.VoiceLatency:F1} ms");
                    }
                    else
                    {
                        Console.WriteLine("Voice:          No speech detected");
                    }

                    // Camera
                    if (!string.IsNullOrEmpty(context.CameraDescription))
                    {
                        Console.WriteLine($"Camera:         \"{context.CameraDescription}\"");
                        Console.WriteLine($"Camera Latency: {context.CameraLatency} ms");
                    }
                    else
                    {
                        Console.WriteLine("Camera:         Waiting for input...");
                    }

                    Console.WriteLine();
                }
                else
                {
                    Console.WriteLine("=== Running in Screen-Only Mode ===");
                    Console.WriteLine("Voice and camera features are disabled.");
                    Console.WriteLine();
                }

                // Display active app content
                if (!string.IsNullOrEmpty(context.ActiveAppContent))
                {
                    Console.WriteLine("=== Active App Content ===");
                    var preview = context.ActiveAppContent.Length > 200
                        ? context.ActiveAppContent.Substring(0, 200) + "..."
                        : context.ActiveAppContent;
                    Console.WriteLine(preview);
                    Console.WriteLine();
                }

                // Display recent apps
                if (context.RecentPeriodActiveApps.Count > 0)
                {
                    Console.WriteLine("=== Recent Applications ===");
                    foreach (var app in context.RecentPeriodActiveApps.Take(5))
                    {
                        Console.WriteLine($"  {app.AppName,-30} {app.TotalSeconds,5}s");

                        // Show sessions for the first app
                        if (app == context.RecentPeriodActiveApps.First() && app.Sessions.Count > 0)
                        {
                            Console.WriteLine($"    Sessions:");
                            foreach (var session in app.Sessions.Take(3))
                            {
                                Console.WriteLine($"      - {session.WindowTitle,-40} ({session.DurationSeconds}s)");
                            }
                        }
                    }
                    Console.WriteLine();
                }

                // Display fused context
                Console.WriteLine("=== Fused Context ===");
                Console.WriteLine(context.FusedContext);
                Console.WriteLine();

                // Display performance metrics
                Console.WriteLine("=== Performance Metrics ===");
                Console.WriteLine($"Context Update Latency: {context.ContextUpdateLatency:F2} ms");
                if (mode == EngineMode.Full)
                {
                    if (context.VoiceLatency.HasValue)
                        Console.WriteLine($"Voice Latency:          {context.VoiceLatency:F1} ms");
                    if (context.CameraLatency.HasValue)
                        Console.WriteLine($"Camera Latency:         {context.CameraLatency} ms");
                }
                Console.WriteLine();

                // Display timestamp
                Console.WriteLine($"Timestamp: {context.Timestamp}");
            }
            catch (PerceptionEngineException ex)
            {
                Console.WriteLine($"❌ Error: {ex.Message}");
                if (ex.InnerException != null)
                {
                    Console.WriteLine($"   Details: {ex.InnerException.Message}");
                }
            }

            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}
