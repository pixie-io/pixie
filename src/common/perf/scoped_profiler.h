#pragma once

#include <string>

#include "src/common/base/base.h"

namespace px {

/**
 * Scoped profiler is used to profile a block of code.
 * Usage:
 *    ScopedProfiler<profiler::CPU> profile(true, "output");
 * @tparam T The type of profiler.
 */
template <class T>
class ScopedProfiler {
 public:
  /**
   * Constructor for scoped profiler.
   * @param enabled true if profiling is enabled
   * @param output_path The output path of the dump.
   */
  ScopedProfiler(bool enabled, std::string output_path) : enabled_(enabled) {
    if (!enabled_) {
      return;
    }

    if (!T::ProfilerAvailable()) {
      LOG(ERROR) << "Profiler enabled, but not available";
      enabled_ = false;
      return;
    }

    CHECK(T::StartProfiler(output_path)) << "Failed to start profiler";
  }

  ~ScopedProfiler() {
    if (!enabled_) {
      return;
    }

    T::StopProfiler();
  }

 private:
  bool enabled_;
};

}  // namespace px
