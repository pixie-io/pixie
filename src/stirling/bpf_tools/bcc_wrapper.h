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

#include <absl/container/flat_hash_set.h>
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
#include "src/stirling/bpf_tools/rr/rr.h"
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
  virtual ~BCCWrapper() {}
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

  virtual StatusOr<ebpf::BPF*> BPF() = 0;
  virtual StatusOr<BPFRecorder*> GetBPFRecorder() const = 0;
  virtual StatusOr<BPFReplayer*> GetBPFReplayer() const = 0;
  virtual bool IsRecording() const = 0;
  virtual bool IsReplaying() const = 0;

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
  virtual Status InitBPFProgram(std::string_view bpf_program, std::vector<std::string> cflags = {},
                                bool requires_linux_headers = true,
                                bool always_infer_task_struct_offsets = false) = 0;

  /**
   * Attach a single kprobe.
   * @param probe Specifications of the kprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  virtual Status AttachKProbe(const KProbeSpec& probe) = 0;

  /**
   * Attach a single uprobe.
   * @param probe Specifications of the uprobe (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  virtual Status AttachUProbe(const UProbeSpec& probe) = 0;

  /**
   * Attach a single tracepoint
   * @param probe Specifications of the tracepoint (attach point, trace function, etc.).
   * @return Error if probe fails to attach.
   */
  virtual Status AttachTracepoint(const TracepointSpec& probe) = 0;

  /**
   * Attach a single sampling probe.
   * @param probe Specifications of the probe (bpf function and sampling frequency).
   * @return Error if probe fails to attach.
   */
  virtual Status AttachSamplingProbe(const SamplingProbeSpec& probe) = 0;

  /**
   * Open a perf buffer for reading events.
   * @param perf_buff Specifications of the perf buffer (name, callback function, etc.).
   * @return Error if perf buffer cannot be opened (e.g. perf buffer does not exist).
   */
  virtual Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer) = 0;

  /**
   * Attach a perf event, which runs a probe every time a perf counter reaches a threshold
   * condition.
   * @param perf_event Specification of the perf event and its sampling frequency.
   * @return Error if the perf event could not be attached.
   */
  virtual Status AttachPerfEvent(const PerfEventSpec& perf_event) = 0;

  /**
   * Convenience function that attaches multiple kprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  virtual Status AttachKProbes(const ArrayView<KProbeSpec>& probes) = 0;

  /**
   * Convenience function that attaches multiple tracepoints.
   * @param probes Vector of TracepointSpec.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  virtual Status AttachTracepoints(const ArrayView<TracepointSpec>& probes) = 0;

  /**
   * Convenience function that attaches multiple uprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  virtual Status AttachUProbes(const ArrayView<UProbeSpec>& uprobes) = 0;

  /**
   * Convenience function that attaches multiple uprobes.
   * @param probes Vector of probes.
   * @return Error of first probe to fail to attach (remaining probe attachments are not attempted).
   */
  virtual Status AttachSamplingProbes(const ArrayView<SamplingProbeSpec>& probes) = 0;

  /**
   * Convenience function that attaches a XDP program.
   */
  virtual Status AttachXDP(const std::string& dev_name, const std::string& fn_name) = 0;

  /**
   * Convenience function that opens multiple perf buffers.
   * @param probes Vector of perf buffer descriptors.
   * @return Error of first failure (remaining perf buffer opens are not attempted).
   */
  virtual Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers) = 0;

  /**
   * Convenience function that opens multiple perf events.
   * @param probes Vector of perf event descriptors.
   * @return Error of first failure (remaining perf event attaches are not attempted).
   */
  virtual Status AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events) = 0;

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
  virtual Status PopulateBPFPerfArray(const std::string& table_name, const uint32_t type,
                                      const uint64_t config) = 0;

  /**
   * Drains a specific perf buffer.
   *
   * @param name The name of the perf buffer to drain.
   * @param timeout_ms If there's no event in the perf buffer, then timeout_ms specifies the
   *                   amount of time to wait for an event to arrive before returning.
   *                   Default is 0, because if nothing is ready, then we want to go back to sleep
   *                   and catch new events in the next iteration.
   * @return Error status. This is useful for callers directly using this method, but not for
   *                       PollPerfBuffers() which directly uses the list of open perf buffers.
   */
  virtual Status PollPerfBuffer(const std::string& name, const int timeout_ms = 0) = 0;

  /**
   * Drains all of the opened perf buffers, calling the handle function that was
   * specified in the PerfBufferSpec when OpenPerfBuffer was called.
   *
   * @param timeout_ms Pass through to PollPerfBuffer()
   */
  virtual void PollPerfBuffers(const int timeout_ms = 0) = 0;

  /**
   * Detaches all probes, and closes all perf buffers that are open.
   */
  virtual void Close() = 0;

  // These are static counters of attached/open probes across all instances.
  // It is meant for verification that we have cleaned-up all resources in tests.
  static size_t num_attached_probes() { return num_attached_kprobes_ + num_attached_uprobes_; }
  static size_t num_open_perf_buffers() { return num_open_perf_buffers_; }
  static size_t num_attached_perf_events() { return num_attached_perf_events_; }

  virtual Status ClosePerfBuffer(const PerfBufferSpec& perf_buffer) = 0;

 protected:
  FRIEND_TEST(BCCWrapperTest, DetachUProbe);

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

class BCCWrapperImpl : public BCCWrapper {
 public:
  int CommonPerfBufferSetup(const PerfBufferSpec& perf_buffer_spec);

  virtual ~BCCWrapperImpl() {
    // Not really required, because BPF destructor handles these.
    // But we do it anyways out of paranoia.
    Close();
  }

  StatusOr<ebpf::BPF*> BPF() override { return &bpf_; }
  StatusOr<BPFRecorder*> GetBPFRecorder() const override { return error::Internal("Wrong impl."); }
  StatusOr<BPFReplayer*> GetBPFReplayer() const override { return error::Internal("Wrong impl."); }
  bool IsRecording() const override { return false; }
  bool IsReplaying() const override { return false; }

  Status InitBPFProgram(std::string_view bpf_program, std::vector<std::string> cflags = {},
                        bool requires_linux_headers = true,
                        bool always_infer_task_struct_offsets = false) override;
  Status AttachKProbe(const KProbeSpec& probe) override;
  Status AttachUProbe(const UProbeSpec& probe) override;
  Status AttachTracepoint(const TracepointSpec& probe) override;
  Status AttachSamplingProbe(const SamplingProbeSpec& probe) override;
  Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer) override;
  Status AttachPerfEvent(const PerfEventSpec& perf_event) override;
  Status AttachKProbes(const ArrayView<KProbeSpec>& probes) override;
  Status AttachTracepoints(const ArrayView<TracepointSpec>& probes) override;
  Status AttachUProbes(const ArrayView<UProbeSpec>& uprobes) override;
  Status AttachSamplingProbes(const ArrayView<SamplingProbeSpec>& probes) override;
  Status AttachXDP(const std::string& dev_name, const std::string& fn_name) override;
  Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffers) override;
  Status AttachPerfEvents(const ArrayView<PerfEventSpec>& perf_events) override;
  Status PopulateBPFPerfArray(const std::string& table_name, const uint32_t type,
                              const uint64_t config) override {
    PX_RETURN_IF_ERROR(bpf_.open_perf_event(table_name, type, config));
    return Status::OK();
  }
  void PollPerfBuffers(const int timeout_ms = 0) override;
  Status PollPerfBuffer(const std::string& name, const int timeout_ms = 0) override;
  void Close() override;

  Status ClosePerfBuffer(const PerfBufferSpec& perf_buffer) override;

 private:
  FRIEND_TEST(BCCWrapperTest, DetachUProbe);

  Status DetachKProbe(const KProbeSpec& probe);
  Status DetachUProbe(const UProbeSpec& probe);
  Status DetachTracepoint(const TracepointSpec& probe);
  Status DetachPerfEvent(const PerfEventSpec& perf_event);

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
  std::vector<PerfEventSpec> perf_events_;

 protected:
  std::vector<PerfBufferSpec> perf_buffer_specs_;

 private:
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
 protected:
  ebpf::BPF bpf_;
};

class RecordingBCCWrapperImpl : public BCCWrapperImpl {
 public:
  bool IsRecording() const override { return true; }
  bool IsReplaying() const override { return false; }
  StatusOr<BPFRecorder*> GetBPFRecorder() const override { return recorder_.get(); }
  StatusOr<BPFReplayer*> GetBPFReplayer() const override { return error::Internal("Wrong impl."); }

  Status OpenPerfBuffer(const PerfBufferSpec& perf_buffer) override;

  RecordingBCCWrapperImpl() { recorder_ = std::make_unique<BPFRecorder>(); }

  void WriteProto(const std::string& pb_file_path) { recorder_->WriteProto(pb_file_path); }

 private:
  std::unique_ptr<BPFRecorder> recorder_;
};

class ReplayingBCCWrapperImpl : public BCCWrapper {
 public:
  bool IsRecording() const override { return false; }
  bool IsReplaying() const override { return true; }
  StatusOr<BPFRecorder*> GetBPFRecorder() const override { return error::Internal("Wrong impl."); }
  StatusOr<BPFReplayer*> GetBPFReplayer() const override { return replayer_.get(); }

  virtual ~ReplayingBCCWrapperImpl() {}

  ReplayingBCCWrapperImpl() { replayer_ = std::make_unique<BPFReplayer>(); }

  StatusOr<ebpf::BPF*> BPF() override { return error::Internal("Wrong impl."); }

  Status InitBPFProgram(std::string_view, std::vector<std::string> cflags = {},
                        bool requires_linux_headers = true,
                        bool always_infer_task_struct_offsets = false) override {
    PX_UNUSED(cflags);
    PX_UNUSED(requires_linux_headers);
    PX_UNUSED(always_infer_task_struct_offsets);
    return Status::OK();
  }
  Status AttachKProbe(const KProbeSpec&) override { return Status::OK(); }
  Status AttachUProbe(const UProbeSpec&) override { return Status::OK(); }
  Status AttachTracepoint(const TracepointSpec&) override { return Status::OK(); }
  Status AttachSamplingProbe(const SamplingProbeSpec&) override { return Status::OK(); }
  Status AttachPerfEvent(const PerfEventSpec&) override { return Status::OK(); }
  Status AttachKProbes(const ArrayView<KProbeSpec>&) override { return Status::OK(); }
  Status AttachTracepoints(const ArrayView<TracepointSpec>&) override { return Status::OK(); }
  Status AttachUProbes(const ArrayView<UProbeSpec>&) override { return Status::OK(); }
  Status AttachSamplingProbes(const ArrayView<SamplingProbeSpec>&) override { return Status::OK(); }
  Status AttachXDP(const std::string&, const std::string&) override { return Status::OK(); }
  Status AttachPerfEvents(const ArrayView<PerfEventSpec>&) override { return Status::OK(); }
  Status PopulateBPFPerfArray(const std::string&, const uint32_t, const uint64_t) override {
    return Status::OK();
  }

  Status OpenPerfBuffer(const PerfBufferSpec& pbs) override {
    perf_buffer_specs_.push_back(std::make_unique<PerfBufferSpec>(pbs));
    return Status::OK();
  }

  Status OpenPerfBuffers(const ArrayView<PerfBufferSpec>& perf_buffer_specs) override {
    for (const auto& pbs : perf_buffer_specs) {
      PX_RETURN_IF_ERROR(OpenPerfBuffer(pbs));
    }
    return Status::OK();
  }

  Status PollPerfBuffer(const std::string& name, const int timeout_ms = 0) override {
    PX_UNUSED(timeout_ms);
    for (const auto& pbs : perf_buffer_specs_) {
      if (pbs->name == name) {
        replayer_->ReplayPerfBufferEvents(*pbs);
        return Status::OK();
      }
    }
    return error::NotFound(absl::Substitute("Perf buffer \"$0\" not found.", name));
  }

  void PollPerfBuffers(const int timeout_ms = 0) override {
    PX_UNUSED(timeout_ms);
    for (const auto& pbs : perf_buffer_specs_) {
      replayer_->ReplayPerfBufferEvents(*pbs);
    }
  };

  void Close() override{};

  Status ClosePerfBuffer(const PerfBufferSpec&) override { return Status::OK(); }

  Status OpenReplayProtobuf(const std::string& file_path) {
    return replayer_->OpenReplayProtobuf(file_path);
  }

 private:
  std::unique_ptr<BPFReplayer> replayer_;
  std::vector<std::unique_ptr<PerfBufferSpec>> perf_buffer_specs_;
};

std::unique_ptr<BCCWrapper> CreateBCC();

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Array Table.
template <typename T>
class WrappedBCCArrayTable {
 public:
  virtual ~WrappedBCCArrayTable() {}
  static std::unique_ptr<WrappedBCCArrayTable> Create(BCCWrapper* bcc, const std::string& name);

  virtual StatusOr<T> GetValue(const uint32_t idx) = 0;
  virtual Status SetValue(const uint32_t idx, const T& value) = 0;
};

template <typename T>
class WrappedBCCArrayTableImpl : public WrappedBCCArrayTable<T> {
 public:
  using U = ebpf::BPFArrayTable<T>;

  StatusOr<T> GetValue(const uint32_t idx) override {
    T value;
    ebpf::StatusTuple s = underlying_->get_value(idx, value);
    if (!s.ok()) {
      return error::Internal(absl::Substitute(err_msg_, "get", name_, idx, s.msg()));
    }
    return value;
  }

  Status SetValue(const uint32_t idx, const T& value) override {
    ebpf::StatusTuple s = underlying_->update_value(idx, value);
    if (!s.ok()) {
      return error::Internal(absl::Substitute(err_msg_, "set", name_, idx, s.msg()));
    }
    return Status::OK();
  }

  WrappedBCCArrayTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name) : name_(name) {
    ebpf::BPF* bpf = bcc->BPF().ConsumeValueOrDie();
    underlying_ = std::make_unique<U>(bpf->get_array_table<T>(name_));
  }

 protected:
  const std::string name_;

 private:
  char const* const err_msg_ = "BPF failed to $0 value for array table: $1, index: $2. $3.";
  std::unique_ptr<U> underlying_;
};

template <typename T>
class RecordingWrappedBCCArrayTableImpl : public WrappedBCCArrayTableImpl<T> {
 public:
  using Super = WrappedBCCArrayTableImpl<T>;

  StatusOr<T> GetValue(const uint32_t idx) override {
    PX_ASSIGN_OR_RETURN(const T value, Super::GetValue(idx));
    recorder_.RecordBPFArrayTableGetValueEvent(this->name_, idx, sizeof(value), &value);
    return value;
  }

  RecordingWrappedBCCArrayTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : WrappedBCCArrayTableImpl<T>(bcc, name),
        recorder_(*bcc->GetBPFRecorder().ConsumeValueOrDie()) {}

 private:
  BPFRecorder& recorder_;
};

template <typename T>
class ReplayingWrappedBCCArrayTableImpl : public WrappedBCCArrayTable<T> {
 public:
  StatusOr<T> GetValue(const uint32_t idx) override {
    T value;
    PX_RETURN_IF_ERROR(replayer_.ReplayArrayGetValue(this->name_, idx, sizeof(T), &value));
    return value;
  }

  Status SetValue(const uint32_t, const T&) override { return Status::OK(); }

  ReplayingWrappedBCCArrayTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : replayer_(*bcc->GetBPFReplayer().ConsumeValueOrDie()), name_(name) {}

 private:
  BPFReplayer& replayer_;
  const std::string name_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Map / BPF Hash Table
template <typename K, typename V, bool kUserSpaceManaged = false>
class WrappedBCCMap {
 public:
  static std::unique_ptr<WrappedBCCMap> Create(bpf_tools::BCCWrapper* bcc, const std::string& name);
  virtual ~WrappedBCCMap() {}

  virtual size_t capacity() const = 0;
  virtual StatusOr<V> GetValue(const K& key) const = 0;
  virtual Status SetValue(const K& key, const V& value) = 0;
  virtual Status RemoveValue(const K& key) = 0;
  virtual std::vector<std::pair<K, V>> GetTableOffline(const bool clear_table = false) = 0;
};

// Template parameter kUserSpaceManaged enables the "shadow keys" optimization.
// Set to true iff the map is modified/updated from user space only.
template <typename K, typename V, bool kUserSpaceManaged = false>
class WrappedBCCMapImpl : public WrappedBCCMap<K, V, kUserSpaceManaged> {
 public:
  using U = ebpf::BPFHashTable<K, V>;

  size_t capacity() const override { return underlying_->capacity(); }

  StatusOr<V> GetValue(const K& key) const override {
    V value;
    ebpf::StatusTuple s = underlying_->get_value(key, value);
    if (!s.ok()) {
      return error::Internal(absl::Substitute(err_msg_, "get", name_, s.msg()));
    }
    return value;
  }

  Status SetValue(const K& key, const V& value) override {
    ebpf::StatusTuple s = underlying_->update_value(key, value);
    if (!s.ok()) {
      return error::Internal(absl::Substitute(err_msg_, "set", name_, s.msg()));
    }
    if constexpr (kUserSpaceManaged) {
      shadow_keys_.insert(key);
    }
    return Status::OK();
  }

  Status RemoveValue(const K& key) override {
    if constexpr (kUserSpaceManaged) {
      if (!shadow_keys_.contains(key)) {
        return Status::OK();
      }
    }

    const auto s = underlying_->remove_value(key);
    if (!s.ok()) {
      return error::Internal(absl::Substitute(err_msg_, "remove", name_, s.msg()));
    }
    if constexpr (kUserSpaceManaged) {
      shadow_keys_.erase(key);
    }
    return Status::OK();
  }

  std::vector<std::pair<K, V>> GetTableOffline(const bool clear_table = false) override {
    if constexpr (!kUserSpaceManaged) {
      return underlying_->get_table_offline(clear_table);
    }

    // "r" our result.
    std::vector<std::pair<K, V>> r;

    // This is a user space managed map: we can iterate over the shadow keys.
    for (const auto& k : shadow_keys_) {
      auto s = GetValue(k);
      const auto v = s.ConsumeValueOrDie();
      r.push_back({k, v});
      if (clear_table) {
        PX_UNUSED(underlying_->remove_value(k));
      }
    }
    if (clear_table) {
      shadow_keys_.clear();
    }
    return r;
  }

  WrappedBCCMapImpl(bpf_tools::BCCWrapper* bcc, const std::string& name) : name_(name) {
    ebpf::BPF* bpf = bcc->BPF().ConsumeValueOrDie();
    underlying_ = std::make_unique<U>(bpf->get_hash_table<K, V>(name_));
  }

 protected:
  const std::string name_;

 private:
  char const* const err_msg_ = "BPF failed to $0 value for map: $1. $2.";
  std::unique_ptr<U> underlying_;
  absl::flat_hash_set<K> shadow_keys_;
};

template <typename K, typename V, bool kUserSpaceManaged = false>
class RecordingWrappedBCCMapImpl : public WrappedBCCMapImpl<K, V, kUserSpaceManaged> {
 public:
  using Super = WrappedBCCMapImpl<K, V, kUserSpaceManaged>;

  StatusOr<V> GetValue(const K& key) const override {
    PX_ASSIGN_OR_RETURN(const V value, Super::GetValue(key));
    recorder_.RecordBPFMapGetValueEvent(this->name_, sizeof(key), &key, sizeof(value), &value);
    return value;
  }

  size_t capacity() const override {
    const size_t n = Super::capacity();
    recorder_.RecordBPFMapCapacityEvent(this->name_, n);
    return n;
  }

  std::vector<std::pair<K, V>> GetTableOffline(const bool clear_table = false) override {
    const auto r = Super::GetTableOffline(clear_table);

    // Synthesize a "get table offline" recording by:
    // 1. Recording how many key/value pairs were returned.
    // 2. Recording each key/value pair individually.
    recorder_.RecordBPFMapGetTableOfflineEvent(this->name_, r.size());
    for (const auto& [key, value] : r) {
      recorder_.RecordBPFMapGetValueEvent(this->name_, sizeof(key), &key, sizeof(value), &value);
    }
    return r;
  }

  RecordingWrappedBCCMapImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : WrappedBCCMapImpl<K, V, kUserSpaceManaged>(bcc, name),
        recorder_(*bcc->GetBPFRecorder().ConsumeValueOrDie()) {}

 private:
  BPFRecorder& recorder_;
};

template <typename K, typename V, bool kUserSpaceManaged = false>
class ReplayingWrappedBCCMapImpl : public WrappedBCCMap<K, V, kUserSpaceManaged> {
 public:
  StatusOr<V> GetValue(const K& k) const override {
    V v;
    PX_RETURN_IF_ERROR(replayer_.ReplayMapGetValue(this->name_, sizeof(K), &k, sizeof(V), &v));
    return v;
  }

  Status SetValue(const K&, const V&) override { return Status::OK(); }
  Status RemoveValue(const K&) override { return Status::OK(); }

  std::vector<std::pair<K, V>> GetTableOffline(const bool) override {
    std::vector<std::pair<K, V>> r;
    auto status_or_size = replayer_.ReplayBPFMapGetTableOfflineEvent(name_);
    if (!status_or_size.ok()) {
      return r;
    }
    const int n = status_or_size.ConsumeValueOrDie();
    PX_UNUSED(n);
    for (int i = 0; i < n; ++i) {
      K k;
      V v;
      auto s = replayer_.ReplayMapGetKeyAndValue(this->name_, sizeof(K), &k, sizeof(V), &v);
      if (!s.ok()) {
        return r;
      }
      r.push_back({k, v});
    }
    return r;
  }

  size_t capacity() const override {
    return replayer_.ReplayBPFMapCapacityEvent(name_).ConsumeValueOr(0);
  }

  ReplayingWrappedBCCMapImpl(BCCWrapper* bcc, const std::string& name)
      : name_(name), replayer_(*bcc->GetBPFReplayer().ConsumeValueOrDie()) {}

 private:
  const std::string name_;
  BPFReplayer& replayer_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Per CPU Array Table
template <typename T>
class WrappedBCCPerCPUArrayTable {
 public:
  static std::unique_ptr<WrappedBCCPerCPUArrayTable> Create(bpf_tools::BCCWrapper* bcc,
                                                            const std::string& name);
  virtual ~WrappedBCCPerCPUArrayTable() {}

  virtual Status SetValues(const int idx, const T& value) = 0;
};

template <typename T>
class WrappedBCCPerCPUArrayTableImpl : public WrappedBCCPerCPUArrayTable<T> {
 public:
  using U = ebpf::BPFPercpuArrayTable<T>;

  Status SetValues(const int idx, const T& value) override {
    std::vector<T> values(bpf_tools::BCCWrapper::kCPUCount, value);
    ebpf::StatusTuple s = underlying_->update_value(idx, values);
    if (!s.ok()) {
      char const* const err_msg_ = "BPF failed to $0 value for per cpu array: $1, index: $2. $3.";
      return error::Internal(absl::Substitute(err_msg_, "set", name_, idx, s.msg()));
    }
    return Status::OK();
  }

  WrappedBCCPerCPUArrayTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : name_(name) {
    ebpf::BPF* bpf = bcc->BPF().ConsumeValueOrDie();
    underlying_ = std::make_unique<U>(bpf->get_percpu_array_table<T>(name_));
  }

 private:
  const std::string name_;
  std::unique_ptr<U> underlying_;
};

template <typename T>
class RecordingWrappedBCCPerCPUArrayTableImpl : public WrappedBCCPerCPUArrayTableImpl<T> {
 public:
  using Super = WrappedBCCPerCPUArrayTableImpl<T>;
  RecordingWrappedBCCPerCPUArrayTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : Super(bcc, name) {}
};

template <typename T>
class ReplayingWrappedBCCPerCPUArrayTableImpl : public WrappedBCCPerCPUArrayTable<T> {
 public:
  Status SetValues(const int, const T&) override { return Status::OK(); }

  ReplayingWrappedBCCPerCPUArrayTableImpl(bpf_tools::BCCWrapper*, const std::string&) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Stack Table
class WrappedBCCStackTable {
 public:
  static std::unique_ptr<WrappedBCCStackTable> Create(bpf_tools::BCCWrapper* bcc,
                                                      const std::string& name);
  virtual ~WrappedBCCStackTable() {}

  virtual std::vector<uintptr_t> GetStackAddr(const int stack_id, const bool clear_stack_id) = 0;
  virtual std::string GetAddrSymbol(const uintptr_t addr, const int pid) = 0;
  virtual void ClearStackID(const int stack_id) = 0;
};

class WrappedBCCStackTableImpl : public WrappedBCCStackTable {
 public:
  using U = ebpf::BPFStackTable;

  std::vector<uintptr_t> GetStackAddr(const int stack_id, const bool clear_stack_id) override {
    return underlying_->get_stack_addr(stack_id, clear_stack_id);
  }

  std::string GetAddrSymbol(const uintptr_t addr, const int pid) override {
    return underlying_->get_addr_symbol(addr, pid);
  }

  void ClearStackID(const int stack_id) override { underlying_->clear_stack_id(stack_id); }

  WrappedBCCStackTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name) : name_(name) {
    ebpf::BPF* bpf = bcc->BPF().ConsumeValueOrDie();
    underlying_ = std::make_unique<U>(bpf->get_stack_table(name_));
  }

 protected:
  const std::string name_;

 private:
  std::unique_ptr<U> underlying_;
};

class RecordingWrappedBCCStackTableImpl : public WrappedBCCStackTableImpl {
 public:
  using Super = WrappedBCCStackTableImpl;

  std::vector<uintptr_t> GetStackAddr(const int stack_id, const bool clear_stack_id) override {
    const auto stack_addrs = Super::GetStackAddr(stack_id, clear_stack_id);
    recorder_.RecordBPFStackTableGetStackAddrEvent(this->name_, stack_id, stack_addrs);
    return stack_addrs;
  }

  std::string GetAddrSymbol(const uintptr_t addr, const int pid) override {
    const auto symbol = Super::GetAddrSymbol(addr, pid);
    recorder_.RecordBPFStackTableGetAddrSymbolEvent(this->name_, addr, pid, symbol);
    return symbol;
  }

  RecordingWrappedBCCStackTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : Super(bcc, name), recorder_(*bcc->GetBPFRecorder().ConsumeValueOrDie()) {}

 private:
  BPFRecorder& recorder_;
};

class ReplayingWrappedBCCStackTableImpl : public WrappedBCCStackTable {
 public:
  std::vector<uintptr_t> GetStackAddr(const int stack_id, const bool) override {
    return replayer_.ReplayBPFStackTableGetStackAddrEvent(name_, stack_id).ConsumeValueOr({0});
  }

  std::string GetAddrSymbol(const uintptr_t addr, const int pid) override {
    return replayer_.ReplayBPFStackTableGetAddrSymbolEvent(name_, addr, pid).ConsumeValueOr("");
  }

  void ClearStackID(const int) override {}

  ReplayingWrappedBCCStackTableImpl(bpf_tools::BCCWrapper* bcc, const std::string& name)
      : name_(name), replayer_(*bcc->GetBPFReplayer().ConsumeValueOrDie()) {}

 private:
  const std::string name_;
  BPFReplayer& replayer_;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Creators fns for wrapped maps & arrays:
// template <typename BaseT, typename ImplT>
// std::unique_ptr<BaseT> CreateBCCWrappedMapOrArray(BCCWrapper* bcc, const std::string& name) {
//   // The decision logic for "normal" vs. "recording" vs. "replaying" impl. will be inserted
//   // here in a future PR.
//   return std::make_unique<ImplT>(bcc, name);
// }

template <typename BaseT, typename ImplT, typename RecordingT, typename ReplayingT>
std::unique_ptr<BaseT> CreateBCCWrappedMapOrArray(BCCWrapper* bcc, const std::string& name) {
  if (bcc->IsRecording()) {
    return std::make_unique<RecordingT>(bcc, name);
  }
  if (bcc->IsReplaying()) {
    return std::make_unique<ReplayingT>(bcc, name);
  }
  return std::make_unique<ImplT>(bcc, name);
}

template <typename T>
std::unique_ptr<WrappedBCCArrayTable<T>> WrappedBCCArrayTable<T>::Create(BCCWrapper* bcc,
                                                                         const std::string& name) {
  using BaseT = WrappedBCCArrayTable<T>;
  using ImplT = WrappedBCCArrayTableImpl<T>;
  using RecordingT = RecordingWrappedBCCArrayTableImpl<T>;
  using ReplayingT = ReplayingWrappedBCCArrayTableImpl<T>;
  return CreateBCCWrappedMapOrArray<BaseT, ImplT, RecordingT, ReplayingT>(bcc, name);
}

template <typename K, typename V, bool U>
std::unique_ptr<WrappedBCCMap<K, V, U>> WrappedBCCMap<K, V, U>::Create(BCCWrapper* bcc,
                                                                       const std::string& name) {
  using BaseT = WrappedBCCMap<K, V, U>;
  using ImplT = WrappedBCCMapImpl<K, V, U>;
  using RecordingT = RecordingWrappedBCCMapImpl<K, V, U>;
  using ReplayingT = ReplayingWrappedBCCMapImpl<K, V, U>;
  return CreateBCCWrappedMapOrArray<BaseT, ImplT, RecordingT, ReplayingT>(bcc, name);
}

template <typename T>
std::unique_ptr<WrappedBCCPerCPUArrayTable<T>> WrappedBCCPerCPUArrayTable<T>::Create(
    BCCWrapper* bcc, const std::string& name) {
  using BaseT = WrappedBCCPerCPUArrayTable<T>;
  using ImplT = WrappedBCCPerCPUArrayTableImpl<T>;
  using RecordingT = RecordingWrappedBCCPerCPUArrayTableImpl<T>;
  using ReplayingT = ReplayingWrappedBCCPerCPUArrayTableImpl<T>;
  return CreateBCCWrappedMapOrArray<BaseT, ImplT, RecordingT, ReplayingT>(bcc, name);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
