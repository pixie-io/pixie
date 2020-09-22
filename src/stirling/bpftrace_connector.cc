#ifdef __linux__
#include <algorithm>
#include <cstring>
#include <ctime>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpftrace_connector.h"

// The following are string_views into BT files that are included in the binary by the linker.
// The BT files are permanently resident in memory, so the string view is permanent too.
BCC_SRC_STRVIEW(kCPUStatBTScript, cpustat);
BCC_SRC_STRVIEW(kPIDRuntimeBTScript, pidruntime);

namespace pl {
namespace stirling {

BPFTraceConnector::BPFTraceConnector(std::string_view source_name,
                                     const ArrayView<DataTableSchema>& table_schemas,
                                     const std::string_view script, std::vector<std::string> params)
    : SourceConnector(source_name, table_schemas), script_(script), params_(std::move(params)) {}

Status BPFTraceConnector::InitImpl() {
  PL_RETURN_IF_ERROR(Compile(script_, params_));
  PL_RETURN_IF_ERROR(Deploy());

  return Status::OK();
}

CPUStatBPFTraceConnector::CPUStatBPFTraceConnector(std::string_view name, uint64_t cpu_id)
    : BPFTraceConnector(name, kTables, kCPUStatBTScript,
                        std::vector<std::string>({std::to_string(cpu_id)})) {}

void CPUStatBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                                DataTable* data_table) {
  CHECK_LT(table_num, kTables.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);

  auto cpustat_map = GetBPFMap("@retval");

  // If kernel hasn't populated BPF map yet, then we have no data to return.
  constexpr size_t kElementsSize = sizeof(kElements) / sizeof(kElements[0]);
  if (cpustat_map.size() != kElementsSize) {
    return;
  }

  DataTable::RecordBuilder<&kTable> r(data_table);
  r.Append<r.ColIndex("time_")>(*(reinterpret_cast<int64_t*>(cpustat_map[0].second.data())) +
                                ClockRealTimeOffset());
  r.Append<r.ColIndex("cpustat_user")>(*(reinterpret_cast<int64_t*>(cpustat_map[1].second.data())));
  r.Append<r.ColIndex("cpustat_nice")>(*(reinterpret_cast<int64_t*>(cpustat_map[2].second.data())));
  r.Append<r.ColIndex("cpustat_system")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[3].second.data())));
  r.Append<r.ColIndex("cpustat_idle")>(*(reinterpret_cast<int64_t*>(cpustat_map[4].second.data())));
  r.Append<r.ColIndex("cpustat_iowait")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[5].second.data())));
  r.Append<r.ColIndex("cpustat_irq")>(*(reinterpret_cast<int64_t*>(cpustat_map[6].second.data())));
  r.Append<r.ColIndex("cpustat_softirq")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[7].second.data())));
}

// Helper function for searching through a BPFTraceMap vector of key-value pairs.
// Note that the vector is sorted by keys, and the search is performed sequentially.
// The search will stop as soon as a key >= the search key is found (not just ==).
// This serves two purposes:
// (1) It enables a quicker return.
// (2) It enables resumed searching, when the next search key is >= the previous search key.
// The latter is significant when iteratively comparing elements between two sorted vectors,
// which is the main use case for this function.
// To enable the resumed searching, this function takes the start iterator as an input.
bpftrace::BPFTraceMap::iterator PIDCPUUseBPFTraceConnector::BPFTraceMapSearch(
    const bpftrace::BPFTraceMap& vector, bpftrace::BPFTraceMap::iterator it, uint64_t search_key) {
  auto next_it =
      std::find_if(it, const_cast<bpftrace::BPFTraceMap&>(vector).end(),
                   [&search_key](const std::pair<std::vector<uint8_t>, std::vector<uint8_t>>& x) {
                     return *(reinterpret_cast<const uint32_t*>(x.first.data())) >= search_key;
                   });
  return next_it;
}

PIDCPUUseBPFTraceConnector::PIDCPUUseBPFTraceConnector(std::string_view name)
    : BPFTraceConnector(name, kTables, kPIDRuntimeBTScript, std::vector<std::string>({})) {}

void PIDCPUUseBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                                  DataTable* data_table) {
  CHECK_LT(table_num, kTables.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);

  auto pid_time_pairs = GetBPFMap("@total_time");
  auto pid_name_pairs = GetBPFMap("@names");

  // This is a special map with only one entry at location 0.
  auto sampling_time = GetBPFMap("@time");
  CHECK_EQ(1ULL, sampling_time.size());
  auto timestamp = *(reinterpret_cast<int64_t*>(sampling_time[0].second.data()));

  auto last_result_it = last_result_times_.begin();
  auto pid_name_it = pid_name_pairs.begin();

  for (auto& pid_time_pair : pid_time_pairs) {
    auto key = pid_time_pair.first;
    auto value = pid_time_pair.second;

    uint64_t cputime = *(reinterpret_cast<uint64_t*>(value.data()));

    DCHECK_EQ(4ULL, key.size()) << "Expected uint32_t key";
    uint64_t pid = *(reinterpret_cast<uint32_t*>(key.data()));

    // Get the name from the auxiliary BPFTraceMap for names.
    std::string name("-");
    pid_name_it = BPFTraceMapSearch(pid_name_pairs, pid_name_it, pid);
    if (pid_name_it != pid_name_pairs.end()) {
      uint32_t found_pid = *(reinterpret_cast<uint32_t*>(pid_name_it->first.data()));
      if (found_pid == pid) {
        name = std::string(reinterpret_cast<char*>(pid_name_it->second.data()));
      } else {
        // Couldn't find the name for the PID.
        LOG(WARNING) << absl::StrFormat("Could not find a name for the PID %d", pid);
      }
    }

    // Get the last cpu time from the BPFTraceMap from previous call to this function.
    uint64_t last_cputime = 0;
    last_result_it = BPFTraceMapSearch(last_result_times_, last_result_it, pid);
    if (last_result_it != last_result_times_.end()) {
      uint32_t found_pid = *(reinterpret_cast<uint32_t*>(last_result_it->first.data()));
      if (found_pid == pid) {
        last_cputime = *(reinterpret_cast<uint64_t*>(last_result_it->second.data()));
      }
    }

    DataTable::RecordBuilder<&kTable> r(data_table);
    r.Append<r.ColIndex("time_")>(timestamp + ClockRealTimeOffset());
    r.Append<r.ColIndex("pid")>(pid);
    r.Append<r.ColIndex("runtime_ns")>(cputime - last_cputime);
    r.Append<r.ColIndex("cmd")>(std::move(name));
  }

  // Keep this, because we will want to compute deltas next time.
  last_result_times_ = std::move(pid_time_pairs);
}

Status BPFTraceConnector::StopImpl() {
  BPFTraceWrapper::Stop();
  return Status::OK();
}

}  // namespace stirling
}  // namespace pl

#endif
