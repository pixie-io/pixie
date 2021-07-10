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

DEFINE_uint32(stirling_perf_profiler_stats_logging_ratio,
              std::chrono::minutes(10) / px::stirling::PerfProfileConnector::kSamplingPeriod,
              "Sets the frequency of printing perf profiler stats.");

namespace px {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables) {}

Status PerfProfileConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  const size_t ncpus = get_nprocs_conf();
  VLOG(1) << "PerfProfiler: get_nprocs_conf(): " << ncpus;

  const std::vector<std::string> defines = {
      absl::Substitute("-DNCPUS=$0", ncpus),
      absl::Substitute("-DTRANSFER_PERIOD=$0", kSamplingPeriod.count()),
      absl::Substitute("-DSAMPLE_PERIOD=$0", kBPFSamplingPeriod.count())};

  PL_RETURN_IF_ERROR(InitBPFProgram(profiler_bcc_script, defines));
  PL_RETURN_IF_ERROR(AttachSamplingProbes(kProbeSpecs));
  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));

  stack_traces_a_ = std::make_unique<ebpf::BPFStackTable>(GetStackTable("stack_traces_a"));
  stack_traces_b_ = std::make_unique<ebpf::BPFStackTable>(GetStackTable("stack_traces_b"));

  histogram_a_perf_buffer_ = GetPerfBuffer("histogram_a");
  histogram_b_perf_buffer_ = GetPerfBuffer("histogram_b");

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

void PerfProfileConnector::AcceptStackTraceKey(stack_trace_key_t* data) {
  raw_histo_data_.push_back(*data);
}

void PerfProfileConnector::HandleHistoEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<PerfProfileConnector*>(cb_cookie);
  auto* histo_key_ptr = static_cast<stack_trace_key_t*>(data);
  connector->AcceptStackTraceKey(histo_key_ptr);
}

void PerfProfileConnector::HandleHistoLoss(void* cb_cookie, uint64_t lost) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<PerfProfileConnector*>(cb_cookie);
  connector->stats_.Increment(StatKey::kLossHistoEvent, lost);
}

void PerfProfileConnector::CleanupSymbolizers(const absl::flat_hash_set<md::UPID>& deleted_upids) {
  for (const auto& md_upid : deleted_upids) {
    // Clean-up caches.
    struct upid_t upid;
    upid.pid = md_upid.pid();
    upid.start_time_ticks = md_upid.start_ts();
    symbolizer_.FlushCache(upid);
  }
}

PerfProfileConnector::StackTraceHisto PerfProfileConnector::AggregateStackTraces(
    ConnectorContext* ctx, ebpf::BPFStackTable* stack_traces) {
  // TODO(jps): switch from using get_table_offline() to directly stepping through
  // the histogram data structure. Inline populating our own data structures with this.
  // Avoid an unnecessary copy of the information in local stack_trace_keys_and_counts.
  StackTraceHisto symbolic_histogram;
  uint64_t cum_sum_count = 0;

  const uint32_t asid = ctx->GetASID();
  const absl::flat_hash_set<md::UPID>& upids_for_symbolization = ctx->GetUPIDs();

  // Create a new stringifier for this iteration of the continuous perf profiler.
  Stringifier stringifier(&symbolizer_, stack_traces);

  absl::flat_hash_set<int> k_stack_ids_to_remove;

  for (const auto& stack_trace_key : raw_histo_data_) {
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

    ++symbolic_histogram[symbolic_stack_trace];
    ++cum_sum_count;

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

  raw_histo_data_.clear();

  VLOG(1) << "PerfProfileConnector::AggregateStackTraces(): cum_sum_count: " << cum_sum_count;
  stats_.Increment(StatKey::kCumulativeSumOfAllStackTraces, cum_sum_count);
  return symbolic_histogram;
}

void PerfProfileConnector::CreateRecords(ebpf::BPFStackTable* stack_traces, ConnectorContext* ctx,
                                         DataTable* data_table) {
  constexpr size_t kMaxSymbolSize = 512;
  constexpr size_t kMaxStackDepth = 64;
  constexpr size_t kMaxStackTraceSize = kMaxStackDepth * kMaxSymbolSize;

  const uint64_t timestamp_ns = CurrentTimeNS();

  // Stack traces from kernel/BPF are ordered lists of instruction pointers (addresses).
  // AggregateStackTraces() will collapse some of those into identical symbolic stack traces;
  // for example, consider the following two stack traces from BPF:
  // p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
  // p0, p1, p3 => main;qux;baz

  StackTraceHisto stack_trace_histogram = AggregateStackTraces(ctx, stack_traces);

  constexpr auto age_tick_period = std::chrono::minutes(5);
  if (sampling_freq_mgr_.count() % (age_tick_period / kSamplingPeriod) == 0) {
    stack_trace_ids_.AgeTick();
  }

  for (const auto& [key, count] : stack_trace_histogram) {
    DataTable::RecordBuilder<&kStackTraceTable> r(data_table, timestamp_ns);

    r.Append<r.ColIndex("time_")>(timestamp_ns);
    r.Append<r.ColIndex("upid")>(key.upid.value());
    r.Append<r.ColIndex("stack_trace_id")>(stack_trace_ids_.Lookup(key));
    r.Append<r.ColIndex("stack_trace"), kMaxStackTraceSize>(key.stack_trace_str);
    r.Append<r.ColIndex("count")>(count);
  }
}

void PerfProfileConnector::ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table) {
  // Choose the maps to consume.
  const bool using_map_set_a = transfer_count_ % 2 == 0;
  auto& stack_traces = using_map_set_a ? stack_traces_a_ : stack_traces_b_;
  auto& histo_perf_buf = using_map_set_a ? histogram_a_perf_buffer_ : histogram_b_perf_buffer_;
  const uint32_t sample_count_idx = using_map_set_a ? kSampleCountAIdx : kSampleCountBIdx;

  // Read out the perf buffer that contains the histogram for this iteration.
  // TODO(jps): change PollPerfBuffer() to use std::chrono.
  constexpr int kPollTimeoutMS = 0;
  histo_perf_buf->poll(kPollTimeoutMS);

  ++transfer_count_;

  // First, tell BPF to switch the maps it writes to.
  const ebpf::StatusTuple s = profiler_state_->update_value(kTransferCountIdx, transfer_count_);
  LOG_IF(ERROR, !s.ok()) << "Error writing transfer_count_";

  // Read BPF stack traces & histogram, build records, incorporate records to data table.
  CreateRecords(stack_traces.get(), ctx, data_table);

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

  stats_.Increment(StatKey::kBPFMapSwitchoverEvent, 1);

  if (sampling_freq_mgr_.count() % FLAGS_stirling_perf_profiler_stats_logging_ratio == 0) {
    VLOG(1) << "PerfProfileConnector statistics: " << stats_.Print();
  }
}

}  // namespace stirling
}  // namespace px
