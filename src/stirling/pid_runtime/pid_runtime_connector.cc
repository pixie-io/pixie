#ifdef __linux__

#include "src/stirling/pid_runtime/pid_runtime_connector.h"

#include <string>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/macros.h"

BPF_SRC_STRVIEW(pidruntime_bcc_script, pidruntime);

namespace pl {
namespace stirling {

Status PIDRuntimeConnector::InitImpl() {
  PL_RETURN_IF_ERROR(InitBPFProgram(pidruntime_bcc_script));
  PL_RETURN_IF_ERROR(AttachPerfEvents(kPerfEvents));
  return Status::OK();
}

Status PIDRuntimeConnector::StopImpl() {
  BCCWrapper::Stop();
  return Status::OK();
}

void PIDRuntimeConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                           DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);

  // TODO(kgandhi): PL-452 There is an extra copy when calling get_table_offline. We should extract
  // the key when it is a struct from the BPFHASHTable directly.
  std::vector<std::pair<uint16_t, pidruntime_val_t>> items =
      bpf().get_hash_table<uint16_t, pidruntime_val_t>("pid_cpu_time").get_table_offline();

  for (auto& item : items) {
    // TODO(kgandhi): PL-460 Consider using other types of BPF tables to avoid a searching through
    // a map for the previously recorded run-time. Alternatively, calculate delta in the bpf code
    // if that is more efficient.
    auto it = prev_run_time_map_.find({item.first});
    uint64_t prev_run_time = 0;
    if (it == prev_run_time_map_.end()) {
      prev_run_time_map_.insert({item.first, item.second.run_time});
    } else {
      prev_run_time = it->second;
    }

    uint64_t time = item.second.timestamp + ClockRealTimeOffset();

    DataTable::RecordBuilder<&kTable> r(data_table, time);
    r.Append<r.ColIndex("time_")>(time);
    r.Append<r.ColIndex("pid")>(item.first);
    r.Append<r.ColIndex("runtime_ns")>(item.second.run_time - prev_run_time);
    r.Append<r.ColIndex("cmd")>(item.second.name);

    prev_run_time_map_[item.first] = item.second.run_time;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
