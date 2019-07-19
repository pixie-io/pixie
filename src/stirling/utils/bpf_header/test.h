#pragma once
#ifdef __linux__

struct pidruntime_val_t {
  uint64_t timestamp;
  uint64_t run_time;
  char name[16];
};
#endif
