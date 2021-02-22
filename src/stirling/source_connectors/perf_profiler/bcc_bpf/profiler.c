// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <linux/bpf_perf_event.h>

#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"

// This BPF probe samples stack-traces using two fundamental data structures:
// 1. stack_traces: a map from stack-trace [1] to stack-trace-id (an integer).
// 2. histogram: a map from stack-trace-id to observation count.
// The higher a count, the more we have observed a particular stack-trace,
// and the more likely something in that stack-trace is a potential perf. issue.

// To keep the stack-trace profiler "always on", we use a double buffering
// scheme wherein we allocate two of each data structure. Therefore,
// we have the following BPF tables:
// 1a. stack_traces_a,
// 1b. stack_traces_b,
// 2a. histogram_a, and
// 2b. histogram_b.

// Periodically, we need to switch over from map-set-a to map-set-b, and vice versa.
// We will conservatively assume that each sample inserts a new key into the stack_traces
// and into the histogram. Also, we will target some nominal time frame (the "push period")
// for the sampling to continue before doing the switch over.

// Given the targeted "push period" and the "sample period", we can find the
// number of entries required to be allocated in each of the maps:
// kMinEntriesPerCPU = push_period / sample_period.
//
// Because sampling occurs per-cpu, the total number of entries required is:
// kMinMapEntries = ncpus * push_period / sample_period = ncpus * kMinEntriesPerCPU.
//
// But, we will include some margin to make sure that hash collisions and
// data races [2] do not cause us to drop data:
// kNumMapEntries = 2 * ncpus * push_period / sample_period = 2 * kMinMapEntries.
//
// The value "kMinMapEntries" is also the sample_threshold,
// i.e. the number of samples after which we switch over to the alternate map-set.
// kSampleThreshold = kNumMapEntries

// Footnotes:
// [1] A stack trace is an (ordered) vector of addresses (u64s), i.e.
// the set of instruction pointers found in the call stack at the moment
// the sample was triggered.
// [2] Once the targeted number of samples have been taken, some CPU
// will notice that this threshold has been crossed. However, in that
// same instant of time, some other CPU(s) may be performing a stack-trace
// sample already. With a sufficiently large stack trace sample period,
// we can expect that the total number of samples (per push period) is
// bounded above by:
// ncpus + ncpus * push_period / sample_period.
// The factor of 2x margin in number is meant to ensure that hash collisions
// do not causes data loss as it surely overprovisions for this data race effect.

// Here, we compute the number of map entries to allocate and the sample threshold,
// per the notes above. NCPS, PUSH_PERIOD, and SAMPLE_PERIOD are given
// by pre-processor defines specified on the compiler command line (e.g. -DNCPUS=24).

#define DIV_ROUND_UP(NUM, DEN) ((NUM + DEN - 1) / DEN)

static const uint32_t kMinEntriesPerCPU = DIV_ROUND_UP(PUSH_PERIOD, SAMPLE_PERIOD);
static const uint32_t kMinMapEntries = NCPUS * kMinEntriesPerCPU;
static const uint32_t kNumMapEntries = 2 * kMinMapEntries;
static const uint32_t kSampleThreshold = kMinMapEntries;

BPF_HASH(histogram_a, struct stack_trace_key_t, uint64_t, kNumMapEntries);
BPF_HASH(histogram_b, struct stack_trace_key_t, uint64_t, kNumMapEntries);
BPF_STACK_TRACE(stack_traces_a, kNumMapEntries);
BPF_STACK_TRACE(stack_traces_b, kNumMapEntries);

// profiler_state: shared state vector between BPF & user space.
// See comments in shared header file "stack_event.h".
BPF_ARRAY(profiler_state, uint64_t, kProfilerStateVectorSize);

int sample_call_stack(struct bpf_perf_event_data* ctx) {
  int bpf_push_count_idx = kBPFPushCountIdx;
  int user_read_and_clear_count_idx = kUserReadAndClearCountIdx;
  int sample_count_idx = kSampleCountIdx;
  int timestamp_idx = kTimeStampIdx;
  int error_status_idx = kErrorStatusIdx;

  uint64_t* push_count_ptr = profiler_state.lookup(&bpf_push_count_idx);
  uint64_t* read_and_clear_count_ptr = profiler_state.lookup(&user_read_and_clear_count_idx);
  uint64_t* sample_count_ptr = profiler_state.lookup(&sample_count_idx);

  const bool push_count_ptr_null = push_count_ptr == NULL;
  const bool read_and_clear_count_ptr_null = read_and_clear_count_ptr == NULL;
  const bool sample_count_ptr_null = sample_count_ptr == NULL;

  if (push_count_ptr_null || read_and_clear_count_ptr_null || sample_count_ptr_null) {
    // One of the map lookups failed.
    // Set the appropriate error bit in the error bitfield:
    uint64_t rd_fail_status_code = kMapReadFailureError;
    profiler_state.update(&error_status_idx, &rd_fail_status_code);
    return 0;
  }

  uint64_t ts_ns = bpf_ktime_get_ns();
  uint64_t ts_ms = ts_ns / 1000 / 1000;

  uint64_t push_count = *push_count_ptr;
  uint64_t read_and_clear_count = *read_and_clear_count_ptr;
  uint64_t sample_count = *sample_count_ptr;

  profiler_state.increment(sample_count_idx);

  uint32_t pid = bpf_get_current_pid_tgid();

  // create map key
  struct stack_trace_key_t key = {.pid = pid};

  bpf_get_current_comm(&key.name, sizeof(key.name));

  if (push_count % 2 == 0) {
    // map set A branch:
    key.user_stack_id = stack_traces_a.get_stackid(&ctx->regs, BPF_F_USER_STACK);
    key.kernel_stack_id = stack_traces_a.get_stackid(&ctx->regs, 0);
    histogram_a.increment(key);
  } else {
    // map set B branch:
    key.user_stack_id = stack_traces_b.get_stackid(&ctx->regs, BPF_F_USER_STACK);
    key.kernel_stack_id = stack_traces_b.get_stackid(&ctx->regs, 0);
    histogram_b.increment(key);
  }

  // sample_count >= kSampleThreshold: indicates the number of samples taken exceeds a threshold
  // beyond which we risk dropping data. The sample threshold is designed to be conservative:
  // 1. we can reasonably expect that fewer map entries were created than samples taken,
  // 2. and, we set the map size at 2x the sample threshold.
  // When the sample threshold is exceeded, we want to send a "data push event"
  // so that the user side can read the data, and clear one of the map sets.
  if (sample_count >= kSampleThreshold) {
    // user_ready_for_push: indicates that the user side is ready to receive a "push event."
    // 1. the data, previously in the disused map set, has been ingested on the user side,
    // 2. and, the disused map set has been cleared.
    // At bootstrap time, both push_count & read_and_clear_count are initialized to zero.
    // After the first push, we have push_count=1 and read_and_clear_count=0.
    // Once the user side complete its read and clear, then push_count=read_and_clear_count=1.
    // Thus, in steady state, these counts should differ by at most one,
    // with push_count >= read_and_clear_count. When the counts are equal,
    // the user side is caught up and ready to receive.
    const bool user_ready_for_push = (push_count == read_and_clear_count);

    if (!user_ready_for_push) {
      // Could not push: this is not expected.
      // Based on setting a sufficiently large "push period" (time interval between
      // data push events) we expect that "ready for push" is always satisified
      // when the sample count crosses the sample threshold.
      // Should this condition not be met, i.e. should we not be able to send a push event,
      // 1. mark the error status bitfield with the "could not push" bit set.
      // 2. continue using the "currently in use map set." This should have no adverse
      // consequence beyond the risk of dropping some stack trace samples.
      uint64_t push_fail_status_code = kCouldNotPushError;
      profiler_state.update(&error_status_idx, &push_fail_status_code);
      return 0;
    }

    // Here, we meet both conditions: sample threshold exceeded and ready for push.
    // Update the shared state vector: increment push count & reset the sample count.
    // By incrementing the push count, user space will know that a read & clear op. is required.

    uint64_t kZero = 0;
    uint64_t ts_ns = bpf_ktime_get_ns();

    // TODO(jps): Do we need to be robust to even more sophisticated corner cases, e.g.:
    // What happens if two CPUs are both going to enter this branch (nominally incrementing
    // push_count), but the user-space code starts consuming after the first CPU moves the count and
    // before the other CPU has updated the maps (stack-traces & histogram)?
    // In this case, we risk losing some information... and worse maybe the non_atomic_clear()
    // will not actually end up with an empty map.
    // We could have an internal "map switchover count" and published "push count" to delay
    // the "push" by one sample period.

    // Cannot use map.increment() for push count because of data races:
    // it is possible for two CPUs to arrive here for the same logical push.
    // Here, we make sure that "both pushes" land in user space with the
    // same push count. User space will filter the second one of these that it processes.
    uint64_t next_push_count = 1 + push_count;
    profiler_state.update(&bpf_push_count_idx, &next_push_count);
    profiler_state.update(&sample_count_idx, &kZero);
    profiler_state.update(&timestamp_idx, &ts_ns);
  }

  return 0;
}
