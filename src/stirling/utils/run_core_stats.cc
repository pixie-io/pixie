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

#include "src/stirling/utils/run_core_stats.h"

namespace px {
namespace stirling {

namespace {

static constexpr std::array<std::chrono::nanoseconds, 23> kSleepBuckets = {
    std::chrono::nanoseconds{0},
    std::chrono::nanoseconds{10000},
    std::chrono::nanoseconds{17783},
    std::chrono::nanoseconds{31623},
    std::chrono::nanoseconds{56234},
    std::chrono::nanoseconds{100000},
    std::chrono::nanoseconds{177828},
    std::chrono::nanoseconds{316228},
    std::chrono::nanoseconds{562341},
    std::chrono::nanoseconds{1000000},
    std::chrono::nanoseconds{1778279},
    std::chrono::nanoseconds{3162278},
    std::chrono::nanoseconds{5623413},
    std::chrono::nanoseconds{10000000},
    std::chrono::nanoseconds{17782794},
    std::chrono::nanoseconds{31622777},
    std::chrono::nanoseconds{56234133},
    std::chrono::nanoseconds{100000000},
    std::chrono::nanoseconds{177827941},
    std::chrono::nanoseconds{316227766},
    std::chrono::nanoseconds{562341325},
    std::chrono::nanoseconds{1000000000},
    std::chrono::nanoseconds{10000000000000}};

std::string CreateHeaderString() {
  std::stringstream s;

  s << "|main_loop_iters,no_work_iters,useful_iters,push+transfer";
  s << ",transfer,push,min_push+transfer,max_push+transfer";

  for (const auto bucket : kSleepBuckets) {
    s << absl::StrFormat(",total_%.2f_ms", static_cast<double>(bucket.count()) / 1e6);
  }
  for (const auto bucket : kSleepBuckets) {
    s << absl::StrFormat(",no_work_%.2f_ms", static_cast<double>(bucket.count()) / 1e6);
  }
  // header_string_ = s.str();
  return s.str();
}

}  // namespace

RunCoreStats::RunCoreStats()
    : header_string_(CreateHeaderString()),
      sleep_histo_(kSleepBuckets.size(), 0),
      no_work_histo_(kSleepBuckets.size(), 0) {}

void RunCoreStats::IncrementTransferDataCount() {
  ++num_transfer_data_;
  ++push_or_transfer_this_iter_;
}

void RunCoreStats::IncrementPushDataCount() {
  ++num_push_data_;
  ++push_or_transfer_this_iter_;
}

void RunCoreStats::LogStats() const {
  std::string s = absl::StrJoin(sleep_histo_, ",");
  absl::StrAppend(&s, ",", absl::StrJoin(no_work_histo_, ","));

  LOG(INFO) << absl::Substitute("|$0,$1,$2,$3,$4,$5,$6,$7,$8", num_main_loop_iters_,
                                num_no_work_iters_, (num_main_loop_iters_ - num_no_work_iters_),
                                (num_transfer_data_ + num_push_data_), num_transfer_data_,
                                num_push_data_, min_push_or_transfer_, max_push_or_transfer_, s);
}

void RunCoreStats::EndIter(const std::chrono::milliseconds sleep_duration) {
  UpdateSleepDurationHisto(sleep_duration, &sleep_histo_);
  ++num_main_loop_iters_;
  min_push_or_transfer_ = std::min(push_or_transfer_this_iter_, min_push_or_transfer_);
  max_push_or_transfer_ = std::max(push_or_transfer_this_iter_, max_push_or_transfer_);

  if (push_or_transfer_this_iter_ == 0) {
    ++num_no_work_iters_;
    UpdateSleepDurationHisto(sleep_duration, &no_work_histo_);
  }
  push_or_transfer_this_iter_ = 0;

  constexpr uint64_t kPrintPeriod = 1000;
  constexpr uint64_t kHeaderPeriod = 50 * kPrintPeriod;

  // Will subtract 1 from iter count to make sure we print the headers immediately.
  if ((num_main_loop_iters_ - 1) % kHeaderPeriod == 0) {
    LOG(INFO) << header_string_;
  }
  if (num_main_loop_iters_ % kPrintPeriod == 0) {
    LogStats();
  }
}

uint64_t RunCoreStats::SleepCountForDuration(const std::chrono::nanoseconds d) const {
  uint32_t bucket_idx = 0;
  for (const auto bucket_value : kSleepBuckets) {
    if (d <= bucket_value) {
      return sleep_histo_[bucket_idx];
    }
    ++bucket_idx;
  }
  return 0;
}

uint64_t RunCoreStats::NoWorkCountForDuration(const std::chrono::nanoseconds d) const {
  uint32_t bucket_idx = 0;
  for (const auto bucket_value : kSleepBuckets) {
    if (d <= bucket_value) {
      return no_work_histo_[bucket_idx];
    }
    ++bucket_idx;
  }
  return 0;
}

void RunCoreStats::UpdateSleepDurationHisto(const std::chrono::milliseconds d,
                                            std::vector<uint64_t>* h) {
  // "d" is the sleep duration and "h" is the histogram that we will upate.
  // The histogram is designed to always find a valid bucket (there is no fall through case).
  uint32_t bucket_idx = 0;
  for (const auto bucket_value : kSleepBuckets) {
    if (d <= bucket_value) {
      auto& histo = *h;
      ++histo[bucket_idx];
      return;
    }
    ++bucket_idx;
  }
}

}  // namespace stirling
}  // namespace px
