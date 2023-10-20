
#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"

BPF_ARRAY(cgroup_id_output, uint64_t, 1);

int probe_bpf_get_current_cgroup_id(struct pt_regs* ctx) {
  uint64_t cgroup_id = UINT64_MAX;
#if GET_CGROUP_ID_ENABLED
  cgroup_id = bpf_get_current_cgroup_id();
#endif
  int kZero = 0;
  cgroup_id_ouptut.update(&kZero, &cgroup_id);
  return 0;
}

