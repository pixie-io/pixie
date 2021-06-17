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

#include <sys/sysinfo.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"

BPF_SRC_STRVIEW(profiler_bcc_script, profiler);

namespace px {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables) {}

Status PerfProfileConnector::InitImpl() {
  sample_push_freq_mgr_.set_sampling_period(kSamplingPeriod);
  // TODO(yzhao): This is not used right now. Eventually use this to control data push frequency.
  sample_push_freq_mgr_.set_push_period(kPushPeriod);

  const size_t ncpus = get_nprocs_conf();
  VLOG(1) << "PerfProfiler: get_nprocs_conf(): " << ncpus;

  const std::vector<std::string> defines = {
      absl::Substitute("-DNCPUS=$0", ncpus),
      absl::Substitute("-DTRANSFER_PERIOD=$0", kSamplingPeriod.count()),
      absl::Substitute("-DSAMPLE_PERIOD=$0", kBPFSamplingPeriod.count())};

  PL_RETURN_IF_ERROR(InitBPFProgram(profiler_bcc_script, defines));
  PL_RETURN_IF_ERROR(AttachSamplingProbes(kProbeSpecs));

  stack_traces_a_ = std::make_unique<ebpf::BPFStackTable>(GetStackTable("stack_traces_a"));
  stack_traces_b_ = std::make_unique<ebpf::BPFStackTable>(GetStackTable("stack_traces_b"));

  histogram_a_ = std::make_unique<ebpf::BPFHashTable<stack_trace_key_t, uint64_t>>(
      GetHashTable<stack_trace_key_t, uint64_t>("histogram_a"));
  histogram_b_ = std::make_unique<ebpf::BPFHashTable<stack_trace_key_t, uint64_t>>(
      GetHashTable<stack_trace_key_t, uint64_t>("histogram_b"));
  profiler_state_ =
      std::make_unique<ebpf::BPFArrayTable<uint64_t>>(GetArrayTable<uint64_t>("profiler_state"));

  LOG(INFO) << "PerfProfiler: Stack trace profiling sampling probe successfully deployed.";

  // Made it here w/ no problems; now init the symbolizer & return its status.
  return symbolizer_.Init();
}

Status PerfProfileConnector::StopImpl() {
  // Must call Close() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  Close();
  return Status::OK();
}

void PerfProfileConnector::CleanupSymbolizers(const absl::flat_hash_set<md::UPID>& deleted_upids) {
  for (const auto& md_upid : deleted_upids) {
    struct upid_t upid;
    upid.pid = md_upid.pid();
    upid.start_time_ticks = md_upid.start_ts();
    symbolizer_.FlushCache(upid);
  }
}

uint64_t PerfProfileConnector::SymbolicStackTraceID(
    const SymbolicStackTrace& symbolic_stack_trace) {
  const auto it = stack_trace_ids_.find(symbolic_stack_trace);

  if (it != stack_trace_ids_.end()) {
    const uint64_t stack_trace_id = it->second;
    return stack_trace_id;
  } else {
    stack_trace_ids_[symbolic_stack_trace] = next_stack_trace_id_;
    return next_stack_trace_id_++;
  }
}

PerfProfileConnector::StackTraceHisto PerfProfileConnector::AggregateStackTraces(
    ConnectorContext* ctx, ebpf::BPFStackTable* stack_traces,
    ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo) {
  // TODO(jps): switch from using get_table_offline() to directly stepping through
  // the histogram data structure. Inline populating our own data structures with this.
  // Avoid an unnecessary copy of the information in local stack_trace_keys_and_counts.
  StackTraceHisto symbolic_histogram;
  uint64_t cum_sum_count = 0;

  const uint32_t asid = ctx->GetASID();
  const absl::flat_hash_set<md::UPID>& upids_for_symbolization = ctx->GetUPIDs();

  // Create a new stringifer for this iteration of the continuous perf profiler.
  Stringifier stringifier(&symbolizer_, stack_traces);

  // Here we "consume" the table (vs. just "reading" it). Passing "clear_table=true"
  // into get_table_offline() clears each entry while walking the table and copying
  // out the data. This shows a significant performance boost vs. using clear_table_non_atomic()
  // after the table has been read.
  constexpr bool kClearTable = true;

  absl::flat_hash_set<int> k_stack_ids_to_remove;

  for (const auto& [stack_trace_key, count] : histo->get_table_offline(kClearTable)) {
    std::string stack_trace_str;

    const md::UPID upid(asid, stack_trace_key.upid.pid, stack_trace_key.upid.start_time_ticks);
    const bool symbolize = upids_for_symbolization.contains(upid);

    if (symbolize) {
      // The stringifier clears stack-ids out of the stack traces table when it
      // first encounters them. If a stack-id is reused by a different stack-trace-key,
      // the stringifier returns its memoized stack trace string. Because the stack-ids
      // are not stable across profiler iterations, we create and destroy a stringifer
      // on each profiler iteration.
      stack_trace_str = stringifier.FoldedStackTraceString(stack_trace_key);
    } else {
      // If we do not stringifiy this stack trace, we still need to clear
      // its entry from the stack traces table. It is safe to do so immediately
      // for the user stack-id, but we need to allow the kernel stack-id to remain
      // in the stack-traces table in case it gets used by a stack trace that we
      // have not yet encountered on this iteration, but will need to symbolize.
      if (stack_trace_key.user_stack_id >= 0) {
        stack_traces->clear_stack_id(stack_trace_key.user_stack_id);
      }
      if (stack_trace_key.kernel_stack_id >= 0) {
        k_stack_ids_to_remove.insert(stack_trace_key.kernel_stack_id);
      }
      stack_trace_str = std::string(profiler::kNotSymbolizedMessage);
    }

    SymbolicStackTrace symbolic_stack_trace = {upid, std::move(stack_trace_str)};

    symbolic_histogram[symbolic_stack_trace] += count;
    cum_sum_count += count;

    // TODO(jps): If we see a perf. issue with having two maps keyed by symbolic-stack-trace,
    // refactor such that creating/finding symoblic-stack-trace-id and count aggregation
    // are both done here, in AggregateStackTraces().
    // One possible implementation:
    // make the histogram a map from "symbolic-track-trace" => "count & stack-trace-id"
    // e.g.:
    // ... auto& count_and_id = symbolic_histogram[symbolic_stack_trace];
    // ... count_and_id.id = id;
    // ... count_and_id.count += count;
    // alternate impl. is a map from "stack-trace-id" => "count & symbolic-stack-trace"
  }

  // Clear any kernel stack-ids, that were potentially not already cleared,
  // out of the stack traces table.
  for (const int k_stack_id : k_stack_ids_to_remove) {
    stack_traces->clear_stack_id(k_stack_id);
  }

  VLOG(1) << "PerfProfileConnector::AggregateStackTraces(): cum_sum_count: " << cum_sum_count;
  return symbolic_histogram;
}

void PerfProfileConnector::CreateRecords(ebpf::BPFStackTable* stack_traces,
                                         ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo,
                                         ConnectorContext* ctx, DataTable* data_table) {
  constexpr size_t kMaxSymbolSize = 512;
  constexpr size_t kMaxStackDepth = 64;
  constexpr size_t kMaxStackTraceSize = kMaxStackDepth * kMaxSymbolSize;

  const uint64_t timestamp_ns = CurrentTimeNS();

  // Stack traces from kernel/BPF are ordered lists of instruction pointers (addresses).
  // AggregateStackTraces() will collapse some of those into identical symbolic stack traces;
  // for example, consider the following two stack traces from BPF:
  // p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
  // p0, p1, p3 => main;qux;baz

  StackTraceHisto stack_trace_histogram = AggregateStackTraces(ctx, stack_traces, histo);

  for (const auto& [key, count] : stack_trace_histogram) {
    DataTable::RecordBuilder<&kStackTraceTable> r(data_table, timestamp_ns);

    const uint64_t stack_trace_id = SymbolicStackTraceID(key);

    r.Append<r.ColIndex("time_")>(timestamp_ns);
    r.Append<r.ColIndex("upid")>(key.upid.value());
    r.Append<r.ColIndex("stack_trace_id")>(stack_trace_id);
    r.Append<r.ColIndex("stack_trace"), kMaxStackTraceSize>(key.stack_trace_str);
    r.Append<r.ColIndex("count")>(count);
  }
}

void PerfProfileConnector::ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table) {
  // Choose the maps to consume.
  auto& histo = transfer_count_ % 2 == 0 ? histogram_a_ : histogram_b_;
  auto& stack_traces = transfer_count_ % 2 == 0 ? stack_traces_a_ : stack_traces_b_;
  const uint32_t sample_count_idx = transfer_count_ % 2 == 0 ? kSampleCountAIdx : kSampleCountBIdx;

  ++transfer_count_;

  // First, tell BPF to switch the maps it writes to.
  const ebpf::StatusTuple s = profiler_state_->update_value(kTransferCountIdx, transfer_count_);
  LOG_IF(ERROR, !s.ok()) << "Error writing transfer_count_";

  // Read BPF stack traces & histogram, build records, incorporate records to data table.
  CreateRecords(stack_traces.get(), histo.get(), ctx, data_table);

  // Now that we've consumed the data, reset the sample count in BPF.
  profiler_state_->update_value(sample_count_idx, 0);
}

void PerfProfileConnector::TransferDataImpl(ConnectorContext* ctx,
                                            const std::vector<DataTable*>& data_tables) {
  DCHECK_EQ(data_tables.size(), 1);

  auto* data_table = data_tables[0];

  if (data_table == nullptr) {
    return;
  }

  ProcessBPFStackTraces(ctx, data_table);

  // Cleanup the symbolizer so we don't leak memory.
  proc_tracker_.Update(ctx->GetUPIDs());
  CleanupSymbolizers(proc_tracker_.deleted_upids());
}

}  // namespace stirling
}  // namespace px
