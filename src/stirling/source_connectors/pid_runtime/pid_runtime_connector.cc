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

#include "src/stirling/source_connectors/pid_runtime/pid_runtime_connector.h"

#include <string>

#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/macros.h"

OBJ_STRVIEW(pidruntime_bcc_script, pidruntime);

namespace px {
namespace stirling {

Status PIDRuntimeConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  PX_RETURN_IF_ERROR(InitBPFProgram(pidruntime_bcc_script));
  PX_RETURN_IF_ERROR(AttachSamplingProbes(kSamplingProbes));

  bpf_data_ = BPFMapDataT::Create(this, "pid_cpu_time");
  return Status::OK();
}

Status PIDRuntimeConnector::StopImpl() {
  Close();
  return Status::OK();
}

void PIDRuntimeConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  DCHECK_EQ(data_tables_.size(), 1U);
  DataTable* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

  for (auto& item : bpf_data_->GetTableOffline()) {
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

    uint64_t time = ConvertToRealTime(item.second.timestamp);

    DataTable::RecordBuilder<&kTable> r(data_table, time);
    r.Append<r.ColIndex("time_")>(time);
    r.Append<r.ColIndex("pid")>(item.first);
    r.Append<r.ColIndex("runtime_ns")>(item.second.run_time - prev_run_time);
    r.Append<r.ColIndex("cmd")>(item.second.name);

    prev_run_time_map_[item.first] = item.second.run_time;
  }
}

}  // namespace stirling
}  // namespace px
