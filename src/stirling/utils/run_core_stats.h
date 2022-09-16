/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

// RunCoreStats tracks the work done in each iteration of StirlingImpl::RunCore.
// It counts the number of PushData() and TransferData() calls.
// It also keeps a histogram of sleep durations: total, and those sleeps where no work is done.
class RunCoreStats {
 public:
  RunCoreStats();

  // Increment totals and per iteration counts.
  void IncrementTransferDataCount();
  void IncrementPushDataCount();

  // Logs the stats.
  void LogStats() const;

  // Called at the end of a StirlingImpl::RunCore() interation. Updates histograms
  // and potentially trigger a periodic log printout.
  void EndIter(const std::chrono::milliseconds sleep_duration);

  uint64_t num_main_loop_iters() const { return num_main_loop_iters_; }
  uint64_t num_push_data() const { return num_push_data_; }
  uint64_t num_transfer_data() const { return num_transfer_data_; }
  uint64_t min_push_or_transfer() const { return min_push_or_transfer_; }
  uint64_t max_push_or_transfer() const { return max_push_or_transfer_; }
  uint64_t num_no_work_iters() const { return num_no_work_iters_; }
  uint64_t push_or_transfer_this_iter() const { return push_or_transfer_this_iter_; }

  // These two accessors give the histogram count based on a duration passed as in input.
  // For now, they are useful only for the test case in run_core_stats_test.cc.
  uint64_t SleepCountForDuration(std::chrono::nanoseconds d) const;
  uint64_t NoWorkCountForDuration(std::chrono::nanoseconds d) const;

 private:
  // Update a particular sleep histogram (passed in as *h). Called by EndIter().
  void UpdateSleepDurationHisto(std::chrono::milliseconds d, std::vector<uint64_t>* h);

  // Header string used for stats printouts, populated in the ctor.
  const std::string header_string_;

  uint64_t num_main_loop_iters_ = 0;
  uint64_t num_push_data_ = 0;
  uint64_t num_transfer_data_ = 0;
  uint64_t min_push_or_transfer_ = ~(0ULL);
  uint64_t max_push_or_transfer_ = 0;
  uint64_t num_no_work_iters_ = 0;
  uint64_t push_or_transfer_this_iter_ = 0;
  std::vector<uint64_t> sleep_histo_;
  std::vector<uint64_t> no_work_histo_;
};

}  // namespace stirling
}  // namespace px
