#pragma once

#include <chrono>

namespace pl {
namespace stirling {

// Manages how often data sampling and pushing should be performed.
class SamplePushFrequencyManager {
 public:
  /**
   * Returns true if sampling is required, for whatever reason (elapsed time, etc.).
   *
   * @return bool
   */
  bool SamplingRequired() const;

  /**
   * Called when sampling data to update timestamps.
   */
  void Sample();

  /**
   * Returns true if a data push is required, for whatever reason (elapsed time, occupancy, etc.).
   *
   * @return bool
   */
  bool PushRequired(double occupancy_percentage, uint32_t occupancy) const;

  /**
   * Called when pushing data to update timestamps.
   */
  void Push();

  /**
   * Returns the next time the source needs to be sampled, according to the sampling period.
   *
   * @return std::chrono::milliseconds
   */
  std::chrono::steady_clock::time_point NextSamplingTime() const;

  /**
   * Returns the next time the data table needs to be pushed upstream, according to the push period.
   *
   * @return std::chrono::milliseconds
   */
  std::chrono::steady_clock::time_point NextPushTime() const;

  void set_sampling_period(std::chrono::milliseconds period) { sampling_period_ = period; }
  void set_push_period(std::chrono::milliseconds period) { push_period_ = period; }
  const auto& sampling_period() const { return sampling_period_; }
  const auto& push_period() const { return push_period_; }
  uint32_t sampling_count() const { return sampling_count_; }

 private:
  // Sampling period.
  std::chrono::milliseconds sampling_period_;

  // Keep track of when the source was last sampled.
  std::chrono::steady_clock::time_point last_sampled_;

  // Sampling period.
  std::chrono::milliseconds push_period_;

  // Keep track of when the source was last sampled.
  std::chrono::steady_clock::time_point last_pushed_;

  // Data push threshold, based number of records after which a push.
  static constexpr uint32_t kDefaultOccupancyThreshold = 1024;
  uint32_t occupancy_threshold_ = kDefaultOccupancyThreshold;

  // Data push threshold, based on percentage of buffer that is filled.
  static constexpr uint32_t kDefaultOccupancyPctThreshold = 100;
  uint32_t occupancy_pct_threshold_ = kDefaultOccupancyPctThreshold;

  uint32_t sampling_count_ = 0;
  uint32_t push_count_ = 0;
};

}  // namespace stirling
}  // namespace pl
