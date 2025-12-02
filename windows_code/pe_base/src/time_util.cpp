#include "pe_base/time_util.h"

#include <chrono>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace pe_base {
int64_t TimeUtil::TimestampNs(void) {
  auto clk = std::chrono::high_resolution_clock::now();
  const auto now = clk.time_since_epoch();
  const auto now_in_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
  return now_in_ns;
}

int64_t TimeUtil::TimestampUs(void) { return TimestampNs() / 1000; }

int64_t TimeUtil::TimestampMs(void) { return TimestampUs() / 1000; }

}  // namespace pe_base

