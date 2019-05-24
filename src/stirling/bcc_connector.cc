#ifdef __linux__

#include <linux/perf_event.h>
#include <linux/sched.h>

#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "src/common/base/base.h"
#include "src/stirling/bcc_connector.h"

namespace pl {
namespace stirling {

Status PIDCPUUseBCCConnector::InitImpl() {
  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }
  auto init_res = bpf_.init(std::string(kBCCScript));
  if (init_res.code() != 0) {
    return error::Internal("Unable to initialize BCC BPF program: $0", init_res.msg());
  }
  auto attach_res =
      bpf_.attach_perf_event(event_type_, event_config_, kFunctionName, 0, kSamplingFreq);
  if (attach_res.code() != 0) {
    return error::Internal("Unable to execute BCC BPF program: $0", attach_res.msg());
  }
  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  InitClockRealTimeOffset();
  return Status::OK();
}

Status PIDCPUUseBCCConnector::StopImpl() {
  // TODO(kgandhi): PL-453  Figure out a fix for below warning.
  // WARNING: Detaching perf events based on event_type_ and event_config_ might
  // end up removing the perf event if there was another source with the same perf event and
  // config. Should be rare but may still be an issue.
  auto detach_res = bpf_.detach_perf_event(event_type_, event_config_);
  if (detach_res.code() != 0) {
    return error::Internal("Unable to STOP BCC BPF program: $0", detach_res.msg());
  }
  return Status::OK();
}

void PIDCPUUseBCCConnector::TransferDataImpl(uint32_t table_num,
                                             types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(table_num, 0ULL) << absl::StrFormat(
      "This connector has only one table, but access to table_num=%d", table_num);

  auto& columns = *record_batch;

  // TODO(kgandhi): PL-452 There is an extra copy when calling get_table_offline. We should extract
  // the key when it is a struct from the BPFHASHTable directly.
  table_ = bpf_.get_hash_table<uint16_t, pl_stirling_bcc_pidruntime_val>("pid_cpu_time")
               .get_table_offline();

  for (auto& item : table_) {
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

    columns[0]->Append<types::Time64NSValue>(item.second.timestamp + ClockRealTimeOffset());
    columns[1]->Append<types::Int64Value>(item.first);
    columns[2]->Append<types::Int64Value>(item.second.run_time - prev_run_time);
    columns[3]->Append<types::StringValue>(item.second.name);

    prev_run_time_map_[item.first] = item.second.run_time;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
