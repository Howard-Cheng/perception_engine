using Microsoft.Extensions.Logging;
using System.Runtime.CompilerServices;

namespace PerceptionMcpBridge;

/// <summary>
/// Helper class for logging with automatic caller information (file name and line number)
/// </summary>
internal static class LogHelper
{
    public static void LogInformationWithCaller(
        this ILogger logger,
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogInformation($"[{fileName}:{lineNumber}] {message}");
    }

    public static void LogInformationWithCaller(
        this ILogger logger,
        string message,
        object arg1,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogInformation($"[{fileName}:{lineNumber}] {message}", arg1);
    }

    public static void LogInformationWithCaller(
        this ILogger logger,
        string message,
        object arg1,
        object arg2,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogInformation($"[{fileName}:{lineNumber}] {message}", arg1, arg2);
    }

    public static void LogErrorWithCaller(
        this ILogger logger,
        Exception ex,
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogError(ex, $"[{fileName}:{lineNumber}] {message}");
    }

    public static void LogWarningWithCaller(
        this ILogger logger,
        Exception ex,
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogWarning(ex, $"[{fileName}:{lineNumber}] {message}");
    }

    public static void LogDebugWithCaller(
        this ILogger logger,
        string message,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogDebug($"[{fileName}:{lineNumber}] {message}");
    }

    public static void LogDebugWithCaller(
        this ILogger logger,
        string message,
        object arg1,
        [CallerFilePath] string filePath = "",
        [CallerLineNumber] int lineNumber = 0)
    {
        var fileName = Path.GetFileName(filePath);
        logger.LogDebug($"[{fileName}:{lineNumber}] {message}", arg1);
    }
}
