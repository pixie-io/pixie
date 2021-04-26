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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/types.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/perf_profiler/bcc_bpf_intf/stack_event.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

namespace px {
namespace stirling {

class PerfProfileConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  static constexpr auto kTables = MakeArray(kStackTraceTable);
  static constexpr uint32_t kPerfProfileTableNum = TableNum(kTables, kStackTraceTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PerfProfileConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table) override;
  static constexpr uint64_t BPFSamplingPeriodMillis() { return kSamplingPeriodMillis; }

 private:
  // SymbolicStackTrace identifies a particular stack trace by:
  // * upid
  // * "folded" stack trace string
  // The stack traces (in kernel & in BPF) are ordered lists of instruction pointers (addresses).
  // Stirling uses BPF to recover the symbols associated with each address, and then
  // uses the "symbolic stack trace" as the histogram key. Some of the stack traces that are
  // distinct in the kernel and in BPF will collapse into the same symoblic stack trace in Stirling.
  // For example, consider the following two stack traces from BPF:
  // p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
  // p0, p1, p3 => main;qux;baz
  //
  // SymbolicStackTrace will serve as a key to the unique stack-trace-id (an integer) in Stirling.
  struct SymbolicStackTrace {
    const md::UPID upid;
    const std::string stack_trace_str;

    template <typename H>
    friend H AbslHashValue(H h, const SymbolicStackTrace& s) {
      return H::combine(std::move(h), s.upid, s.stack_trace_str);
    }

    friend bool operator==(const SymbolicStackTrace& lhs, const SymbolicStackTrace& rhs) {
      if (lhs.upid != rhs.upid) {
        return false;
      }
      return lhs.stack_trace_str == rhs.stack_trace_str;
    }
  };

  // StackTraceHisto: SymbolicStackTrace => observation-count
  // StackTraceIDMap: SymbolicStackTrace => stack-trace-id
  using StackTraceHisto = absl::flat_hash_map<SymbolicStackTrace, uint64_t>;
  using StackTraceIDMap = absl::flat_hash_map<SymbolicStackTrace, uint64_t>;

  explicit PerfProfileConnector(std::string_view source_name);

  Status ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table);

  // Read BPF data structures, build & incorporate records to the table.
  void CreateRecords(const uint64_t timestamp_ns, ebpf::BPFStackTable* stack_traces,
                     ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo, ConnectorContext* ctx,
                     DataTable* data_table);

  uint64_t SymbolicStackTraceID(const SymbolicStackTrace& symbolic_stack_trace);

  StackTraceHisto AggregateStackTraces(ConnectorContext* ctx, ebpf::BPFStackTable* stack_traces,
                                       ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo);

  std::string FoldedStackTraceString(const bool symbolize, ebpf::BPFStackTable* stack_traces,
                                     const stack_trace_key_t& key);

  void CleanupSymbolizers(const absl::flat_hash_set<md::UPID>& deleted_upids);

  // data structures shared with BPF:
  std::unique_ptr<ebpf::BPFStackTable> stack_traces_a_;
  std::unique_ptr<ebpf::BPFStackTable> stack_traces_b_;
  std::unique_ptr<ebpf::BPFHashTable<stack_trace_key_t, uint64_t> > histogram_a_;
  std::unique_ptr<ebpf::BPFHashTable<stack_trace_key_t, uint64_t> > histogram_b_;
  std::unique_ptr<ebpf::BPFArrayTable<uint64_t> > profiler_state_;

  // Number of read & clear ops completed:
  uint64_t read_and_clear_count_ = 0;

  // Tracks the next stack-trace-id to be assigned;
  // incremented by 1 for each such assignment.
  uint64_t next_stack_trace_id_ = 0;

  // Tracks unique stack trace ids, for the lifetime of Stirling:
  StackTraceIDMap stack_trace_ids_;

  // At the discretion of its policy, a symbolizer will either:
  // ... return the symbol given an address,
  // ... or just stringify the address (i.e. to avoid the cost of symbol lookup).
  // The symbolizer may maintain a set of symbol cache to make a best effort at
  // reducing the cost of symbol lookup.
  // Symbolizers are created with a specific policy on a per upid basis.
  absl::flat_hash_map<struct upid_t, Symbolizer> symbolizers_;

  // Some special case symbolizers:
  // ... 1. A symbolizer that has the policy of "do not symbolize," and
  // ... 2. a dedicated symbolizer for kernel syms.
  Symbolizer dummy_symbolizer_;
  Symbolizer kernel_symbolizer_;

  // Keeps track of processes. Used to find destroyed processes on which to perform clean-up.
  // TODO(oazizi): Investigate ways of sharing across source_connectors.
  ProcTracker proc_tracker_;

  // kSamplingPeriodMillis: the time interval in between stack trace samples.
  // kTargetPushPeriodMillis: time interval between "push events".
  // ... a push event is when the BPF perf-profiler probe notifies stirling (user space)
  // that the shared maps are full and ready for consumption. After each push,
  // the BPF side switches over to the other map set.
  static constexpr uint64_t kSamplingPeriodMillis = 11;
  static constexpr uint64_t kTargetPushPeriodMillis = 10 * 1000;
  static constexpr auto kProbeSpecs =
      MakeArray<bpf_tools::SamplingProbeSpec>({"sample_call_stack", kSamplingPeriodMillis});
};

}  // namespace stirling
}  // namespace px
