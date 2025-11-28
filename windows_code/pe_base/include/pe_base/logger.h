#pragma once
// Auth: Hanson Drew
// Desp: Log output function.

#include <stdint.h>

#include <memory>
#include <sstream>

#include "pe_base/pe_exports.h"
#include "pe_base/trace/trace.hpp"

namespace pe_base {
enum class LogLevel {
  kLogLevelInvalid = -1,
  kLogLevelDebug = 0,
  kLogLevelInfo = 1,
  kLogLevelWarn = 2,
  kLogLevelError = 3
};
//
// brief: Log message wrapper. Should not be used directly.
//
class pe_base_API LogMessage {
 public:
  static std::shared_ptr<LogMessage> Create(const std::string& file, long line,
                                            LogLevel type,
                                            const std::string& func = "");
  virtual std::stringstream& stream() = 0;
  virtual LogLevel level() const = 0;
  static std::string PointerToString(void* p);

 protected:
  virtual ~LogMessage(){};
};

class pe_base_API LogWriter {
 public:
  typedef void (*CustomLogFunctionPtr)(LogMessage&);
  //
  // brief: Set the log file path.
  //       Must be called before writing the first log message.
  // argv : [in] name: The log file path.
  //
  static void SetLogFilePrefix(const std::string& name);
  //
  // brief: Set the log min output level.
  //       e.g. If setting kLogLevelInfo, debug message cannot be output.
  //       Should be set before writing the first log message.
  // argv : [in] level: The log min output level.
  //       The default value is kLogLevelInfo.
  //
  static void SetOuputLogLevel(LogLevel level);

  static void SetCustomLogFunction(CustomLogFunctionPtr log_func);

  static void EnableTraceLog(bool enable);

  //
  // brief: Should not be called directly!!!
  //       Use the macro instead.
  // argv : [in] log_msg: The message need to be output.
  //
  static void Log(LogMessage& log_msg);

  static void LogTrace(std::stringstream& ss);

#if defined(__ANDROID__)
  static void SetLogTag(const std::string& tag);
#endif
};
}  // namespace pe_base
//
// brief: Convert pointer to hexadecimal, just like 0xefdffa70.
//
#define PE_LOG_PTR(ptr) pe_base::LogMessage::PointerToString((void*)ptr)
//
// brief: Should not be used. Tool Macro to output this pointer.
//
#define PE_THIS_PTR PE_LOG_PTR(this)
//
// brief: Should not be used. Tool Macro.
//
#define PE_LOG(msg, type)                                           \
  do {                                                              \
    auto m = pe_base::LogMessage::Create(                           \
        __FILE__, __LINE__, pe_base::LogLevel::type, __FUNCTION__); \
    m->stream() << msg;                                             \
    pe_base::LogWriter::Log(*m);                                    \
  } while (false);
#define PE_LOG_THIS(msg, type) PE_LOG(msg << ", this= " << PE_THIS_PTR, type);
//
// brief: Output log with different level.
//       E.x. PE_DEBUG("my value= " << value << ", second value= " << value2);
//       Only support base type.
//
#define PE_DEBUG(msg) PE_LOG(msg, kLogLevelDebug)
#define PE_INFO(msg) PE_LOG(msg, kLogLevelInfo)
#define PE_WARN(msg) PE_LOG(msg, kLogLevelWarn)
#define PE_ERROR(msg) PE_LOG(msg, kLogLevelError)

//
// brief: Output log with this pointer. Should be used in class member function,
//       not static function.
//
#define PE_DEBUG_THIS(msg) PE_LOG_THIS(msg, kLogLevelDebug)
#define PE_INFO_THIS(msg) PE_LOG_THIS(msg, kLogLevelInfo)
#define PE_WARN_THIS(msg) PE_LOG_THIS(msg, kLogLevelWarn)
#define PE_ERROR_THIS(msg) PE_LOG_THIS(msg, kLogLevelError)

//
// brief: Check the condition and output error log.
//       E.x. PE_CHECK(p == nullptr, "invalid pointer");
//
// NOTE(by Hanson Drew): Lack of operator checking, use carefully.
#define PE_CHECK(C, S)                       \
  {                                          \
    if (C) {                                 \
      PE_ERROR_THIS(#S << ", cond= " << #C); \
    }                                        \
  }
//
// brief: Check the condition, output error log and return value.
//       E.x. PE_CHECK_RET(p == nullptr, "invalid pointer", false);
//
#define PE_CHECK_RET(C, S, V)          \
  {                                    \
    if (C) {                           \
      PE_ERROR_THIS(#S << ", " << #C); \
      return V;                        \
    }                                  \
  }

//
// brief: Check the condition, output error log and run the code.
//       E.x. PE_CHECK_RET(p == nullptr, "invalid pointer", continue);
//
#ifndef PE_CHECK_RUN
#define PE_CHECK_RUN(C, S, R)          \
  {                                    \
    if (C) {                           \
      PE_ERROR_THIS(#S << ", " << #C); \
      R;                               \
    }                                  \
  }
#endif

#define PE_LOG_TRACE(msg)              \
  do {                                 \
    std::stringstream ss;              \
    ss << msg;                        \
    pe_base::LogWriter::LogTrace(ss); \
  } while (false)


