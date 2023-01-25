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

#include "src/stirling/source_connectors/pid_runtime_bpftrace/pid_runtime_bpftrace_connector.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/macros.h"

// The following is a string_view into a BT file that is included in the binary by the linker.
// The BT files are permanently resident in memory, so the string view is permanent too.
OBJ_STRVIEW(kPIDRuntimeBTScript, bpftrace_pidruntime);

namespace px {
namespace stirling {

Status PIDCPUUseBPFTraceConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  PX_RETURN_IF_ERROR(CompileForMapOutput(kPIDRuntimeBTScript, std::vector<std::string>({})));
  PX_RETURN_IF_ERROR(Deploy());

  return Status::OK();
}

Status PIDCPUUseBPFTraceConnector::StopImpl() {
  BPFTraceWrapper::Stop();
  return Status::OK();
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

void PIDCPUUseBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  DCHECK_EQ(data_tables_.size(), 1U) << "PIDCPUUseBPFTraceConnector only has one data table.";

  auto* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

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
    r.Append<r.ColIndex("time_")>(ConvertToRealTime(timestamp));
    r.Append<r.ColIndex("pid")>(pid);
    r.Append<r.ColIndex("runtime_ns")>(cputime - last_cputime);
    r.Append<r.ColIndex("cmd")>(std::move(name));
  }

  // Keep this, because we will want to compute deltas next time.
  last_result_times_ = std::move(pid_time_pairs);
}

}  // namespace stirling
}  // namespace px
