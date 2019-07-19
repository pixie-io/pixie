#pragma once

#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/numeric/int128.h"
#include "absl/strings/substitute.h"

namespace pl {
namespace md {

/**
 * UID refers to the unique IDs used by K8s. These IDs are
 * unique in both space and time.
 *
 * K8s stores UID as both regular strings and UUIDs.
 */
using UID = std::string;

/**
 * CID refers to a unique container ID. This ID is unique in both
 * space and time.
 */
using CID = std::string;

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

  uint32_t pid() const { return static_cast<uint32_t>(absl::Uint128High64(value_)); }

  uint32_t asid() const { return static_cast<uint32_t>(absl::Uint128High64(value_) >> 32U); }

  int64_t start_ts() const { return static_cast<uint32_t>(Uint128Low64(value_)); }

  absl::uint128 value() const { return value_; }

  std::string String() const { return absl::Substitute("$0:$1:$2", asid(), pid(), start_ts()); }

  bool operator==(const UPID& rhs) const { return this->value_ == rhs.value_; }

  bool operator!=(const UPID& rhs) const { return !(*this == rhs); }

  template <typename H>
  friend H AbslHashValue(H h, const UPID& c) {
    return H::combine(std::move(h), c.value_);
  }

 private:
  absl::uint128 value_ = 0;
};

}  // namespace md
}  // namespace pl
