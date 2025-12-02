#pragma warning(disable : 4566)
#pragma warning(disable : 4996)
#include "logger/log_writer_impl.h"

#include <mutex>

#include "spdlog/async.h"

#include "pe_base/trace/trace.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
#include "unistd.h"
#endif
namespace pe_base {
static const char* kDefaultLoggerName = "PES";
#if defined(_WIN32) || defined(__MACH__)
static const char* kDefaultLogFilePrefix = "PES";
constexpr size_t kDefaultMaxLogFileSize = 1024 * 1024 * 20;
constexpr size_t kDefaultMaxLogFileNumber = 10;
#elif defined(__ANDROID__)
static const char* kDefaultAndroidLogTag = "PES";
#endif

LogWriterImpl* LogWriterImpl::Instance() {
  static LogWriterImpl* sInstance = nullptr;
  static std::mutex sMutex;
  if (sInstance == nullptr) {
    std::lock_guard<std::mutex> lock(sMutex);
    if (sInstance == nullptr) {
      sInstance = new LogWriterImpl();
    }
  }
  return sInstance;
}

void LogWriterImpl::SetLogFilePrefix(const std::string& name) {
#if defined(_WIN32) || defined(__MACH__)
  if (logger_) return;
  std::lock_guard<std::mutex> lk(mutex_);
  log_file_prefix_ = name;
#endif
}

bool LogWriterImpl::InitSpdLogger() {
  std::lock_guard<std::mutex> lk(mutex_);
  if (logger_) {
    return true;
  }
#if defined(_WIN32) || defined(__MACH__)
  std::string log_name = log_file_prefix_;
  //         log_name += "_";
  //         log_name += std::to_string(pid_);
  log_name += ".log";
  logger_ = spdlog::rotating_logger_mt<spdlog::async_factory>(
      kDefaultLoggerName, log_name, kDefaultMaxLogFileSize,
      kDefaultMaxLogFileNumber, false);
#elif defined(__ANDROID__)
  logger_ =
      spdlog::android_logger_mt<spdlog::async_factory>(log_tag_, log_tag_);
#endif
  if (!logger_) {
    return false;
  }
  logger_->set_level(spdlog::level::level_enum::trace);
  logger_->info("***********  Log Begin **********");
  logger_->info("process id= {}", pid_);
  return true;
}

LogWriterImpl::LogWriterImpl()
    : pid_(std::to_string(GetPid()))
#if defined(_WIN32) || defined(__MACH__)
      ,
      log_file_prefix_(kDefaultLogFilePrefix)
#elif defined(__ANDROID__)
      ,
      log_tag_(kDefaultLoggerName)
#endif
{
}

uint64_t LogWriterImpl::GetPid() {
  uint64_t pid = 0;
#if defined(_WIN32)
  pid = GetCurrentProcessId();
#else
  pid = getpid();
#endif
  return pid;
}

std::string LogWriterImpl::GetPidStr() const { return pid_; }

void LogWriterImpl::Write(LogMessage& msg) {
  if (custom_log_function_) {
    custom_log_function_(msg);
  }

  if (msg.level() < output_level_) {
    return;
  }

  if (!logger_) {
    if (!InitSpdLogger()) return;
  }
  spdlog::level::level_enum level = spdlog::level::level_enum::err;
  switch (msg.level()) {
    case LogLevel::kLogLevelDebug:
#ifdef __ANDROID__
      level = spdlog::level::level_enum::info;
#else
      level = spdlog::level::level_enum::debug;
#endif
      break;
    case LogLevel::kLogLevelInfo:
      level = spdlog::level::level_enum::info;
      break;
    case LogLevel::kLogLevelWarn:
      level = spdlog::level::level_enum::warn;
      break;
    default:
      break;
  }
  logger_->log(level, msg.stream().str());
  logger_->flush();
}

void LogWriterImpl::SetLogLevel(LogLevel level) { output_level_ = level; }

void LogWriterImpl::SetCustomLogFunction(
    LogWriter::CustomLogFunctionPtr log_func) {
  custom_log_function_ = log_func;
}

#if defined(__ANDROID__)
void LogWriterImpl::SetLogTag(const std::string& tag) {
  if (logger_) return;
  std::lock_guard<std::mutex> lk(mutex_);
  log_tag_ = tag;
}
#endif

void LogWriterImpl::WriteTrace(std::stringstream& trace_msg) {
  if (!TraceLogEnabled()) {
    return;
  }
#if defined(__ANDROID__)
  PE_TRACE_NAME(trace_msg.str().c_str());
#endif
}
}  // namespace pe_base
