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
#include <utility>

#include <absl/strings/substitute.h>
#include <grpcpp/grpcpp.h>

#include "src/common/base/base.h"
#include "src/integrations/grpc_clocksync/grpc_clock_converter.h"

namespace px {
namespace integrations {
namespace grpc_clocksync {

DEFINE_string(grpc_clocksync_port, gflags::StringFromEnv("PL_GRPC_CLOCKSYNC_PORT", "6171"),
              "The port the gRPC clocksync agent is listening on.");

GRPCClockConverter::GRPCClockConverter()
    : mono_to_realtime_(std::make_unique<DefaultMonoToRealtimeConverter>()) {
  MustSetupGRPC();
  LOG(INFO) << "Started gRPC ClockConverter";
}

void GRPCClockConverter::Update() {
  if (time_since_mono_to_realtime_ >= DefaultMonoToRealtimeConverter::kUpdatePeriod) {
    time_since_mono_to_realtime_ = std::chrono::milliseconds{0};
    mono_to_realtime_->Update();
  }

  auto s = PollForRefTime();
  if (s.ok()) {
    disable_grpc_offsets_ = false;
    consecutive_failed_polls_ = 0;
    update_period_ = kUpdatePeriod;
    time_since_mono_to_realtime_ += update_period_;
    return;
  }

  if (disable_grpc_offsets_.load()) {
    update_period_ = kBackoffUpdatePeriod;
    time_since_mono_to_realtime_ += update_period_;
    return;
  }

  LOG(WARNING) << "Failed to poll grpc_clocksync: " << s.code() << ", " << s.msg();
  consecutive_failed_polls_++;
  if (consecutive_failed_polls_ >= kMaxConsecutiveFailures) {
    disable_grpc_offsets_ = true;
  }
  time_since_mono_to_realtime_ += update_period_;
}

void GRPCClockConverter::MustSetupGRPC() {
  std::string host_ip = gflags::StringFromEnv("PL_HOST_IP", "");
  if (host_ip.length() == 0) {
    LOG(FATAL) << "Must set PL_HOST_IP";
  }
  std::string target = absl::Substitute("$0:$1", host_ip, FLAGS_grpc_clocksync_port);
  auto chan = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  stub_ = ::clocksync::ProberService::NewStub(chan);
}

StatusOr<::clocksync::CurrentReferenceTimeReply> GRPCClockConverter::GetCurrentReferenceTime() {
  grpc::ClientContext ctx;
  ::clocksync::CurrentReferenceTimeRequest req;
  ::clocksync::CurrentReferenceTimeReply resp;
  auto s = stub_->CurrentReferenceTime(&ctx, req, &resp);
  if (!s.ok()) {
    return Status(statuspb::Code::INTERNAL, s.error_message());
  }
  return resp;
}

Status GRPCClockConverter::PollForRefTime() {
  PX_ASSIGN_OR_RETURN(auto resp, GetCurrentReferenceTime());
  realtime_to_reftime_.Emplace(resp.local_time(), resp.reference_time());
  return Status::OK();
}

uint64_t GRPCClockConverter::Convert(uint64_t mono_time) const {
  auto real_time = mono_to_realtime_->Convert(mono_time);
  if (disable_grpc_offsets_.load()) {
    return real_time;
  }
  auto reftime = realtime_to_reftime_.Get(real_time);
  return reftime;
}

}  // namespace grpc_clocksync
}  // namespace integrations
}  // namespace px
