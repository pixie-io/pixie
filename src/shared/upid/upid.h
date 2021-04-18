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

#include <string>
#include <utility>

#include <absl/hash/hash.h>
#include <absl/numeric/int128.h>
#include <absl/strings/substitute.h>
#include <sole.hpp>

#include "src/common/base/error.h"
#include "src/common/base/statusor.h"

namespace px {
namespace md {

/**
 * UID refers to the unique IDs used by K8s. These IDs are
 * unique in both space and time.
 *
 * K8s stores UID as both regular strings and UUIDs.
 * The view equivalents are used to provide heterogeneous lookups.
 */
using UID = std::string;
using UIDView = std::string_view;

/**
 * CID refers to a unique container ID. This ID is unique in both
 * space and time.
 * The view equivalents are used to provide heterogeneous lookups.
 */
using CID = std::string;
using CIDView = std::string_view;
/**
 * Unique PIDs refers to uniquefied pids. They are unique in both
 * space and time. The format for a unique PID is:
 *
 * ------------------------------------------------------------
 * | 32-bit ASID     |  32-bit PID  |     64-bit Start TS     |
 * ------------------------------------------------------------
 */
class UPID {
 public:
  // TODO(zasgar): delete when we start using UPIDs and don't need the empty constructor anymore.
  UPID() = default;
  UPID(uint32_t asid, uint32_t pid, int64_t ts_ns) {
    uint64_t upper = asid;
    upper <<= 32U;
    upper |= pid;
    value_ = absl::MakeUint128(upper, ts_ns);
  }

  explicit UPID(absl::uint128 value) : value_(value) {}

  uint32_t pid() const { return static_cast<uint32_t>(absl::Uint128High64(value_)); }

  uint32_t asid() const { return static_cast<uint32_t>(absl::Uint128High64(value_) >> 32U); }

  int64_t start_ts() const { return static_cast<int64_t>(Uint128Low64(value_)); }

  absl::uint128 value() const { return value_; }
  sole::uuid uuid() const {
    return sole::rebuild(absl::Uint128High64(value_), absl::Uint128Low64(value_));
  }

  std::string String() const { return absl::Substitute("$0:$1:$2", asid(), pid(), start_ts()); }

  bool operator==(const UPID& rhs) const { return this->value_ == rhs.value_; }

  bool operator!=(const UPID& rhs) const { return !(*this == rhs); }

  bool operator<(const UPID& other) const { return value_ < other.value_; }

  template <typename H>
  friend H AbslHashValue(H h, const UPID& c) {
    return H::combine(std::move(h), c.value_);
  }

  static StatusOr<UPID> ParseFromUUIDString(const std::string& str) {
    sole::uuid u = sole::rebuild(str);
    if (u.ab == 0 && u.cd == 0) {
      return error::InvalidArgument("'$0' is not a valid UUID", str);
    }
    return UPID(absl::MakeUint128(u.ab, u.cd));
  }

 private:
  absl::uint128 value_ = 0;
};

// Needed for gtest to print UPID.
inline std::ostream& operator<<(std::ostream& os, const md::UPID& upid) {
  os << upid.String();
  return os;
}

}  // namespace md
}  // namespace px
