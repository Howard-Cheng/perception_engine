#pragma once

#include <string>
#include <memory>
#include <sstream>

namespace perception {

// Log levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

// Logger class - singleton pattern
class Logger {
public:
    // Get singleton instance
    static Logger& getInstance();
    
    // Delete copy constructor and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // Logging methods
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    // Set log level
    void setLogLevel(LogLevel level);
    
    // Enable/disable file logging
    void enableFileLogging(const std::string& filePath);
    void disableFileLogging();
    
private:
    Logger();
    ~Logger();
    
    void log(LogLevel level, const std::string& message);
    std::string getLevelString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    
    LogLevel currentLevel_;
    bool fileLoggingEnabled_;
    std::string logFilePath_;
};

// Convenient macros for logging
#define LOG_DEBUG(msg) perception::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) perception::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) perception::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) perception::Logger::getInstance().error(msg)

} // namespace perception
