#pragma once
#include <sstream>
#include <string>

#include "pe_base/logger.h"

namespace pe_base {
class LogMessageImpl : public LogMessage {
 public:
  LogMessageImpl(const std::string& file, long line, LogLevel level,
                 const std::string& func = "");
  virtual std::stringstream& stream() override;
  virtual LogLevel level() const override;

 private:
  inline std::string GetFileName(const std::string& path);
  inline std::string GetFunctionName(const std::string& input);

 private:
  std::stringstream stream_;
  LogLevel level_ = LogLevel::kLogLevelInvalid;
};
}  // namespace pe_base

