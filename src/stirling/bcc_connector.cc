#ifdef __linux__

#include <linux/perf_event.h>
#include <linux/sched.h>

#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "src/common/utils.h"
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

RawDataBuf PIDCPUUseBCCConnector::GetDataImpl() {
  // TODO(kgandhi): PL-452 There is an extra copy when calling get_table_offline. We should extract
  // the key when it is a struct from the BPFHASHTable directly.
  table_ = bpf_.get_hash_table<uint16_t, pl_stirling_bcc_pidruntime_val>("pid_cpu_time")
               .get_table_offline();

  data_buf_.clear();
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
    data_buf_.push_back(item.second.time_stamp + ClockRealTimeOffset());
    data_buf_.push_back(static_cast<uint64_t>(item.first));
    data_buf_.push_back(item.second.run_time - prev_run_time);
    data_buf_.push_back(reinterpret_cast<uint64_t>(&item.second.name));
    prev_run_time_map_[item.first] = item.second.run_time;
  }

  return RawDataBuf(table_.size(), reinterpret_cast<uint8_t*>(data_buf_.data()));
}

}  // namespace stirling
}  // namespace pl

#endif
