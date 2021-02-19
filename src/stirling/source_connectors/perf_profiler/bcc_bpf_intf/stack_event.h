#pragma once

// TODO(jps): add a macro that wraps bpf_trace_printk for debug & no-ops for prod builds.

// Indices into the profiler shared state vector:
// e.g. state_vector[0] is the push count.
static const uint32_t kBPFPushCountIdx = 0;
static const uint32_t kUserReadAndClearCountIdx = 1;
static const uint32_t kSampleCountIdx = 2;
static const uint32_t kErrorStatusIdx = 3;

// stack_trace_key_t indexes into the stack-trace histogram.
// By tying together the user & kernel stack-trace-ids [1],
// it fully identifies a unique stack trace.
//
// [1] user & kernel stack trace ids are tracked separately (the kernel creates
// user & kernel stacks separately because of address aliasing).
struct stack_trace_key_t {
  // TODO(jps): re-evaluate the fields in this key.
  // Currently, fields inspired  by iovisor/bcc/tools/profile.py.
  // as a future work, should we:
  // ... 1. use UPID, i.e. pid+timestamp?
  // ... 2. remove char name[TASK_COMM_LEN] (subsumed by (1) above)?
  unsigned int pid;

  // user_stack_id, an index into the stack-traces map.
  int user_stack_id;

  // kernel_stack_id, an index into the stack-traces map.
  int kernel_stack_id;

  // name, the name of the executable command
  char name[16 /*TASK_COMM_LEN*/];
};

// Bit positions in the error status bitfield:
static const uint32_t kCouldNotPushBitPos = 0;
static const uint32_t kMapReadFailureBitPos = 1;

// The error codes, themselves:
static const uint64_t kCouldNotPushError = 1ULL << kCouldNotPushBitPos;
static const uint64_t kMapReadFailureError = 1ULL << kMapReadFailureBitPos;
