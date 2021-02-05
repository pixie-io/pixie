// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

// Placeholder taken from experimental code.
// TODO(jps): Implement more fully.

BPF_HASH(counts, struct stack_trace_key_t, u64);
BPF_STACK_TRACE(stack_traces, 16384);

int sample_call_stack(struct bpf_perf_event_data* ctx) {
  u32 pid = bpf_get_current_pid_tgid();

  // create map key
  struct stack_trace_key_t key = {.pid = pid};

  bpf_get_current_comm(&key.name, sizeof(key.name));
  key.user_stack_id = stack_traces.get_stackid(&ctx->regs, BPF_F_USER_STACK);
  key.kernel_stack_id = stack_traces.get_stackid(&ctx->regs, 0);
  counts.increment(key);
  return 0;
}
