#pragma once

#ifdef __cplusplus
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
  inline bool operator==(const upid_t& other) const {
    return (tgid == other.tgid) && (start_time_ticks == other.start_time_ticks);
  }
#endif
};

#ifdef __cplusplus
// Hash function required to use TableTabletCol as an unordered_map key.
class UPIDHashFn {
 public:
  inline size_t operator()(const struct upid_t& val) const {
    size_t hash = 0;
    hash = pl::HashCombine(hash, val.tgid);
    hash = pl::HashCombine(hash, val.start_time_ticks);
    return hash;
  }
};
#endif
