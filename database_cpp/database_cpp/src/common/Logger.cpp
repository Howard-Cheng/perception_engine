#include "common/Logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace perception {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : currentLevel_(LogLevel::INFO)
    , fileLoggingEnabled_(false) {
}

Logger::~Logger() {
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel_ = level;
}

void Logger::enableFileLogging(const std::string& filePath) {
    logFilePath_ = filePath;
    fileLoggingEnabled_ = true;
}

void Logger::disableFileLogging() {
    fileLoggingEnabled_ = false;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = getLevelString(level);
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Console output
    if (level >= LogLevel::ERROR) {
        std::cerr << logMessage << std::endl;
    } else {
        std::cout << logMessage << std::endl;
    }
    
    // File output
    if (fileLoggingEnabled_ && !logFilePath_.empty()) {
        std::ofstream logFile(logFilePath_, std::ios::app);
        if (logFile.is_open()) {
            logFile << logMessage << std::endl;
            logFile.close();
        }
    }
}

std::string Logger::getLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace perception
