#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizer.h"

BPF_SRC_STRVIEW(profiler_bcc_script, profiler);

namespace pl {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables) {}

Status PerfProfileConnector::InitImpl() {
  PL_RETURN_IF_ERROR(InitBPFProgram(profiler_bcc_script));
  PL_RETURN_IF_ERROR(AttachSamplingProbes(kSamplingProbeSpecs));
  LOG(INFO) << "PerfProfiler: Probes successfully deployed.";

  stacks_ = std::make_unique<ebpf::BPFStackTable>(bpf().get_stack_table("stack_traces"));
  counts_ = std::make_unique<ebpf::BPFHashTable<stack_trace_key_t, uint64_t>>(
      bpf().get_hash_table<stack_trace_key_t, uint64_t>("counts"));

  return Status::OK();
}

Status PerfProfileConnector::StopImpl() {
  // Must call Stop() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  bpf_tools::BCCWrapper::Stop();
  return Status::OK();
}

void PerfProfileConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);
}

std::string FoldedStackTraceString(ebpf::BPFStackTable* stack_traces,
                                   const stack_trace_key_t& key) {
  using SymbolsVec = std::vector<std::string>;

  SymbolsVec user_symbols = stack_traces->get_stack_symbol(key.user_stack_id, key.pid);
  SymbolsVec kernel_symbols = stack_traces->get_stack_symbol(key.kernel_stack_id, -1);

  return stack_traces::FoldedStackTraceString(key.name, user_symbols, kernel_symbols);
}

void PerfProfileConnector::PushRecords(ebpf::BPFStackTable* stack_traces,
                                       ebpf::BPFHashTable<stack_trace_key_t, uint64_t>* histo,
                                       DataTable* data_table) {
  constexpr size_t kMaxSymbolSize = 512;
  constexpr size_t kMaxStackDepth = 64;
  constexpr size_t kMaxStackTraceSize = kMaxStackDepth * kMaxSymbolSize;

  const uint64_t timestamp_ns = AdjustedSteadyClockNowNS();

  for (const auto& [stack_trace_key, count] : histo->get_table_offline()) {
    DataTable::RecordBuilder<&kStackTraceTable> r(data_table, timestamp_ns);

    // TODO(oazizi/jps): Populate ASID and timestamp.
    md::UPID upid(0, stack_trace_key.pid, 0);

    // TODO(jps/oazizi): Add logic to create stack_trace-id.
    uint64_t stack_trace_id = 0;

    std::string stack_trace_str = FoldedStackTraceString(stack_traces, stack_trace_key);

    r.Append<r.ColIndex("time_")>(timestamp_ns);
    r.Append<r.ColIndex("upid")>(upid.value());
    r.Append<r.ColIndex("stack_trace_id")>(stack_trace_id);
    r.Append<r.ColIndex("stack_trace"), kMaxStackTraceSize>(std::move(stack_trace_str));
    r.Append<r.ColIndex("count")>(count);
  }
}

}  // namespace stirling
}  // namespace pl
