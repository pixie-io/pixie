#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

BPF_HASH(pid_to_cpu, pid_t, int);
BPF_HASH(pid_to_ts, pid_t, uint64_t);
BPF_HASH(cpu_time, int, uint64_t);

int task_switch_event(struct pt_regs *ctx, struct task_struct *prev) {
  pid_t prev_pid = prev->pid;
  int* prev_cpu = pid_to_cpu.lookup(&prev_pid);
  uint64_t* prev_ts = pid_to_ts.lookup(&prev_pid);

  pid_t cur_pid = bpf_get_current_pid_tgid();
  int cur_cpu = bpf_get_smp_processor_id();
  uint64_t cur_ts = bpf_ktime_get_ns();

  uint64_t this_cpu_time = 0;
  if (prev_ts) {
    pid_to_ts.delete(&prev_pid);
    this_cpu_time = (cur_ts - *prev_ts);
  }
  if (prev_cpu) {
    pid_to_cpu.delete(&prev_pid);
    if (this_cpu_time > 0) {
      int cpu_key = *prev_cpu;
      uint64_t* history_time = cpu_time.lookup(&cpu_key);
      if (history_time)
        this_cpu_time += *history_time;
      cpu_time.update(&cpu_key, &this_cpu_time);
    }
  }

  pid_to_cpu.update(&cur_pid, &cur_cpu);
  pid_to_ts.update(&cur_pid, &cur_ts);

  return 0;
}
