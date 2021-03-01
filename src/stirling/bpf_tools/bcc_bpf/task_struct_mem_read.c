// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include "src/stirling/bpf_tools/bcc_bpf_intf/types.h"

// Map that holds a copy of the raw memory of the task struct.
BPF_ARRAY(task_struct_buf, struct buf, 1);

// Map that holds the address of the task struct.
// Used to look for a task_struct pointer to itself,
// which would be an indication of the group leader.
BPF_ARRAY(task_struct_address_map, uint64_t, 1);

int task_struct_probe(struct pt_regs* ctx) {
  int kIndex = 0;

  // Get the task struct.
  struct task_struct* task = (struct task_struct*)bpf_get_current_task();

  // Copy the raw memory of the task_struct.
  struct buf* buf = task_struct_buf.lookup(&kIndex);
  if (buf == NULL) {
    return 0;
  }
  bpf_probe_read(buf, sizeof(struct buf), task);

  // Copy the task struct address.
  uint64_t task_addr = (uint64_t)task;
  task_struct_address_map.update(&kIndex, &task_addr);

  return 0;
}
