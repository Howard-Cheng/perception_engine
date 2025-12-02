#pragma warning(disable : 4566)
#pragma warning(disable : 4996)

#include "pe_base/logger.h"

#include "logger/log_message_impl.h"
#include "logger/log_writer_impl.h"

namespace pe_base {
LogWriter::CustomLogFunctionPtr p_custom_log_function = nullptr;

void LogWriter::SetLogFilePrefix(const std::string& name) {
  LogWriterImpl::Instance()->SetLogFilePrefix(name);
}

void LogWriter::SetOuputLogLevel(LogLevel level) {
  LogWriterImpl::Instance()->SetLogLevel(level);
}

void LogWriter::EnableTraceLog(bool enable) {
  LogWriterImpl::Instance()->EnableTraceLog(enable);
}

void LogWriter::SetCustomLogFunction(CustomLogFunctionPtr log_func) {
  LogWriterImpl::Instance()->SetCustomLogFunction(log_func);
}

#if defined(__ANDROID__)
    void LogWriter::SetLogTag(const std::string& tag) {
  LogWriterImpl::Instance()->SetLogTag(tag);
}
#endif

void LogWriter::Log(LogMessage& log_msg) {
  LogWriterImpl::Instance()->Write(log_msg);
}

void LogWriter::LogTrace(std::stringstream& trace_msg) {
  LogWriterImpl::Instance()->WriteTrace(trace_msg);
}

std::shared_ptr<LogMessage> LogMessage::Create(
    const std::string& file, long line, LogLevel level,
    const std::string& func /*= ""*/) {
  return std::make_shared<LogMessageImpl>(file, line, level, func);
}

std::string LogMessage::PointerToString(void* p) {
  char buf[128];
  // Note(by Hanson Drew): Check if safety in different system.
#if defined(_WIN32) || defined(__MACH__)
  size_t num = snprintf(buf, 128, "0x%llx", (uint64_t)p);
#else
  size_t num = sprintf(buf, "0x%lx", (uint64_t)p);
#endif

  return buf;
}
}  // namespace pe_base
