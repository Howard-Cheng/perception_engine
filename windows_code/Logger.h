#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <windows.h>

// Log levels - Using _L suffix to avoid Windows macro conflicts
enum class LogLevel {
    TRACE_L = 0,
    DEBUG_L = 1,
    INFO_L = 2,
    WARN_L = 3,
    ERROR_L = 4,
    FATAL_L = 5
};

// Logger class - Thread-safe singleton
class Logger {
public:
    static Logger& GetInstance();

    // Initialize logger with log file path
    void Initialize(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO_L);

    // Set minimum log level
    void SetMinLevel(LogLevel level);

    // Log functions with file and line info
    void Log(LogLevel level, const char* file, int line, const std::string& message);

    // Format log with printf-style formatting
    template<typename... Args>
    void LogFormat(LogLevel level, const char* file, int line, const char* format, Args... args) {
        char buffer[4096];
        snprintf(buffer, sizeof(buffer), format, args...);
        Log(level, file, line, std::string(buffer));
    }

    // Flush log to disk
    void Flush();

    // Close log file
    void Shutdown();

    // Check if initialized
    bool IsInitialized() const { return initialized; }

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string GetTimestamp();
    std::string GetLogLevelString(LogLevel level);
    std::string ExtractFileName(const char* fullPath);

    std::ofstream logFile;
    std::mutex logMutex;
    LogLevel minLogLevel;
    bool initialized;
    bool enableConsole;  // Also output to console
};

// Convenience macros for logging with automatic file and line
#define LOG_TRACE(msg) Logger::GetInstance().Log(LogLevel::TRACE_L, __FILE__, __LINE__, msg)
#define LOG_DEBUG(msg) Logger::GetInstance().Log(LogLevel::DEBUG_L, __FILE__, __LINE__, msg)
#define LOG_INFO(msg)  Logger::GetInstance().Log(LogLevel::INFO_L,  __FILE__, __LINE__, msg)
#define LOG_WARN(msg)  Logger::GetInstance().Log(LogLevel::WARN_L,  __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) Logger::GetInstance().Log(LogLevel::ERROR_L, __FILE__, __LINE__, msg)
#define LOG_FATAL(msg) Logger::GetInstance().Log(LogLevel::FATAL_L, __FILE__, __LINE__, msg)

// Format versions with printf-style formatting
#define LOG_TRACE_FMT(fmt, ...) Logger::GetInstance().LogFormat(LogLevel::TRACE_L, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) Logger::GetInstance().LogFormat(LogLevel::DEBUG_L, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...)  Logger::GetInstance().LogFormat(LogLevel::INFO_L,  __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)  Logger::GetInstance().LogFormat(LogLevel::WARN_L,  __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) Logger::GetInstance().LogFormat(LogLevel::ERROR_L, __FILE__, __LINE__, fmt, __VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...) Logger::GetInstance().LogFormat(LogLevel::FATAL_L, __FILE__, __LINE__, fmt, __VA_ARGS__)
