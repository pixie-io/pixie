/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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
#include "src/common/json/json.h"
#include "src/stirling/bpf_tools/probe_specs/probe_specs.h"
#include "src/stirling/bpf_tools/task_struct_resolver.h"
#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
/*
 * Status adapter for ebpf::StatusTuple.
 */
template <>
inline Status StatusAdapter<ebpf::StatusTuple>(const ebpf::StatusTuple& s) noexcept {
  if (s.ok()) {
    return Status::OK();
  }
  return Status(statuspb::INTERNAL, s.msg());
}
}  // namespace px

namespace px {
namespace stirling {
namespace bpf_tools {

/**
 * Wrapper around BCC, as a convenience.
 */
class BCCWrapper {
 public:
  inline static const size_t kCPUCount = ebpf::BPFTable::get_possible_cpu_count();

  /**
   * Returns the globally-shared TaskStructOffsets object.
   * The task_struct offset resolution has to be performed the first time, and if successful,
   * the obtained result will be cached and reused afterwards.
   */
  static StatusOr<utils::TaskStructOffsets> ComputeTaskStructOffsets();

  /**
   * Returns the stored offset object.
   * This is used by ProcExitConnector to write the exit_code offset value to BPF array.
   */
  static const std::optional<utils::TaskStructOffsets>& task_struct_offsets_opt() {
    return task_struct_offsets_opt_;
  }

  ~BCCWrapper() {
    // Not really required, because BPF destructor handles these.
    // But we do it anyways out of paranoia.
    Close();
  }

  /**
   * Compiles the BPF code.
   * @param bpf_program The BPF code to compile.
   * @param cflags compiler flags.
   * @param requires_linux_headers Search for local headers, or attempt installation of
   *                               packaged headers if available.
   * @param always_infer_task_struct_offsets When true, run the task_struct offset resolver even
   *                                         when local/host headers are found.
   * @return error if no root access, code could not be compiled, or required linux headers are not
   *               available.
   */
  Status InitBPFProgram(std::string_view bpf_program, std::vector<std::string> cflags = {},
                        bool requires_linux_headers = true,
                        bool always_infer_task_struct_offsets = false);

  /**
   * Attach a single kprobe.
   * @param probe Specifications of the kprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  Status AttachKProbe(const KProbeSpec& probe);

  /**
   * Attach a single uprobe.
   * @param probe Specifications of the uprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  Status AttachUProbe(const UProbeSpec& probe);

  /**
   * Attach a single tracepoint
   * @param probe Specifications of the tracepoint (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  Status AttachTracepoint(const TracepointSpec& probe);

  /**
   * Attach a single sampling probe.
   * @param probe Specifications of the probe (bpf function and sampling frequency).
   * @return Error if probe fails to attach.
   */
  Status AttachSamplingProbe(const SamplingProbeSpec& probe);

  /**
   * Open a perf buffer for reading events.
   * @param perf_buff Specifications of the perf buffer (name, callback function, etc.).
   * @param cb_cookie A pointer that is sent to the callback function when triggered by
   * PollPerfBuffer().
   * @return Error if perf buffer cannot be opened (e.g. perf buffer does not exist).
   */
  Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer, void* cb_cookie = nullptr);

  /**
   * Attach a perf event, which runs a probe every time a perf counter reaches a threshold
   * condition.
   * @param perf_event Specification of the perf event and its sampling frequency.
   * @return Error if the perf event could not be attached.
   */
  Status AttachPerfEvent(const PerfEventSpec& perf_event);

  /**
   * Convenience function that attaches multiple kprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachKProbes(const ArrayView<KProbeSpec>& probes);

  /**
   * Convenience function that attaches multiple tracepoints.
   * @param probes Vector of TracepointSpec.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachTracepoints(const ArrayView<TracepointSpec>& probes);

  /**
   * Convenience function that attaches multiple uprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachUProbes(const ArrayView<UProbeSpec>& uprobes);

  /**
   * Convenience function that attaches multiple uprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  Status AttachSamplingProbes(const ArrayView<SamplingProbeSpec>& probes);

  /**
   * Convenience function that attaches a XDP program.
   */
  Status AttachXDP(const std::string& dev_name, const std::string& fn_name);

  /**
   * Convenience function that opens multiple perf buffers.
   * @param probes Vector of perf buffer descriptors.
   * @param cb_cookie Raw pointer returned on callback, typically used for tracking context.
   * @return Error of first failure (remaining perf buffer opens are not attempted).
   */
  Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers, void* cb_cookie);

  /**
   * Convenience function that opens multiple perf events.
   * @param probes Vector of perf event descriptors.
   * @return Error of first failure (remaining perf event attaches are not attempted).
   */
  Status AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events);

  /**
   * Convenience function that populates a BPFPerfEventArray (aka BPF_PERF_ARRAY), used to directly
   * read CPU perf counters from within a BPF program. If the counts read from said
   * counters are needed on the user side (vs. BPF side), then another shared array
   * or map is required to store those values.
   * @param table_name The name of the BPF_PERF_ARRAY from its declarion in the BPF program.
   * @param type PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, etc...
   * @param config PERF_COUNT_HW_CPU_CYCLES, PERF_COUNT_HW_INSTRUCTIONS, etc...
   * @return Error status.
   */
  Status PopulateBPFPerfArray(const std::string& table_name, const uint32_t type,
                              const uint64_t config) {
    PX_RETURN_IF_ERROR(bpf_.open_perf_event(table_name, type, config));
    return Status::OK();
  }

  /**
   * Drains all of the opened perf buffers, calling the handle function that was
   * specified in the PerfBufferSpec when OpenPerfBuffer was called.
   *
   * @param timeout_ms If there's no event in the perf buffer, then timeout_ms specifies the
   *                   amount of time to wait for an event to arrive before returning.
   *                   Default is 0, because if nothing is ready, then we want to go back to sleep
   *                   and catch new events in the next iteration.
   */
  void PollPerfBuffers(int timeout_ms = 0);

  /**
   * Detaches all probes, and closes all perf buffers that are open.
   */
  void Close();

  template <typename TKeyType, typename TValueType>
  ebpf::BPFHashTable<TKeyType, TValueType> GetHashTable(const std::string& table_name) {
    return bpf_.get_hash_table<TKeyType, TValueType>(table_name);
  }

  template <typename TValueType>
  ebpf::BPFArrayTable<TValueType> GetArrayTable(const std::string& table_name) {
    return bpf_.get_array_table<TValueType>(table_name);
  }

  ebpf::BPFStackTable GetStackTable(const std::string& table_name) {
    return bpf_.get_stack_table(table_name);
  }

  template <typename TKeyType>
  ebpf::BPFMapInMapTable<TKeyType> GetMapInMapTable(const std::string& table_name) {
    return bpf_.get_map_in_map_table<TKeyType>(table_name);
  }

  ebpf::BPFPerfBuffer* GetPerfBuffer(const std::string& perf_buffer_name) {
    return bpf_.get_perf_buffer(perf_buffer_name);
  }

  template <typename TValueType>
  ebpf::BPFPercpuArrayTable<TValueType> GetPerCPUArrayTable(const std::string& table_name) {
    return bpf_.get_percpu_array_table<TValueType>(table_name);
  }

  // These are static counters of attached/open probes across all instances.
  // It is meant for verification that we have cleaned-up all resources in tests.
  static size_t num_attached_probes() { return num_attached_kprobes_ + num_attached_uprobes_; }
  static size_t num_open_perf_buffers() { return num_open_perf_buffers_; }
  static size_t num_attached_perf_events() { return num_attached_perf_events_; }

 private:
  FRIEND_TEST(BCCWrapperTest, DetachUProbe);

  Status DetachKProbe(const KProbeSpec& probe);
  Status DetachUProbe(const UProbeSpec& probe);
  Status DetachTracepoint(const TracepointSpec& probe);
  Status ClosePerfBuffer(const PerfBufferSpec& perf_buffer);
  Status DetachPerfEvent(const PerfEventSpec& perf_event);
  void PollPerfBuffer(std::string_view perf_buffer_name, int timeout_ms);

  // Detaches all kprobes/uprobes/perf buffers/perf events that were attached by the wrapper.
  // If any fails to detach, an error is logged, and the function continues.
  void DetachKProbes();
  void DetachUProbes();
  void DetachTracepoints();
  void ClosePerfBuffers();
  void DetachPerfEvents();

  // Returns the name that identifies the target to attach this k-probe.
  std::string GetKProbeTargetName(const KProbeSpec& probe);

  std::vector<KProbeSpec> kprobes_;
  std::vector<UProbeSpec> uprobes_;
  std::vector<TracepointSpec> tracepoints_;
  std::vector<PerfBufferSpec> perf_buffers_;
  std::vector<PerfEventSpec> perf_events_;

  std::string system_headers_include_dir_;

  // Initialize this with one of the below bitmask flags to turn on different debug output.
  // For example, bpf_{0x2} instructs to print the BPF bytecode.
  // See https://github.com/iovisor/bcc/blob/master/src/cc/bpf_module.h for the effects of these
  // flags.
  //   DEBUG_LLVM_IR = 0x1,
  //   DEBUG_BPF = 0x2,
  //   DEBUG_PREPROCESSOR = 0x4,
  //   DEBUG_SOURCE = 0x8,
  //   DEBUG_BPF_REGISTER_STATE = 0x10,
  //   DEBUG_BTF = 0x20,
  ebpf::BPF bpf_;

  // These are static counters across all instances, because:
  // 1) We want to ensure we have cleaned all BPF resources up across *all* instances (no leaks).
  // 2) It is for verification only, and it doesn't make sense to create accessors from stirling to
  // here.
  inline static size_t num_attached_kprobes_;
  inline static size_t num_attached_uprobes_;
  inline static size_t num_attached_tracepoints_;
  inline static size_t num_open_perf_buffers_;
  inline static size_t num_attached_perf_events_;

 private:
  // This is shared by all source connectors that uses BCCWrapper.
  inline static std::optional<utils::TaskStructOffsets> task_struct_offsets_opt_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
