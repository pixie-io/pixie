#include "src/stirling/core/utils.h"

namespace px {
namespace stirling {

bool SamplePushFrequencyManager::SamplingRequired() const {
  return std::chrono::steady_clock::now() > NextSamplingTime();
}

void SamplePushFrequencyManager::Sample() {
  last_sampled_ = std::chrono::steady_clock::now();
  ++sampling_count_;
}

bool SamplePushFrequencyManager::PushRequired(double occupancy_percentage,
                                              uint32_t occupancy) const {
  // Note: It's okay to exercise an early Push, by returning true before the final return,
  // but it is not okay to 'return false' in this function.

  if (static_cast<uint32_t>(100 * occupancy_percentage) > occupancy_pct_threshold_) {
    return true;
  }

  if (occupancy > occupancy_threshold_) {
    return true;
  }

  return std::chrono::steady_clock::now() > NextPushTime();
}

void SamplePushFrequencyManager::Push() {
  last_pushed_ = std::chrono::steady_clock::now();
  ++push_count_;
}

std::chrono::steady_clock::time_point SamplePushFrequencyManager::NextSamplingTime() const {
  return last_sampled_ + sampling_period_;
}

std::chrono::steady_clock::time_point SamplePushFrequencyManager::NextPushTime() const {
  return last_pushed_ + push_period_;
}

}  // namespace stirling
}  // namespace px
