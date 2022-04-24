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

#include <memory>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace md {

/**
 * Store information about PIDs.
 */
class PIDInfo {
 public:
  PIDInfo() = delete;
  PIDInfo(UPID upid, std::string exe_path, std::string cmdline, CID cid)
      : upid_(upid),
        exe_path_(std::move(exe_path)),
        cmdline_(std::move(cmdline)),
        cid_(std::move(cid)),
        stop_time_ns_(0) {}

  UPID upid() const { return upid_; }

  int64_t start_time_ns() const { return upid_.start_ts(); }

  int64_t stop_time_ns() const { return stop_time_ns_; }

  void set_stop_time_ns(int64_t ts) { stop_time_ns_ = ts; }

  const std::string& exe_path() const { return exe_path_; }

  const std::string& cmdline() const { return cmdline_; }

  const CID& cid() const { return cid_; }

  std::unique_ptr<PIDInfo> Clone() {
    auto pid_info = std::make_unique<PIDInfo>(*this);
    return pid_info;
  }

  bool operator==(const PIDInfo& other) const {
    return (other.upid_ == upid_) && (other.exe_path_ == exe_path_) &&
           (other.cmdline_ == cmdline_) && (other.cid_ == cid_) &&
           (other.stop_time_ns_ == stop_time_ns_);
  }

  bool operator!=(const PIDInfo& other) const { return !(other == *this); }

  std::string DebugString() const;

 private:
  UPID upid_;

  /**
   * The path to the executable of this process.
   */
  std::string exe_path_;

  /**
   * The command line used to start this PID.
   */
  std::string cmdline_;

  /**
   * The container running this PID.
   */
  CID cid_;

  /**
   * The time that this PID stopped running. If 0 we can assume it's still running.
   */
  int64_t stop_time_ns_ = 0;
};

/*
 * Description of events used to transmit information about
 * PID creation and deletion.
 */

/**
 * The PID status event type.
 */
enum class PIDStatusEventType : uint8_t { kUnknown = 0, kStarted, kTerminated };

/**
 * Base class for PID status event.
 */
struct PIDStatusEvent {
  PIDStatusEvent() = delete;
  virtual ~PIDStatusEvent() = default;

  explicit PIDStatusEvent(PIDStatusEventType type) : type(type) {}
  PIDStatusEventType type = PIDStatusEventType::kUnknown;

  virtual std::string DebugString() const = 0;
};

/**
 * PIDStartEvent has information about new PIDs.
 * It contains a copy of the newly created PID Info.
 */
struct PIDStartedEvent : public PIDStatusEvent {
  explicit PIDStartedEvent(const PIDInfo& other)
      : PIDStatusEvent(PIDStatusEventType::kStarted), pid_info(other) {}

  std::string DebugString() const override;

  const PIDInfo pid_info;
};

/**
 * PIDTerminatedEvent has information about deleted PIDs.
 * It only contains the unique PID that was deleted and the termination time stamp.
 */
struct PIDTerminatedEvent : public PIDStatusEvent {
  explicit PIDTerminatedEvent(UPID stopped_pid, int64_t _stop_time_ns)
      : PIDStatusEvent(PIDStatusEventType::kTerminated),
        upid(stopped_pid),
        stop_time_ns(_stop_time_ns) {}

  std::string DebugString() const override;

  const UPID upid;
  const int64_t stop_time_ns;
};

/**
 * Print and compare functions.
 */
std::ostream& operator<<(std::ostream& os, const PIDInfo& info);
std::ostream& operator<<(std::ostream& os, const PIDStatusEvent& ev);
std::ostream& operator<<(std::ostream& os, const PIDStartedEvent& ev);

bool operator==(const PIDStartedEvent& lhs, const PIDStartedEvent& rhs);
bool operator==(const PIDTerminatedEvent& lhs, const PIDTerminatedEvent& rhs);

}  // namespace md
}  // namespace px
