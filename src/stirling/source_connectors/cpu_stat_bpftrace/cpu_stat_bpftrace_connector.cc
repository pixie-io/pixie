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

#include <algorithm>
#include <cstring>
#include <ctime>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/cpu_stat_bpftrace/cpu_stat_bpftrace_connector.h"

// The following is a string_view into a BT file that is included in the binary by the linker.
// The BT files are permanently resident in memory, so the string view is permanent too.
OBJ_STRVIEW(kCPUStatBTScript, cpustat);

namespace px {
namespace stirling {

Status CPUStatBPFTraceConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);

  PX_RETURN_IF_ERROR(
      CompileForMapOutput(kCPUStatBTScript, std::vector<std::string>({std::to_string(cpu_id_)})));
  PX_RETURN_IF_ERROR(Deploy());

  return Status::OK();
}

Status CPUStatBPFTraceConnector::StopImpl() {
  BPFTraceWrapper::Stop();
  return Status::OK();
}

void CPUStatBPFTraceConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  DCHECK_EQ(data_tables_.size(), 1U) << "CPUStatBPFTraceConnector only has one data table.";

  auto* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

  auto cpustat_map = GetBPFMap("@retval");

  // If kernel hasn't populated BPF map yet, then we have no data to return.
  constexpr size_t kElementsSize = sizeof(kElements) / sizeof(kElements[0]);
  if (cpustat_map.size() != kElementsSize) {
    return;
  }

  DataTable::RecordBuilder<&kTable> r(data_table);
  r.Append<r.ColIndex("time_")>(
      ConvertToRealTime(*(reinterpret_cast<int64_t*>(cpustat_map[0].second.data()))));
  r.Append<r.ColIndex("cpustat_user")>(*(reinterpret_cast<int64_t*>(cpustat_map[1].second.data())));
  r.Append<r.ColIndex("cpustat_nice")>(*(reinterpret_cast<int64_t*>(cpustat_map[2].second.data())));
  r.Append<r.ColIndex("cpustat_system")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[3].second.data())));
  r.Append<r.ColIndex("cpustat_softirq")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[4].second.data())));
  r.Append<r.ColIndex("cpustat_irq")>(*(reinterpret_cast<int64_t*>(cpustat_map[5].second.data())));
  r.Append<r.ColIndex("cpustat_idle")>(*(reinterpret_cast<int64_t*>(cpustat_map[6].second.data())));
  r.Append<r.ColIndex("cpustat_iowait")>(
      *(reinterpret_cast<int64_t*>(cpustat_map[7].second.data())));
}

}  // namespace stirling
}  // namespace px
