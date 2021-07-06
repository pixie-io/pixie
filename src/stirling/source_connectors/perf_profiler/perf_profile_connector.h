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
#include "src/stirling/source_connectors/perf_profiler/stringifier.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

namespace px {
namespace stirling {

namespace profiler {
static constexpr std::string_view kNotSymbolizedMessage = "<not symbolized>";
}  // namespace profiler

class PerfProfileConnector : public SourceConnector, public bpf_tools::BCCWrapper {
 public:
  static constexpr std::string_view kName = "perf_profiler";
  static constexpr auto kTables = MakeArray(kStackTraceTable);
  static constexpr uint32_t kPerfProfileTableNum = TableNum(kTables, kStackTraceTable);

  // kBPFSamplingPeriod: the time interval in between stack trace samples.
  static constexpr auto kBPFSamplingPeriod = std::chrono::milliseconds{11};

  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{30000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{15000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new PerfProfileConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, const std::vector<DataTable*>& data_tables) override;

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
  using StackTraceHisto = absl::flat_hash_map<SymbolicStackTrace, uint64_t>;

  explicit PerfProfileConnector(std::string_view source_name);

  void ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table);

  // Read BPF data structures, build & incorporate records to the table.
  void CreateRecords(ebpf::BPFStackTable* stack_traces,
                     ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo, ConnectorContext* ctx,
                     DataTable* data_table);

  uint64_t StackTraceID(const SymbolicStackTrace& stack_trace);

  StackTraceHisto AggregateStackTraces(ConnectorContext* ctx, ebpf::BPFStackTable* stack_traces,
                                       ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo);

  void CleanupSymbolizers(const absl::flat_hash_set<md::UPID>& deleted_upids);

  // data structures shared with BPF:
  std::unique_ptr<ebpf::BPFStackTable> stack_traces_a_;
  std::unique_ptr<ebpf::BPFStackTable> stack_traces_b_;
  std::unique_ptr<ebpf::BPFHashTable<stack_trace_key_t, uint64_t>> histogram_a_;
  std::unique_ptr<ebpf::BPFHashTable<stack_trace_key_t, uint64_t>> histogram_b_;
  std::unique_ptr<ebpf::BPFArrayTable<uint64_t>> profiler_state_;

  // Number of iterations, where each iteration is drains the information collectid in BPF.
  uint64_t transfer_count_ = 0;

  // Tracks the next stack-trace-id to be assigned;
  // incremented by 1 for each such assignment.
  uint64_t next_stack_trace_id_ = 0;

  // The symbolizer has an instance of a BPF stack table (internally),
  // solely to gain access to the BCC symbolization API. Depending on the
  // value of FLAGS_stirling_profiler_symcache, symbolizer will attempt (or not)
  // to find the symbol in its internally managed symbol cache.
  Symbolizer symbolizer_;

  // Keeps track of processes. Used to find destroyed processes on which to perform clean-up.
  // TODO(oazizi): Investigate ways of sharing across source_connectors.
  ProcTracker proc_tracker_;

  static constexpr auto kProbeSpecs =
      MakeArray<bpf_tools::SamplingProbeSpec>({"sample_call_stack", kBPFSamplingPeriod.count()});
};

}  // namespace stirling
}  // namespace px
