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

#include "src/shared/metadata/pids.h"

namespace px {
namespace md {

std::string PIDInfo::DebugString() const {
  return absl::Substitute("upid: $0, exe_path=$1, cmdline=$2, cid=$3", upid().String(), exe_path(),
                          cmdline(), cid());
}

std::string PIDStartedEvent::DebugString() const {
  return absl::Substitute("PIDStartedEvent<$0>", pid_info.DebugString());
}

std::string PIDTerminatedEvent::DebugString() const {
  return absl::Substitute("PIDTerminatedEvent<upid=$0, stop_time_ns=$1>", upid.String(),
                          stop_time_ns);
}

std::ostream& operator<<(std::ostream& os, const PIDInfo& info) { return os << info.DebugString(); }

std::ostream& operator<<(std::ostream& os, const PIDStatusEvent& ev) {
  return os << ev.DebugString();
}

std::ostream& operator<<(std::ostream& os, const PIDStartedEvent& ev) {
  return os << ev.DebugString();
}

bool operator==(const PIDStartedEvent& lhs, const PIDStartedEvent& rhs) {
  return lhs.pid_info == rhs.pid_info;
}

bool operator==(const PIDTerminatedEvent& lhs, const PIDTerminatedEvent& rhs) {
  return lhs.upid == rhs.upid && lhs.stop_time_ns == rhs.stop_time_ns;
}
}  // namespace md
}  // namespace px
