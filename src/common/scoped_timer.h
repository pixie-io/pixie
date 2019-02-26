#pragma once

#include <string>

#include "absl/strings/str_format.h"
#include "src/common/base.h"
#include "src/common/elapsed_timer.h"
#include "src/common/logging.h"

namespace pl {

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
  explicit ScopedTimer(const std::string& name) : name_(name) { timer_.Start(); }

  /**
   * Writes to the log the elapsed time.
   */
  ~ScopedTimer() {
    double elapsed = timer_.ElapsedTime_us();
    LOG(INFO) << absl::StrFormat("Timer(%s) : %s", name_, PrettyString(elapsed));
  }

 private:
  TTimer timer_;
  std::string name_;

  std::string PrettyString(double t) {
    // us,ms,s,
    if (t < 1000) {
      return absl::StrFormat("%.2f\u03BCs", t);
    } else if (t < 100'000) {
      return absl::StrFormat("%.2fms", t / 1000.0);
    }
    return absl::StrFormat("%.2fs", t / 1000.0 / 1000.0);
  }
};

}  // namespace pl
