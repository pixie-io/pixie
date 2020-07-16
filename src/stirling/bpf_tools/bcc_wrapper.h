#pragma once

#ifdef __linux__

#include <bcc/BPF.h>
// Including bcc/BPF.h creates some conflicts with llvm.
// So must remove this stray define for things to work.
#ifdef STT_GNU_IFUNC
#undef STT_GNU_IFUNC
#endif

// Including bcc/BPF.h creates some conflicts with our own code.
#ifdef DECLARE_ERROR
#undef DECLARE_ERROR
#endif

#include <linux/perf_event.h>

#include <gtest/gtest_prod.h>

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/obj_tools/elf_tools.h"

// Provides a string view into a char array included in the binary via objcopy.
// Useful for include BPF programs that are copied into the binary.
#define OBJ_STRVIEW(varname, objname)     \
  extern char objname##_start;            \
  extern char objname##_end;              \
  inline const std::string_view varname = \
      std::string_view(&objname##_start, &objname##_end - &objname##_start);

// Macro to load BPF source code embedded in object files.
// See 'pl_bpf_cc_resource' bazel rule to see how these are generated.
#define BCC_SRC_STRVIEW(varname, build_label) OBJ_STRVIEW(varname, _binary_##build_label##_bpf_src);

DECLARE_uint32(stirling_bpf_perf_buffer_page_count);

namespace pl {
namespace stirling {
namespace bpf_tools {

enum class BPFProbeAttachType {
  // Attach to function entry.
  kEntry = BPF_PROBE_ENTRY,
  // Attach to function return (BCC native way, using stack).
  kReturn = BPF_PROBE_RETURN,
  // Attach to all function return instructions (required for golang).
  kReturnInsts,
};

/**
 * Describes a kernel probe (kprobe).
 */
struct KProbeSpec {
  // Name of kernel function to probe (currently must be syscall).
  std::string_view kernel_fn;

  // Whether this is an ENTRY or RETURN probe.
  BPFProbeAttachType attach_type = BPFProbeAttachType::kEntry;

  // Name of user-provided function to run when event is triggered.
  std::string_view probe_fn;
};

/**
 * Describes a userspace probe (uprobe).
 */
struct UProbeSpec {
  // The canonical path to the binary to which this uprobe is attached.
  std::filesystem::path binary_path;
  // Exact one of symbol and address must be specified.
  std::string symbol;
  uint64_t address = 0;
  BPFProbeAttachType attach_type = BPFProbeAttachType::kEntry;
  std::string probe_fn;

  std::string ToString() const {
    return absl::Substitute("[binary=$0 symbol=$1 address=$2 type=$3 probe=$4]",
                            binary_path.string(), symbol, address,
                            magic_enum::enum_name(attach_type), probe_fn);
  }
};

/**
 * Describes a uprobe template.
 *
 * As uprobes can be attached to arbitrary binary matching certain symbol pattern, it is not
 * meaningful to include binary_path.
 *
 * Also this allows to use literal types, so we can keep these as constexpr.
 */
// TODO(yzhao): This should be updated after D2844 is landed.
struct UProbeTmpl {
  std::string_view symbol;
  elf_tools::SymbolMatchType match_type;
  std::string_view probe_fn;
  BPFProbeAttachType attach_type = BPFProbeAttachType::kEntry;
};

/**
 * Describes a BPF perf buffer, through which data is returned to user-space.
 */
struct PerfBufferSpec {
  // Name of the perf buffer.
  // Must be the same as the perf buffer name declared in the probe code with BPF_PERF_OUTPUT.
  std::string name;

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
  perf_type_id type;

  // The actual event to be counted (e.g. PERF_COUNT_HW_CPU_CYCLES).
  uint32_t config;

  // Name of user-provided function to run when event is triggered.
  std::string_view probe_fn;

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

  ~BCCWrapper() {
    // Not really required, because BPF destructor handles these.
    // But we do it anyways out of paranoia.
    Stop();
  }

  /**
   * @brief Compiles the BPF code.
   * @param cflags compiler flags.
   * @return error if no root access, or code could not be compiled.
   */
  Status InitBPFProgram(std::string_view bpf_program, const std::vector<std::string>& cflags = {});

  /**
   * @brief Attach a single kprobe.
   * @param probe Specifications of the kprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  Status AttachKProbe(const KProbeSpec& probe);

  /**
   * @brief Attach a single uprobe.
   * @param probe Specifications of the uprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  Status AttachUProbe(const UProbeSpec& probe);

  /**
   * @brief Open a perf buffer for reading events.
   * @param perf_buff Specifications of the perf buffer (name, callback function, etc.).
   * @param cb_cookie A pointer that is sent to the callback function when triggered by
   * PollPerfBuffer().
   * @return Error if perf buffer cannot be opened (e.g. perf buffer does not exist).
   */
  Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer, void* cb_cookie = nullptr);

  /**
   * @brief Attach a perf event, which runs a probe every time a perf counter reaches a threshold
   * condition.
   * @param perf_event Specification of the perf event and its sampling frequency.
   * @return Error if the perf event could not be attached.
   */
  Status AttachPerfEvent(const PerfEventSpec& perf_event);

  /**
   * @brief Convenience function that attaches multiple kprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachKProbes(const ArrayView<KProbeSpec>& probes);

  /**
   * @brief Convenience function that attaches multiple uprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachUProbes(const ArrayView<UProbeSpec>& uprobes);

  /**
   * @brief Convenience function that opens multiple perf buffers.
   * @param probes Vector of perf buffer descriptors.
   * @param cb_cookie Raw pointer returned on callback, typically used for tracking context.
   * @return Error of first failure (remaining perf buffer opens are not attempted).
   */
  Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers, void* cb_cookie);

  /**
   * @brief Convenience function that opens multiple perf events.
   * @param probes Vector of perf event descriptors.
   * @return Error of first failure (remaining perf event attaches are not attempted).
   */
  Status AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events);

  /**
   * @brief Drains the perf buffer, calling the handle function that was
   * specified in the PerfBufferSpec when OpenPerfBuffer was called.
   */
  void PollPerfBuffer(std::string_view perf_buffer_name, int timeout_ms = 1);

  /**
   * @brief Detaches all probes, and closes all perf buffers that are open.
   */
  void Stop();

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
  static size_t num_attached_probes() { return num_attached_kprobes_ + num_attached_uprobes_; }
  static size_t num_open_perf_buffers() { return num_open_perf_buffers_; }
  static size_t num_attached_perf_events() { return num_attached_perf_events_; }

 private:
  FRIEND_TEST(BCCWraperTest, DetachUProbe);

  Status DetachKProbe(const KProbeSpec& probe);
  Status DetachUProbe(const UProbeSpec& probe);
  Status ClosePerfBuffer(const PerfBufferSpec& perf_buffer);
  Status DetachPerfEvent(const PerfEventSpec& perf_event);

  // Detaches all kprobes/uprobes/perf buffers/perf events that were attached by the wrapper.
  // If any fails to detach, an error is logged, and the function continues.
  void DetachKProbes();
  void DetachUProbes();
  void ClosePerfBuffers();
  void DetachPerfEvents();

  std::vector<KProbeSpec> kprobes_;
  std::vector<UProbeSpec> uprobes_;
  std::vector<PerfBufferSpec> perf_buffers_;
  std::vector<PerfEventSpec> perf_events_;

  ebpf::BPF bpf_;

  // These are static counters across all instances, because:
  // 1) We want to ensure we have cleaned all BPF resources up across *all* instances (no leaks).
  // 2) It is for verification only, and it doesn't make sense to create accessors from stirling to
  // here.
  inline static size_t num_attached_kprobes_;
  inline static size_t num_attached_uprobes_;
  inline static size_t num_open_perf_buffers_;
  inline static size_t num_attached_perf_events_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
