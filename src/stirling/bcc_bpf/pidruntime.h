#pragma once
#ifdef __linux__
#include <linux/sched.h>
#include <stdint.h>

// TODO(kgandhi): PL-451
// TASK_COMM_LEN seems to be undefined so hardcoding to 16 for now.
struct pl_stirling_bcc_pidruntime_val {
  uint64_t timestamp;
  uint64_t run_time;
  char name[16];
};
#endif
