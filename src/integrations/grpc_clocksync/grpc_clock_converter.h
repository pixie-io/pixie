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

#include <absl/base/internal/spinlock.h>
#include <memory>
#include <string>
#include <vector>

#include <magic_enum.hpp>
#include "src/common/clock/clock_conversion.h"
#include "src/integrations/grpc_clocksync/proto/clocksync.grpc.pb.h"

namespace px {
namespace integrations {
namespace grpc_clocksync {

using clock::DefaultMonoToRealtimeConverter;
using clock::InterpolatingLookupTable;

class GRPCClockConverter : public clock::ClockConverter {
 public:
  GRPCClockConverter();

  uint64_t Convert(uint64_t monotonic_time) const override;
  void Update() override;
  std::chrono::milliseconds UpdatePeriod() const override { return update_period_; }

 private:
  static constexpr std::chrono::seconds kUpdatePeriod = std::chrono::seconds{1};
  // Max number of times to retry polling grpc endpoint without a successful poll. After this, the
  // converter will only use the mono to realtime converter and ignore grpc offsets until
  // a successful poll.
  static constexpr size_t kMaxConsecutiveFailures = 5;
  // After polling has failed kMaxConsecutiveFailures times consecutively, we backoff the update
  // period until another successful poll.
  static constexpr std::chrono::seconds kBackoffUpdatePeriod = std::chrono::seconds{30};

  std::unique_ptr<DefaultMonoToRealtimeConverter> mono_to_realtime_;
  InterpolatingLookupTable<clock::ClockConverter::BufferCapacity(kUpdatePeriod)>
      realtime_to_reftime_;
  // gRPC offsets are disabled until a successful poll.
  std::atomic<bool> disable_grpc_offsets_ = true;

  std::unique_ptr<::clocksync::ProberService::StubInterface> stub_;

  std::chrono::milliseconds update_period_ = kUpdatePeriod;
  size_t consecutive_failed_polls_ = 0;
  std::chrono::milliseconds time_since_mono_to_realtime_ = std::chrono::milliseconds{0};

  void MustSetupGRPC();
  Status PollForRefTime();
  StatusOr<::clocksync::CurrentReferenceTimeReply> GetCurrentReferenceTime();
};

}  // namespace grpc_clocksync
}  // namespace integrations
}  // namespace px
