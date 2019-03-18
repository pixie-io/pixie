#include <linux/perf_event.h>
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

// TODO(kgandhi): PL-451 These struct definitions should come
// from a header file. Figure out how to expand the header file
// using preprocessing in pl_cc_resource.

struct val_t {
  uint64_t time_stamp;
  uint64_t run_time;
  char name[TASK_COMM_LEN];
};

BPF_HASH(pid_cpu_time, uint16_t, struct val_t);

int trace_pid_runtime(struct pt_regs* ctx) {
  uint16_t cur_pid = bpf_get_current_pid_tgid() >> 32;

  struct val_t* cur_val = pid_cpu_time.lookup(&cur_pid);
  uint64_t cur_ts = bpf_ktime_get_ns();

  if (cur_val) {
    // Only update the time_stamp and run_time.
    cur_val->run_time += cur_ts - cur_val->time_stamp;
    cur_val->time_stamp = cur_ts;
    return 0;
  }
  // Create a new entry.
  struct val_t new_val = {.time_stamp = cur_ts, .run_time = 0};
  bpf_get_current_comm(&new_val.name, sizeof(new_val.name));
  pid_cpu_time.update(&cur_pid, &new_val);
  return 0;
}
