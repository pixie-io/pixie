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

#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"

#include <sys/sysinfo.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bpf_tools/macros.h"

OBJ_STRVIEW(profiler_bcc_script, profiler);

DEFINE_string(stirling_profiler_symbolizer, "bcc",
              "Choice of which symbolizer to use. Options: bcc, elf");
DEFINE_bool(stirling_profiler_cache_symbols, true, "Whether to cache symbols");
DEFINE_uint32(stirling_profiler_log_period_minutes, 10,
              "Number of minutes between profiler stats log printouts.");
DEFINE_uint32(stirling_profiler_table_update_period_seconds,
              gflags::Uint32FromEnv("PL_PROFILER_UPDATE_PERIOD", 30),
              "Number of seconds between profiler table updates.");
DEFINE_uint32(stirling_profiler_stack_trace_sample_period_ms, 11,
              "Number of milliseconds between stack trace samples.");

// Scaling factor is sized to avoid hash table collisions and timing variations.
DEFINE_double(stirling_profiler_stack_trace_size_factor, 3.0,
              "Scaling factor to apply to Profiler's eBPF stack trace map sizes");

// Scaling factor for perf buffer to account for timing variations.
// This factor is smaller than the stack trace scaling factor because there is no need to account
// for hash table collisions.
DEFINE_double(stirling_profiler_perf_buffer_size_factor, 1.2,
              "Scaling factor to apply to Profiler's eBPF perf buffer sizes");

namespace px {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables),
      stack_trace_sampling_period_(
          std::chrono::milliseconds{FLAGS_stirling_profiler_stack_trace_sample_period_ms}),
      sampling_period_(
          std::chrono::milliseconds{1000 * FLAGS_stirling_profiler_table_update_period_seconds}),
      push_period_(sampling_period_ / 2),
      profiler_state_overflow_gauge_(
          BuildGauge("perf_profiler_overflow_gauge",
                     "Overflow ratio, i.e. actual:expected number of stack traces.")),
      profiler_transfer_data_counter_(
          BuildCounter("perf_profiler_transfer_data_counter",
                       "Count of times perf profiler transfer data is invoked.")),
      profiler_state_overflow_counter_(
          BuildCounter("perf_profiler_overflow",
                       "Count of times the perf profiler overran CFG_OVERRUN_THRESHOLD")),
      profiler_state_map_read_error_counter_(
          BuildCounter("perf_profiler_map_read_error",
                       "Count of times the perf profiler encountered a map lookup error")),
      stats_log_interval_(std::chrono::minutes(FLAGS_stirling_profiler_log_period_minutes) /
                          sampling_period_) {
  constexpr auto kMaxSamplingPeriod = std::chrono::milliseconds{30000};
  DCHECK(sampling_period_ <= kMaxSamplingPeriod) << "Sampling period set too high.";
  DCHECK(sampling_period_ >= stack_trace_sampling_period_);
}

Status PerfProfileConnector::InitImpl() {
  sampling_freq_mgr_.set_period(sampling_period_);
  push_freq_mgr_.set_period(push_period_);

  const size_t ncpus = get_nprocs_conf();

  // Compute sizes of eBPF data structures:

  // Given the targeted "transfer period" and the "stack trace sample period",
  // we can find the number of entries required to be allocated in each of the maps,
  // i.e. the number of expected stack traces:
  const int32_t expected_stack_traces_per_cpu =
      IntRoundUpDivide(sampling_period_.count(), stack_trace_sampling_period_.count());

  // Because sampling occurs per-cpu, the total number of expected stack traces is:
  expected_stack_traces_ = ncpus * expected_stack_traces_per_cpu;

  // Include some margin to ensure that hash collisions and data races do not cause data drop:
  const double stack_traces_overprovision_factor = FLAGS_stirling_profiler_stack_trace_size_factor;

  // Compute the size of the stack traces map.
  const int32_t provisioned_stack_traces =
      static_cast<int32_t>(stack_traces_overprovision_factor * expected_stack_traces_);

  // A threshold for checking that we've overrun the maps.
  // This should be higher than expected_stack_traces due to timing variations,
  // but it should be lower than provisioned_stack_traces.
  const int32_t overrun_threshold = (expected_stack_traces_ + provisioned_stack_traces) / 2;

  // Compute the size of the perf buffers.
  const double perf_buffer_overprovision_factor = FLAGS_stirling_profiler_perf_buffer_size_factor;
  const int32_t num_perf_buffer_entries =
      static_cast<int32_t>(perf_buffer_overprovision_factor * expected_stack_traces_per_cpu);

  // Perf buffer entries use the following struct:
  //      struct {
  //        struct perf_event_header {
  //          __u32   type;
  //          __u16   misc;
  //          __u16   size;
  //        } header;
  //        u32    size;        /* if PERF_SAMPLE_RAW */
  //        char  data[size];   /* if PERF_SAMPLE_RAW */
  //      };
  // The entire struct as a whole is 12-bytes + data[size].
  const int32_t perf_buffer_entry_size =
      sizeof(struct perf_event_header) + sizeof(uint32_t) + sizeof(stack_trace_key_t);
  const int32_t perf_buffer_size = perf_buffer_entry_size * num_perf_buffer_entries;

  const std::vector<std::string> defines = {
      absl::Substitute("-DCFG_STACK_TRACE_ENTRIES=$0", provisioned_stack_traces),
      absl::Substitute("-DCFG_OVERRUN_THRESHOLD=$0", overrun_threshold),
  };

  const auto probe_specs = MakeArray<bpf_tools::SamplingProbeSpec>(
      {"sample_call_stack", static_cast<uint64_t>(stack_trace_sampling_period_.count())});

  const auto perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>(
      {{std::string(kHistogramAName), HandleHistoEvent, HandleHistoLoss, perf_buffer_size},
       {std::string(kHistogramBName), HandleHistoEvent, HandleHistoLoss, perf_buffer_size}});

  PX_RETURN_IF_ERROR(InitBPFProgram(profiler_bcc_script, defines));
  PX_RETURN_IF_ERROR(AttachSamplingProbes(probe_specs));
  PX_RETURN_IF_ERROR(OpenPerfBuffers(perf_buffer_specs, this));

  stack_traces_a_ = WrappedBCCStackTable::Create(this, "stack_traces_a");
  stack_traces_b_ = WrappedBCCStackTable::Create(this, "stack_traces_b");

  profiler_state_ = WrappedBCCArrayTable<uint64_t>::Create(this, "profiler_state");

  LOG(INFO) << "PerfProfiler: Stack trace profiling sampling probe successfully deployed.";

  // Create a symbolizer for user symbols.
  if (FLAGS_stirling_profiler_symbolizer == "bcc") {
    PX_ASSIGN_OR_RETURN(u_symbolizer_, BCCSymbolizer::Create());
  } else if (FLAGS_stirling_profiler_symbolizer == "elf") {
    PX_ASSIGN_OR_RETURN(u_symbolizer_, ElfSymbolizer::Create());
  } else {
    return error::Internal("Unrecognized symbolizer $0", FLAGS_stirling_profiler_symbolizer);
  }

  // Create a symbolizer for kernel symbols.
  // Kernel symbolizer always uses BCC symbolizer.
  PX_ASSIGN_OR_RETURN(k_symbolizer_, BCCSymbolizer::Create());

  if (FLAGS_stirling_profiler_java_symbols) {
    LOG(INFO) << "PerfProfiler: Java symbolization enabled.";
    PX_ASSIGN_OR_RETURN(u_symbolizer_, JavaSymbolizer::Create(std::move(u_symbolizer_)));
  } else {
    LOG(INFO) << "PerfProfiler: Java symbolization disabled.";
  }

  if (FLAGS_stirling_profiler_cache_symbols) {
    // Add a caching layer on top of the existing symbolizer.
    PX_ASSIGN_OR_RETURN(u_symbolizer_, CachingSymbolizer::Create(std::move(u_symbolizer_)));
    PX_ASSIGN_OR_RETURN(k_symbolizer_, CachingSymbolizer::Create(std::move(k_symbolizer_)));
  }

  return Status::OK();
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
    u_symbolizer_->DeleteUPID(upid);
  }

  if (FLAGS_stirling_profiler_cache_symbols) {
    size_t evict_count;

    evict_count = static_cast<CachingSymbolizer*>(u_symbolizer_.get())->PerformEvictions();
    VLOG(1) << absl::Substitute("PerfProfiler symbol cache: Evicted $0 user symbols.", evict_count);

    evict_count = static_cast<CachingSymbolizer*>(k_symbolizer_.get())->PerformEvictions();
    VLOG(1) << absl::Substitute("PerfProfiler symbol cache: Evicted $0 kernel symbols.",
                                evict_count);
  }
}

PerfProfileConnector::StackTraceHisto PerfProfileConnector::AggregateStackTraces(
    ConnectorContext* ctx, WrappedBCCStackTable* stack_traces) {
  // TODO(jps): switch from using get_table_offline() to directly stepping through
  // the histogram data structure. Inline populating our own data structures with this.
  // Avoid an unnecessary copy of the information in local stack_trace_keys_and_counts.
  StackTraceHisto symbolic_histogram;
  uint64_t cum_sum_count = 0;

  const uint32_t asid = ctx->GetASID();

  // Cause symbolizers to perform any necessary updates before we put them to work.
  u_symbolizer_->IterationPreTick();
  k_symbolizer_->IterationPreTick();

  // Create a new stringifier for this iteration of the continuous perf profiler.
  Stringifier stringifier(u_symbolizer_.get(), k_symbolizer_.get(), stack_traces);

  absl::flat_hash_set<int> k_stack_ids_to_remove;

  for (const auto& stack_trace_key : raw_histo_data_) {
    std::string stack_trace_str;

    const md::UPID upid(asid, stack_trace_key.upid.pid, stack_trace_key.upid.start_time_ticks);

    if (ctx->UPIDIsInContext(upid)) {
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
        stack_traces->ClearStackID(stack_trace_key.user_stack_id);
      }
      if (stack_trace_key.kernel_stack_id >= 0) {
        k_stack_ids_to_remove.insert(stack_trace_key.kernel_stack_id);
      }
      stack_trace_str = std::string(profiler::kNotSymbolizedMessage);
    }

    profiler::SymbolicStackTrace symbolic_stack_trace = {upid, std::move(stack_trace_str)};

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
    stack_traces->ClearStackID(k_stack_id);
  }

  raw_histo_data_.clear();

  VLOG(1) << "PerfProfileConnector::AggregateStackTraces(): cum_sum_count: " << cum_sum_count;
  stats_.Increment(StatKey::kCumulativeSumOfAllStackTraces, cum_sum_count);
  return symbolic_histogram;
}

void PerfProfileConnector::CreateRecords(WrappedBCCStackTable* stack_traces, ConnectorContext* ctx,
                                         DataTable* data_table) {
  constexpr size_t kMaxSymbolSize = 512;
  constexpr size_t kMaxStackDepth = 64;
  constexpr size_t kMaxStackTraceSize = kMaxStackDepth * kMaxSymbolSize;

  const uint64_t timestamp_ns = AdjustedSteadyClockNowNS();

  // Stack traces from kernel/BPF are ordered lists of instruction pointers (addresses).
  // AggregateStackTraces() will collapse some of those into identical symbolic stack traces;
  // for example, consider the following two stack traces from BPF:
  // p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
  // p0, p1, p3 => main;qux;baz

  StackTraceHisto stack_trace_histogram = AggregateStackTraces(ctx, stack_traces);

  constexpr auto age_tick_period = std::chrono::minutes(5);
  if (sampling_freq_mgr_.count() % (age_tick_period / sampling_period_) == 0) {
    stack_trace_ids_.AgeTick();
  }

  for (const auto& [key, count] : stack_trace_histogram) {
    DataTable::RecordBuilder<&kStackTraceTable> r(data_table, timestamp_ns);

    r.Append<r.ColIndex("time_")>(timestamp_ns);
    r.Append<r.ColIndex("upid")>(key.upid.value());
    r.Append<r.ColIndex("stack_trace_id")>(stack_trace_ids_.Lookup(key));
    r.Append<r.ColIndex("stack_trace")>(key.stack_trace_str, kMaxStackTraceSize);
    r.Append<r.ColIndex("count")>(count);
  }
}

void PerfProfileConnector::ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table) {
  // Choose the maps to consume.
  const bool using_map_set_a = transfer_count_ % 2 == 0;
  auto& stack_traces = using_map_set_a ? stack_traces_a_ : stack_traces_b_;
  auto& perfbuf_name = using_map_set_a ? kHistogramAName : kHistogramBName;
  const uint32_t sample_count_idx = using_map_set_a ? kSampleCountAIdx : kSampleCountBIdx;

  // Read out the perf buffer that contains the histogram for this iteration.
  // TODO(jps): change PollPerfBuffer() to use std::chrono.
  constexpr int kPollTimeoutMS = 0;
  PollPerfBuffer(perfbuf_name, kPollTimeoutMS);

  ++transfer_count_;

  // First, tell BPF to switch the maps it writes to.
  const auto s = profiler_state_->SetValue(kTransferCountIdx, transfer_count_);
  LOG_IF(ERROR, !s.ok()) << "Error writing transfer_count_";

  // Read BPF stack traces & histogram, build records, incorporate records to data table.
  CreateRecords(stack_traces.get(), ctx, data_table);

  const uint64_t num_stack_traces_sampled = profiler_state_->GetValue(sample_count_idx).ValueOr(0);
  CheckProfilerState(num_stack_traces_sampled);

  // Now that we've consumed the data, reset the sample count in BPF.
  PX_UNUSED(profiler_state_->SetValue(sample_count_idx, 0));
}

void PerfProfileConnector::CheckProfilerState(const uint64_t num_stack_traces) {
  const uint64_t error_code =
      profiler_state_->GetValue(kErrorStatusIdx).ValueOr(kPerfProfilerStatusOk);

  DCHECK_EQ(error_code, kPerfProfilerStatusOk);

  switch (error_code) {
    case kOverflowError: {
      // overflow_ratio is actual:expected. That is, the actual number of stack traces sampled
      // vs. the expected number of stack traces. We keep its max value in the gauge.
      const double overflow_ratio =
          static_cast<double>(num_stack_traces) / static_cast<double>(expected_stack_traces_);
      if (overflow_ratio > profiler_state_overflow_gauge_.Value()) {
        profiler_state_overflow_gauge_.Set(overflow_ratio);
      }

      // Compute the increment to profiler_transfer_data_counter_ such that the counter value is
      // equal to the total number of transfer data invocations.
      const double current_transfer_counter = profiler_transfer_data_counter_.Value();
      const double transfer_count_increment = transfer_count_ - current_transfer_counter;

      // Track the total number of overflows and the total number of transfer data invocations.
      profiler_transfer_data_counter_.Increment(transfer_count_increment);
      profiler_state_overflow_counter_.Increment();
      break;
    }
    case kMapReadFailureError: {
      profiler_state_map_read_error_counter_.Increment();
      break;
    }
  }
  // Reset the BPF map to its default value so that each occurrence
  // can be detected.
  if (error_code != kPerfProfilerStatusOk) {
    PX_UNUSED(profiler_state_->SetValue(kErrorStatusIdx, kPerfProfilerStatusOk));
  }
}

void PerfProfileConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 1U);

  auto* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

  ProcessBPFStackTraces(ctx, data_table);

  // Cleanup the symbolizer so we don't leak memory.
  proc_tracker_.Update(ctx->GetUPIDs());
  CleanupSymbolizers(proc_tracker_.deleted_upids());

  stats_.Increment(StatKey::kBPFMapSwitchoverEvent, 1);

  if (sampling_freq_mgr_.count() % stats_log_interval_ == 0) {
    PrintStats();
  }
}

void PerfProfileConnector::PrintStats() const {
  LOG(INFO) << "PerfProfileConnector statistics: " << stats_.Print();
  if (FLAGS_stirling_profiler_cache_symbols) {
    auto u_symbolizer = static_cast<CachingSymbolizer*>(u_symbolizer_.get());
    auto k_symbolizer = static_cast<CachingSymbolizer*>(k_symbolizer_.get());
    const uint64_t u_hits = u_symbolizer->stat_hits();
    const uint64_t k_hits = k_symbolizer->stat_hits();
    const uint64_t u_accesses = u_symbolizer->stat_accesses();
    const uint64_t k_accesses = k_symbolizer->stat_accesses();
    const uint64_t u_num_symbols = u_symbolizer->GetNumberOfSymbolsCached();
    const uint64_t k_num_symbols = k_symbolizer->GetNumberOfSymbolsCached();
    const double u_hit_rate =
        u_accesses == 0 ? 0 : 100.0 * static_cast<double>(u_hits) / static_cast<double>(u_accesses);
    const double k_hit_rate =
        k_accesses == 0 ? 0 : 100.0 * static_cast<double>(k_hits) / static_cast<double>(k_accesses);
    LOG(INFO) << absl::Substitute(
        "PerfProfileConnector u_symbolizer num_symbols_cached=$0 hits=$1 accesses=$2 hit_rate=$3",
        u_num_symbols, u_hits, u_accesses, u_hit_rate);
    LOG(INFO) << absl::Substitute(
        "PerfProfileConnector k_symbolizer num_symbols_cached=$0 hits=$1 accesses=$2 hit_rate=$3",
        k_num_symbols, k_hits, k_accesses, k_hit_rate);
  }
}

}  // namespace stirling
}  // namespace px
