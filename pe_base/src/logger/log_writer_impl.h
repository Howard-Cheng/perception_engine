#pragma once
#include "pe_base/logger.h"

#if !(defined(ANDROID) || defined(__ANDROID__))
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#else
#include "spdlog/sinks/android_sink.h"
#endif

#include <atomic>
namespace pe_base {
class LogWriterImpl {
 public:
  static LogWriterImpl* Instance();
  void SetLogFilePrefix(const std::string& name);
  void Write(LogMessage& msg);
  void SetLogLevel(LogLevel level);
  void SetCustomLogFunction(LogWriter::CustomLogFunctionPtr log_func);
  std::string GetPidStr() const;
#if defined (__ANDROID__)
  void SetLogTag(const std::string& tag);
#endif
  void EnableTraceLog(bool enable) {enable_trace_log_ = enable;}
  bool TraceLogEnabled() const {return enable_trace_log_;}
  void WriteTrace(std::stringstream& trace_msg);

 private:
  LogWriterImpl();
  bool InitSpdLogger();
  inline uint64_t GetPid();

 private:
  std::atomic<LogLevel> output_level_ = LogLevel::kLogLevelInfo;
  std::shared_ptr<spdlog::logger> logger_;
  std::atomic_bool inited_;
  std::mutex mutex_;
  std::string log_file_prefix_;
  std::string pid_;
  std::atomic_bool enable_trace_log_ = false;
#if defined (__ANDROID__)
  std::string log_tag_;
#endif
  LogWriter::CustomLogFunctionPtr custom_log_function_ = nullptr;
};
}  // namespace pe_base
