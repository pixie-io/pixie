// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <linux/ptrace.h>

#include "src/stirling/bcc_bpf/utils.h"
#include "src/stirling/bcc_bpf_interface/proc_trace.h"

BPF_PERF_OUTPUT(proc_creation_events);

static __inline void ret_probe(struct pt_regs* ctx) {
  struct proc_creation_event_t event = {
      .timestamp_ns = bpf_ktime_get_ns(),
      .ppid = bpf_get_current_pid_tgid() >> 32,
      .pid = PT_REGS_RC(ctx),
  };
  proc_creation_events.perf_submit(ctx, &event, sizeof(event));
}

// fork() seems a user land wrapper of clone().
void syscall__probe_ret_clone(struct pt_regs* ctx) { ret_probe(ctx); }

void syscall__probe_ret_vfork(struct pt_regs* ctx) { ret_probe(ctx); }
