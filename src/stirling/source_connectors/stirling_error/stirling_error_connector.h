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

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/stirling_error/probe_status_table.h"
#include "src/stirling/source_connectors/stirling_error/stirling_error_table.h"
#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {

class StirlingErrorConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "stirling_error";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kTables = MakeArray(kStirlingErrorTable, kProbeStatusTable);
  static constexpr uint32_t kStirlingErrorTableNum = TableNum(kTables, kStirlingErrorTable);
  static constexpr uint32_t kProbeStatusTableNum = TableNum(kTables, kProbeStatusTable);

  StirlingErrorConnector() = delete;
  ~StirlingErrorConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new StirlingErrorConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;

  void TransferDataImpl(ConnectorContext* ctx) override;

 private:
  explicit StirlingErrorConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {}

  void TransferStirlingErrorTable(ConnectorContext* ctx, DataTable* data_table);
  void TransferProbeStatusTable(ConnectorContext* ctx, DataTable* data_table);

  StirlingMonitor& monitor_ = *StirlingMonitor::GetInstance();
  int32_t pid_ = -1;
  uint64_t start_time_ = -1;
};

}  // namespace stirling
}  // namespace px
