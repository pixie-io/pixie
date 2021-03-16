#pragma once

#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"

// TODO(jps): add a macro that wraps bpf_trace_printk for debug & no-ops for prod builds.

// Indices into the profiler shared state vector "profiler_state":
// profiler_state[0]: push count              # written on BPF side, read on user side
// profiler_state[1]: read & clear count      # written on user side, read on BPF side
// profiler_state[2]: sample count            # reset after each push, BPF side only
// profiler_state[3]: timestamp of sample     # written on BPF side, read on user side
// profiler_state[4]: error status bitfield   # written on BPF side, read on user side
static const uint32_t kBPFPushCountIdx = 0;
static const uint32_t kUserReadAndClearCountIdx = 1;
static const uint32_t kSampleCountIdx = 2;
static const uint32_t kTimeStampIdx = 3;
static const uint32_t kErrorStatusIdx = 4;
static const uint32_t kProfilerStateVectorSize = 5;

// stack_trace_key_t indexes into the stack-trace histogram.
// By tying together the user & kernel stack-trace-ids [1],
// it fully identifies a unique stack trace.
//
// [1] user & kernel stack trace ids are tracked separately (the kernel creates
// user & kernel stacks separately because of address aliasing).
struct stack_trace_key_t {
  struct upid_t upid;

  // user_stack_id, an index into the stack-traces map.
  int user_stack_id;

  // kernel_stack_id, an index into the stack-traces map.
  int kernel_stack_id;
};

// Bit positions in the error status bitfield:
static const uint32_t kCouldNotPushBitPos = 0;
static const uint32_t kMapReadFailureBitPos = 1;

// The error codes, themselves:
static const uint64_t kCouldNotPushError = 1ULL << kCouldNotPushBitPos;
static const uint64_t kMapReadFailureError = 1ULL << kMapReadFailureBitPos;
