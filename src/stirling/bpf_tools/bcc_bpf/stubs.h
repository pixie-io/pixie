// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.
// List of stubs of BCC/BPF helper functions.

#pragma once

#ifdef __cplusplus

#include "src/common/base/base.h"

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
