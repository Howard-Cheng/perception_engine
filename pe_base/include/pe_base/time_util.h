#pragma once
#include <cstdint>

#include "pe_base/pe_exports.h"

namespace pe_base {
class pe_base_API TimeUtil {
 public:
  static int64_t TimestampNs();
  static int64_t TimestampUs();
  static int64_t TimestampMs();
};
}  // namespace pe_base
