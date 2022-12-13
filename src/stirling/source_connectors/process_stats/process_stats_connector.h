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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/process_stats/process_stats_table.h"

namespace px {
namespace stirling {

class ProcessStatsConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "process_stats";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kTables = MakeArray(kProcessStatsTable);
  static constexpr uint32_t kProcStatsTableNum = TableNum(kTables, kProcessStatsTable);

  ProcessStatsConnector() = delete;
  ~ProcessStatsConnector() override = default;

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcessStatsConnector(name));
  }

  Status InitImpl() override;

  Status StopImpl() override;

  void TransferDataImpl(ConnectorContext* ctx) override;

 protected:
  explicit ProcessStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {
    proc_parser_ = std::make_unique<system::ProcParser>();
  }

 private:
  void TransferProcessStatsTable(ConnectorContext* ctx, DataTable* data_table);

  std::unique_ptr<system::ProcParser> proc_parser_;
};

}  // namespace stirling
}  // namespace px
