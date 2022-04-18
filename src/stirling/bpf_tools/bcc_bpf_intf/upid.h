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

#ifdef __cplusplus
#include <cstdint>
#include <utility>
#include "src/common/base/hash_utils.h"
#include "src/shared/upid/upid.h"
#endif

// UPID stands for unique pid.
// Since PIDs can be reused, this attaches the start time of the PID,
// so that the identifier becomes unique.
// Note that this version is node specific; there is also a 'class UPID'
// definition under shared which also includes an Agent ID (ASID),
// to uniquely identify PIDs across a cluster. The ASID is not required here.
struct upid_t {
  // Comes from the process from which this is captured.
  // See https://stackoverflow.com/a/9306150 for details.
  // Use union to give it two names. We use tgid in kernel-space, pid in user-space.
  union {
    uint32_t pid;
    uint32_t tgid;
  };
  uint64_t start_time_ticks;

#ifdef __cplusplus
  friend inline bool operator==(const upid_t& lhs, const upid_t& rhs) {
    return (lhs.tgid == rhs.tgid) && (lhs.start_time_ticks == rhs.start_time_ticks);
  }

  friend inline bool operator!=(const upid_t& lhs, const upid_t& rhs) { return !(lhs == rhs); }

  template <typename H>
  friend H AbslHashValue(H h, const upid_t& upid) {
    return H::combine(std::move(h), upid.tgid, upid.start_time_ticks);
  }

  px::md::UPID ToMetadataUPID(uint32_t asid) const {
    return px::md::UPID{asid, pid, static_cast<int64_t>(start_time_ticks)};
  }
#endif
};
