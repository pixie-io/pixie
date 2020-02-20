#pragma once

struct proc_creation_event_t {
  uint64_t timestamp_ns;
  // Parent pid.
  uint32_t ppid;
  uint32_t pid;
};
