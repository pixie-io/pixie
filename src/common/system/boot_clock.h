#pragma once

#include <chrono>

namespace px {
namespace chrono {

// An std::chrono style clock that is based on CLOCK_BOOTTIME.
// This is in contrast to std::steady_clock which is based on CLOCK_MONOTONIC.
class boot_clock {
 public:
  typedef std::chrono::nanoseconds duration;
  typedef std::chrono::time_point<boot_clock, duration> time_point;

  static inline time_point now() {
    timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return time_point(std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
  }
};

}  // namespace chrono
}  // namespace px
