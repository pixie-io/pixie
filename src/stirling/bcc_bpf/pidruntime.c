#include <linux/perf_event.h>
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

#include "src/stirling/bcc_bpf_interface/pidruntime.h"

BPF_HASH(pid_cpu_time, uint16_t, struct pidruntime_val_t);

int trace_pid_runtime(struct pt_regs* ctx) {
  uint16_t cur_pid = bpf_get_current_pid_tgid() >> 32;

  struct pidruntime_val_t* cur_val = pid_cpu_time.lookup(&cur_pid);
  uint64_t cur_ts = bpf_ktime_get_ns();

  if (cur_val) {
    // Only update the timestamp and run_time.
    cur_val->run_time += cur_ts - cur_val->timestamp;
    cur_val->timestamp = cur_ts;
    return 0;
  }
  // Create a new entry.
  struct pidruntime_val_t new_val = {.timestamp = cur_ts, .run_time = 0};
  bpf_get_current_comm(&new_val.name, sizeof(new_val.name));
  pid_cpu_time.update(&cur_pid, &new_val);
  return 0;
}
