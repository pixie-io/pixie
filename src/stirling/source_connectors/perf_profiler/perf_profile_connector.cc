#include <sys/sysinfo.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

BPF_SRC_STRVIEW(profiler_bcc_script, profiler);

namespace pl {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables) {}

Status PerfProfileConnector::InitImpl() {
  const size_t ncpus = get_nprocs_conf();
  VLOG(1) << "PerfProfiler: get_nprocs_conf(): " << ncpus;

  const std::vector<std::string> defines = {
      absl::Substitute("-DNCPUS=$0", ncpus),
      absl::Substitute("-DPUSH_PERIOD=$0", kTargetPushPeriodMillis),
      absl::Substitute("-DSAMPLE_PERIOD=$0", kSamplingPeriodMillis)};

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
  return Status::OK();
}

Status PerfProfileConnector::StopImpl() {
  // Must call Close() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  Close();
  return Status::OK();
}

namespace {
std::string FoldedStackTraceString(ebpf::BPFStackTable* stack_traces,
                                   const stack_trace_key_t& key) {
  using SymbolsVec = std::vector<std::string>;

  SymbolsVec user_symbols = stack_traces->get_stack_symbol(key.user_stack_id, key.pid);
  SymbolsVec kernel_symbols = stack_traces->get_stack_symbol(key.kernel_stack_id, -1);

  return stack_traces::FoldedStackTraceString(user_symbols, kernel_symbols);
}
}  // namespace

void PerfProfileConnector::ProcessBPFStackTraces(ConnectorContext* ctx, DataTable* data_table) {
  auto& histo = read_and_clear_count_ % 2 == 0 ? histogram_a_ : histogram_b_;
  auto& stack_traces = read_and_clear_count_ % 2 == 0 ? stack_traces_a_ : stack_traces_b_;

  ++read_and_clear_count_;

  // TODO(jps): stop tracking these status codes, or, write a wrapper for PL_RETURN_IF_ERROR.
  uint64_t timestamp_ns;
  const ebpf::StatusTuple rd_status = profiler_state_->get_value(kTimeStampIdx, timestamp_ns);
  LOG_IF(ERROR, !rd_status.ok()) << "Error reading profiler_state_";

  timestamp_ns += ClockRealTimeOffset();

  // Read BPF stack traces & histogram, build records, incorporate records to data table.
  CreateRecords(timestamp_ns, stack_traces.get(), histo.get(), ctx, data_table);

  // Clear the map set that we have just now ingested.
  //
  // stack_traces->clear_table_non_atomic() does not return an error code,
  // but histo->clear_table_non_atomic() does. We will do what we can with these.
  //
  // TODO(jps): create a wrapper for ebpf status tuple and use PL_RETURN_IF_ERROR.
  stack_traces->clear_table_non_atomic();
  const ebpf::StatusTuple histo_clear_status = histo->clear_table_non_atomic();
  LOG_IF(ERROR, !histo_clear_status.ok()) << "Failed to clear profiler histogram BPF table.";

  // update the "read & clear count":
  // TODO(jps): do we really need to track these status codes?
  const ebpf::StatusTuple wr_status =
      profiler_state_->update_value(kUserReadAndClearCountIdx, read_and_clear_count_);
  LOG_IF(ERROR, !wr_status.ok()) << "Error writing profiler_state_";
}

void PerfProfileConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);

  uint64_t push_count = 0;

  // TODO(jps): stop tracking these status codes, or, write a wrapper for PL_RETURN_IF_ERROR.
  const ebpf::StatusTuple rd_status = profiler_state_->get_value(kBPFPushCountIdx, push_count);
  LOG_IF(ERROR, !rd_status.ok()) << "Error reading profiler_state_";

  if (push_count > read_and_clear_count_) {
    // BPF side incremented the push_count, initiating a push event.
    // Invoke ProcessBPFStackTraces() to read & clear the shared BPF maps.

    // Before ProcessBPFStackTraces(), we expect that push_count == 1+read_and_clear_count_.
    // After (and in steady state), we expect push_count==read_and_clear_count_.
    const uint64_t expected_push_count = 1 + read_and_clear_count_;
    DCHECK_EQ(push_count, expected_push_count) << "stack trace handshake protocol out of sync.";
    ProcessBPFStackTraces(ctx, data_table);
  }
  DCHECK_EQ(push_count, read_and_clear_count_) << "stack trace handshake protocol out of sync.";
}

uint64_t PerfProfileConnector::SymbolicStackTradeID(
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
    ebpf::BPFStackTable* stack_traces, ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo,
    ConnectorContext* ctx) {
  // TODO(jps): switch from using get_table_offline() to directly stepping through
  // the histogram data structure. Inline populating our own data structures with this.
  // Avoid an unnecessary copy of the information in local stack_trace_keys_and_counts.
  StackTraceHisto symbolic_histogram;
  uint64_t cum_sum_count = 0;
  for (const auto& [stack_trace_key, count] : histo->get_table_offline()) {
    cum_sum_count += count;

    // TODO(jps): use 'struct upid_t' as a field in SymbolicStackTrace
    // refactor use of ctx->getASID() to CreateRecords().
    const md::UPID upid(ctx->GetASID(), stack_trace_key.pid, stack_trace_key.start_time_ticks);

    std::string stack_trace_str = FoldedStackTraceString(stack_traces, stack_trace_key);
    SymbolicStackTrace symbolic_stack_trace = {upid, std::move(stack_trace_str)};
    symbolic_histogram[symbolic_stack_trace] += count;

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

  VLOG(1) << "PerfProfileConnector::AggregateStackTraces(): cum_sum_count: " << cum_sum_count;
  return symbolic_histogram;
}

void PerfProfileConnector::CreateRecords(const uint64_t timestamp_ns,
                                         ebpf::BPFStackTable* stack_traces,
                                         ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo,
                                         ConnectorContext* ctx, DataTable* data_table) {
  constexpr size_t kMaxSymbolSize = 512;
  constexpr size_t kMaxStackDepth = 64;
  constexpr size_t kMaxStackTraceSize = kMaxStackDepth * kMaxSymbolSize;

  // Stack traces from kernel/BPF are ordered lists of instruction pointers (addresses).
  // AggregateStackTraces() will collapse some of those into identical symbolic stack traces;
  // for example, consider the following two stack traces from BPF:
  // p0, p1, p2 => main;qux;baz   # both p2 & p3 point into baz.
  // p0, p1, p3 => main;qux;baz

  StackTraceHisto symbolic_histogram = AggregateStackTraces(stack_traces, histo, ctx);

  for (const auto& [symbolic_stack_trace, count] : symbolic_histogram) {
    DataTable::RecordBuilder<&kStackTraceTable> r(data_table, timestamp_ns);

    const uint64_t stack_trace_id = SymbolicStackTradeID(symbolic_stack_trace);

    r.Append<r.ColIndex("time_")>(timestamp_ns);
    r.Append<r.ColIndex("upid")>(symbolic_stack_trace.upid.value());
    r.Append<r.ColIndex("stack_trace_id")>(stack_trace_id);
    r.Append<r.ColIndex("stack_trace"), kMaxStackTraceSize>(symbolic_stack_trace.stack_trace_str);
    r.Append<r.ColIndex("count")>(count);
  }
}

}  // namespace stirling
}  // namespace pl
