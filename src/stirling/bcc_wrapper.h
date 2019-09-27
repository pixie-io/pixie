#pragma once

#ifdef __linux__

#include <bcc/BPF.h>
#include <linux/perf_event.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "src/stirling/bcc_bpf_interface/log_event.h"
}

#include "src/common/base/base.h"

DECLARE_uint32(stirling_bpf_perf_buffer_page_count);
DECLARE_bool(stirling_bpf_enable_logging);

namespace pl {
namespace stirling {

/**
 * Describes a kernel probe (kprobe).
 * Currently only works for syscalls.
 */
struct ProbeSpec {
  // Name of kernel function to probe (currently must be syscall).
  std::string_view kernel_fn_short_name;

  // Name of user-provided function to run when event is triggered.
  std::string_view trace_fn_name;

  // Whether this is an ENTRY or RETURN probe.
  bpf_probe_attach_type attach_type;
};

/**
 * Describes a perf buffer used in BCC code, through which data is returned to user-space.
 */
struct PerfBufferSpec {
  // Name of the perf buffer.
  // Must be the same as the perf buffer name declared in the probe code with BPF_PERF_OUTPUT.
  std::string_view name;

  // Function that will be called for every event in the perf buffer,
  // when perf buffer read is triggered.
  perf_reader_raw_cb probe_output_fn;

  // Function that will be called if there are lost/clobbered perf events.
  perf_reader_lost_cb probe_loss_fn;
};

/**
 * Describes a perf event to attach.
 * This can be run stand-alone and is not dependent on kProbes.
 */
struct PerfEventSpec {
  // The type of perf event (e.g. PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, etc.)
  uint32_t event_type;

  // The actual event to be counted (e.g. PERF_COUNT_HW_CPU_CYCLES).
  uint32_t event_config;

  // Name of user-provided function to run when event is triggered.
  std::string_view probe_func;

  // Sampling period in number of events.
  // Mutually exclusive with sample_freq.
  // TODO(oazizi): Even though BCC does it this way, we can have a better scheme.
  uint64_t sample_period;

  // Sampling frequency in Hz to trigger the probe.
  // Kernel will try to modulate the sample period to achieve the desired frequency.
  // Mutually exclusive with sample_period.
  uint64_t sample_freq;
};

/**
 * Wrapper around BCC, as a convenience.
 */
class BCCWrapper {
 public:
  inline static const size_t kCPUCount = ebpf::BPFTable::get_possible_cpu_count();

  BCCWrapper() = delete;

  /**
   * Constructor, which takes source code as an input.
   * @param bpf_program a string_view to the source code to compile.
   */
  explicit BCCWrapper(const std::string_view bpf_program) : bpf_program_(bpf_program) {}

  ~BCCWrapper() {
    // Not really required, because BPF destructor handles these.
    // But we do it anyways.
    DetachPerfEvents();
    ClosePerfBuffers();
    DetachProbes();
  }

  /**
   * @brief Compiles the BPF code.
   * @param cflags compiler flags.
   * @return error if no root access, or code could not be compiled.
   */
  Status InitBPFCode(const std::vector<std::string>& cflags);
  Status InitBPFCode() { return InitBPFCode(std::vector<std::string>()); }

  /**
   * @brief Convenience function that attaches multiple probes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachProbes(const ArrayView<ProbeSpec>& probes);

  /**
   * @brief Detaches all probes that were attached by the wrapper.
   * If any probe fails to detach, an error is logged, and the function continues.
   */
  void DetachProbes();

  /**
   * @brief Convenience function that opens multiple perf buffers.
   * @param probes Vector of perf buffer descriptors.
   * @param cb_cookie Raw pointer returned on callback, typically used for tracking context.
   * @return Error of first failure (remaining perf buffer opens are not attempted).
   */
  Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers, void* cb_cookie);

  /**
   * @brief Detaches all perf buffers that were opened by the wrapper.
   * If any perf buffer fails to close, an error is logged, and the function continues.
   */
  void ClosePerfBuffers();

  /**
   * @brief Convenience function that opens multiple perf events.
   * @param probes Vector of perf event descriptors.
   * @return Error of first failure (remaining perf event attaches are not attempted).
   */
  Status AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events);

  /**
   * @brief Detaches all perf buffers that were opened by the wrapper.
   * If any perf buffer fails to close, an error is logged, and the function continues.
   */
  void DetachPerfEvents();

  /**
   * @brief Dumps BPF logging events through GLOG logging facility.
   */
  void DumpBPFLog();

  /**
   * @brief Drains the perf buffer, calling the handle function that was
   * specified in the PerfBufferSpec when OpenPerfBuffer was called.
   */
  void PollPerfBuffer(std::string_view perf_buffer_name, int timeout_ms = 1);

  /**
   * Provide access to the BPF instance, for direct access.
   * Eventually, this should go away, and everything should
   * go through the API in the rest of this class.
   *
   * @return reference to the underlying BPF instance.
   */
  // TODO(oazizi): Try to get rid of this function, since it bypasses the wrapper.
  ebpf::BPF& bpf() { return bpf_; }

  // These are static counters of attached/open probes across all instances.
  // It is meant for verification that we have cleaned-up all resources in tests.
  static size_t num_attached_probes() { return num_attached_probes_; }
  static size_t num_open_perf_buffers() { return num_open_perf_buffers_; }
  static size_t num_attached_perf_events() { return num_attached_perf_events_; }

 private:
  Status InitLogging();
  Status AttachProbe(const ProbeSpec& probe);
  Status DetachProbe(const ProbeSpec& probe);
  Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer, void* cb_cookie);
  Status ClosePerfBuffer(const PerfBufferSpec& perf_buffer);
  Status AttachPerfEvent(const PerfEventSpec& perf_event);
  Status DetachPerfEvent(const PerfEventSpec& perf_event);

  std::string_view bpf_program_;
  std::vector<ProbeSpec> probes_;
  std::vector<PerfBufferSpec> perf_buffers_;
  std::vector<PerfEventSpec> perf_events_;
  bool logging_enabled_ = false;

  ebpf::BPF bpf_;

  // These are static counters across all instances, because:
  // 1) We want to ensure we have cleaned all BPF resources up across *all* instances (no leaks).
  // 2) It is for verification only, and it doesn't make sense to create accessors from stirling to
  // here.
  inline static size_t num_attached_probes_;
  inline static size_t num_open_perf_buffers_;
  inline static size_t num_attached_perf_events_;
};

}  // namespace stirling
}  // namespace pl

#endif
