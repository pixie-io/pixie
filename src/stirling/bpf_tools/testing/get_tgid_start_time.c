#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"

BPF_ARRAY(tgid_start_time_output, uint64_t, 1);

int probe_tgid_start_time(struct pt_regs* ctx) {
  uint64_t start_time_ticks = get_tgid_start_time();
  int kZero = 0;
  tgid_start_time_output.update(&kZero, &start_time_ticks);
  return 0;
}
