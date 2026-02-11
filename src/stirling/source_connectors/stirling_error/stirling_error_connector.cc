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

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/source_connectors/stirling_error/stirling_error_connector.h"

namespace px {
namespace stirling {

Status StirlingErrorConnector::InitImpl() {
  // Set Stirling start_time.
  pid_ = getpid();
  const std::string proc_pid_path = std::string("/proc/") + std::to_string(pid_);
  PX_ASSIGN_OR_RETURN(start_time_, system::GetPIDStartTimeTicks(proc_pid_path));

  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

Status StirlingErrorConnector::StopImpl() { return Status::OK(); }

void StirlingErrorConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 2U) << "StirlingErrorConnector has two data tables.";

  if (data_tables_[kStirlingErrorTableNum] != nullptr) {
    TransferStirlingErrorTable(ctx, data_tables_[kStirlingErrorTableNum]);
  }

  if (data_tables_[kProbeStatusTableNum] != nullptr) {
    TransferProbeStatusTable(ctx, data_tables_[kProbeStatusTableNum]);
  }
}

void StirlingErrorConnector::TransferStirlingErrorTable(ConnectorContext* ctx,
                                                        DataTable* data_table) {
  md::UPID upid = md::UPID(ctx->GetASID(), pid_, start_time_);
  for (auto& record : monitor_.ConsumeSourceStatusRecords()) {
    DataTable::RecordBuilder<&kStirlingErrorTable> r(data_table, record.timestamp_ns);
    r.Append<r.ColIndex("time_")>(static_cast<uint64_t>(record.timestamp_ns));
    r.Append<r.ColIndex("upid")>(upid.value());
    r.Append<r.ColIndex("source_connector")>(std::move(record.source_connector));
    r.Append<r.ColIndex("status")>(static_cast<uint64_t>(record.status));
    r.Append<r.ColIndex("error")>(std::move(record.error));
    r.Append<r.ColIndex("context")>(std::move(record.context));
  }
}

void StirlingErrorConnector::TransferProbeStatusTable(ConnectorContext* ctx,
                                                      DataTable* data_table) {
  md::UPID upid = md::UPID(ctx->GetASID(), pid_, start_time_);
  for (auto& record : monitor_.ConsumeProbeStatusRecords()) {
    DataTable::RecordBuilder<&kProbeStatusTable> r(data_table, record.timestamp_ns);
    r.Append<r.ColIndex("time_")>(static_cast<uint64_t>(record.timestamp_ns));
    r.Append<r.ColIndex("upid")>(upid.value());
    r.Append<r.ColIndex("source_connector")>(std::move(record.source_connector));
    r.Append<r.ColIndex("tracepoint")>(std::move(record.tracepoint));
    r.Append<r.ColIndex("status")>(static_cast<uint64_t>(record.status));
    r.Append<r.ColIndex("error")>(std::move(record.error));
    r.Append<r.ColIndex("info")>(std::move(record.info));
  }
}

}  // namespace stirling
}  // namespace px
