#pragma warning(disable : 4996)
#include "logger/log_message_impl.h"

#include <thread>

#include "logger/log_writer_impl.h"

#if defined(__ANDROID__)
#include <sys/syscall.h>
#endif
namespace pe_base {
LogMessageImpl::LogMessageImpl(const std::string& file, long line,
                               LogLevel level, const std::string& func /*= ""*/)
    : level_(level) {
  stream_.clear();
#if defined(_WIN32) || defined(__MACH__)
  stream_ << "[" << LogWriterImpl::Instance()->GetPidStr() << "|"
          << std::this_thread::get_id() << "]";
#else
#if defined(__ANDROID__)
  stream_ << "[" << LogWriterImpl::Instance()->GetPidStr() << "|"
          << (int)syscall(SYS_gettid) << "]";
#endif
#endif

  stream_ << GetFileName(file) << ":" << line << " ";
  if (!func.empty()) {
    stream_ << GetFunctionName(func) << ": ";
  }
}

std::stringstream& LogMessageImpl::stream() { return stream_; }

LogLevel LogMessageImpl::level() const { return level_; }

std::string LogMessageImpl::GetFileName(const std::string& path) {
  std::string filePath = path;
  if (filePath.back() == '\\' || filePath.back() == '/') filePath.pop_back();
  auto pos = filePath.find_last_of("/");
  if (pos == std::string::npos) {
    pos = filePath.find_last_of("\\");
  }
  return pos == std::string::npos ? filePath : filePath.substr(pos + 1);
}

std::string LogMessageImpl::GetFunctionName(const std::string& input) {
  std::string funcName = input;
  if (funcName.back() == ':') funcName.pop_back();
  auto pos = funcName.find_last_of("::");
  return pos == std::string::npos ? funcName : funcName.substr(pos + 1);
}
}  // namespace pe_base
