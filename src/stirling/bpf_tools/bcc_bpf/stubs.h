// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.
// List of stubs of BCC/BPF helper functions.

#pragma once

#ifdef __cplusplus

#include <arpa/inet.h>
#include <linux/sched.h>

#include "src/common/base/base.h"

#define NSEC_PER_SEC 1000000000L
#define USER_HZ 100

// Kernel headers do not make this available.
struct task_struct {
  struct task_struct* group_leader;
  uint64_t real_start_time;
};

inline uint64_t div_u64(uint64_t l, uint64_t r) { return l / r; }

inline struct task_struct* bpf_get_current_task() {
  DCHECK(false) << "bpf_get_current_task() is not implemented";
  return NULL;
}

template <typename TDestCharType, typename TSrcCharType>
inline void bpf_probe_read(TDestCharType* destination, size_t len, TSrcCharType* src) {
  memcpy(destination, src, len);
}

inline int32_t bpf_ntohl(int32_t val) { return ntohl(val); }

#endif
