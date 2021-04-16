#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "src/common/event/time_system.h"

namespace px {
namespace event {

/**
 * Real-world time implementation of TimeSource.
 */
class RealTimeSource : public TimeSource {
 public:
  SystemTimePoint SystemTime() const override { return std::chrono::system_clock::now(); }
  MonotonicTimePoint MonotonicTime() const override { return std::chrono::steady_clock::now(); }
};

class RealTimeSystem : public TimeSystem {
 public:
  SchedulerUPtr CreateScheduler(Scheduler* base_scheduler) override;

  SystemTimePoint SystemTime() const override { return time_source_.SystemTime(); }
  MonotonicTimePoint MonotonicTime() const override { return time_source_.MonotonicTime(); }

  const TimeSource& time_source() { return time_source_; }

 private:
  RealTimeSource time_source_;
};

}  // namespace event
}  // namespace px
