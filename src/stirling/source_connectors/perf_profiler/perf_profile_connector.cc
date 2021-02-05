#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"

#include <memory>

#include "src/stirling/bpf_tools/macros.h"

BPF_SRC_STRVIEW(profile_bcc_script, profile);

namespace pl {
namespace stirling {

PerfProfileConnector::PerfProfileConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables) {}

Status PerfProfileConnector::InitImpl() {
  PL_RETURN_IF_ERROR(InitBPFProgram(profile_bcc_script));
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

}  // namespace stirling
}  // namespace pl
