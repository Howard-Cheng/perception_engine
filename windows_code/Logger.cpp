#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <cstring>

// Singleton instance
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : minLogLevel(LogLevel::INFO_L)  // Changed from INFO to INFO_L
    , initialized(false)
    , enableConsole(true) {
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Initialize(const std::string& logFilePath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (initialized) {
        return;  // Already initialized
    }

    minLogLevel = minLevel;

    // Create log directory if it doesn't exist
    std::filesystem::path logPath(logFilePath);
    std::filesystem::path logDir = logPath.parent_path();

    if (!logDir.empty() && !std::filesystem::exists(logDir)) {
        std::filesystem::create_directories(logDir);
    }

    // Open log file in append mode
    logFile.open(logFilePath, std::ios::out | std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << logFilePath << std::endl;
        return;
    }

    initialized = true;

    // Write header
    logFile << std::string(80, '=') << std::endl;
    logFile << "Log started at: " << GetTimestamp() << std::endl;
    logFile << std::string(80, '=') << std::endl;
    logFile.flush();
}

void Logger::SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    minLogLevel = level;
}

void Logger::Log(LogLevel level, const char* file, int line, const std::string& message) {
    // Check if level is high enough
    if (level < minLogLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    // Format log entry
    std::ostringstream logEntry;
    logEntry << "[" << GetTimestamp() << "]"
             << " [" << GetLogLevelString(level) << "]"
             << " [" << ExtractFileName(file) << ":" << line << "]"
             << " " << message;

    std::string formattedLog = logEntry.str();

    // Write to file
    if (initialized && logFile.is_open()) {
        logFile << formattedLog << std::endl;
        logFile.flush();  // Flush immediately for real-time logging
    }

    // Also write to console
    if (enableConsole) {
        if (level >= LogLevel::ERROR_L) {
            std::cerr << formattedLog << std::endl;
        } else {
            std::cout << formattedLog << std::endl;
        }
    }

    // Write to Windows Debug Output
    OutputDebugStringA((formattedLog + "\n").c_str());
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.flush();
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!initialized) {
        return;
    }

    if (logFile.is_open()) {
        logFile << std::string(80, '=') << std::endl;
        logFile << "Log ended at: " << GetTimestamp() << std::endl;
        logFile << std::string(80, '=') << std::endl;
        logFile.close();
    }

    initialized = false;
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string Logger::GetLogLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE_L: return "TRACE";
        case LogLevel::DEBUG_L: return "DEBUG";
        case LogLevel::INFO_L:  return "INFO ";
        case LogLevel::WARN_L:  return "WARN ";
        case LogLevel::ERROR_L: return "ERROR";
        case LogLevel::FATAL_L: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::ExtractFileName(const char* fullPath) {
    if (!fullPath) {
        return "unknown";
    }

    // Find last backslash or forward slash
    const char* lastSlash = strrchr(fullPath, '\\');
    if (!lastSlash) {
        lastSlash = strrchr(fullPath, '/');
    }

    if (lastSlash) {
        return std::string(lastSlash + 1);
    }

    return std::string(fullPath);
}
