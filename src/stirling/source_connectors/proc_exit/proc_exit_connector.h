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
#include <vector>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

// This connector is not registered yet, so it has no effect.
class ProcExitConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "proc_exit";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{100};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};
  // clang-format off
  static constexpr DataElement kElements[] = {
      canonical_data_elements::kTime,
  };
  // clang-format on
  static constexpr auto kTable =
      DataTableSchema("proc_exit", "Traces all abnormal process exits", kElements);
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new ProcExitConnector(name));
  }

  ProcExitConnector() = delete;
  ~ProcExitConnector() override = default;

 protected:
  explicit ProcExitConnector(std::string_view name) : SourceConnector(name, kTables) {}

  Status InitImpl() override;
  void TransferDataImpl(ConnectorContext* ctx, const std::vector<DataTable*>& data_tables) override;
  Status StopImpl() override { return Status::OK(); }
};

}  // namespace stirling
}  // namespace px
