#pragma once

#include <string>
#include <utility>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/perf/elapsed_timer.h"

namespace px {

/**
 * Times a particular function scope and prints the time to the log.
 * @tparam TTimer Can be any class that implements Start and ElapsedTime_us().
 */
template <class TTimer = ElapsedTimer>
class ScopedTimer : public NotCopyable {
 public:
  /**
   * Creates a scoped timer with the given name.
   * @param name
   */
  explicit ScopedTimer(std::string name) : name_(std::move(name)) { timer_.Start(); }

  /**
   * Writes to the log the elapsed time.
   */
  ~ScopedTimer() {
    double elapsed = timer_.ElapsedTime_us();
    LOG(INFO) << absl::StrFormat("Timer(%s) : %s", name_, PrettyDuration(1000 * elapsed));
  }

 private:
  TTimer timer_;
  std::string name_;
};

}  // namespace px
