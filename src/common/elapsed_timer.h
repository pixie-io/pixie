#pragma once
#include <chrono>
#include <iostream>

#include "src/common/base.h"
#include "src/common/logging.h"

namespace pl {

/**
 * Timing class.
 *
 * Allows for measurement with high resolution timer with clocks that can be started and stopped.
 */
class ElapsedTimer : public NotCopyable {
 public:
  /**
   * Start the timer.
   */
  void Start() {
    DCHECK(!timer_running_) << "Timer already running";
    timer_running_ = true;
    start_time_ = std::chrono::high_resolution_clock::now();
  }

  /**
   * Stop the timer.
   */
  void Stop() {
    DCHECK(timer_running_) << "Stop called when timer is not running";
    timer_running_ = false;
    elapsed_time_us_ += TimeDiff();
  }

  /**
   * Reset the timer.
   */
  void Reset() {
    timer_running_ = false;
    elapsed_time_us_ = 0;
  }

  /**
   * @return the elapsed time in us.
   */
  double ElapsedTime_us() const { return elapsed_time_us_ + (timer_running_ ? TimeDiff() : 0); }

 private:
  double TimeDiff() const {
    auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(current - start_time_).count();
  }
  bool timer_running_ = false;
  std::chrono::high_resolution_clock::time_point start_time_;
  double elapsed_time_us_ = 0;
};

}  // namespace pl
