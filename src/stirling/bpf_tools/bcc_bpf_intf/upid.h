#pragma once

#ifdef __cplusplus
#include <utility>
#include "src/common/base/hash_utils.h"
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
#endif
};
