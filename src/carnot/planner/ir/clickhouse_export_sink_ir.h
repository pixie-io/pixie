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

#include <absl/container/flat_hash_set.h>
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The IR representation for the ClickHouseExportSink operator.
 * Represents a configuration to export a DataFrame to a ClickHouse database.
 */
class ClickHouseExportSinkIR : public SinkOperatorIR {
 public:
  explicit ClickHouseExportSinkIR(int64_t id, std::string mutation_id)
      : SinkOperatorIR(id, IRNodeType::kClickHouseExportSink, mutation_id) {}

  Status Init(OperatorIR* parent, const std::string& table_name, const std::string& clickhouse_dsn);

  StatusOr<planpb::ClickHouseConfig> ParseClickHouseDSN(const std::string& dsn);

  Status ToProto(planpb::Operator* op) const override;

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>*) override;

  Status ResolveType(CompilerState* compiler_state);
  inline bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  const std::string& table_name() const { return table_name_; }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to ClickHouseExportSinkIR::PruneOutputColumnsTo.");
  }

 private:
  std::string table_name_;
  absl::flat_hash_set<std::string> required_column_names_;
  std::unique_ptr<planpb::ClickHouseConfig> clickhouse_config_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
