#pragma once

// TODO(kgandhi): PL-451
// TASK_COMM_LEN seems to be undefined so hardcoding to 16 for now.
struct pidruntime_val_t {
  uint64_t timestamp;
  uint64_t run_time;
  char name[16];
};
