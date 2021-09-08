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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The MemorySourceIR is a dual logical plan
 * and IR node operator. It inherits from both classes
 */
class EmptySourceIR : public OperatorIR {
 public:
  EmptySourceIR() = delete;
  explicit EmptySourceIR(int64_t id) : OperatorIR(id, IRNodeType::kEmptySource) {}

  /**
   * @brief Initialize the EmptySource. Must have types already decided before.
   *
   * @param table_name the table to load.
   * @param select_columns the columns to select. If vector is empty, then select all columns.
   * @return Status
   */
  Status Init(TypePtr type);

  Status ToProto(planpb::Operator*) const override;

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return std::vector<absl::flat_hash_set<std::string>>{};
  }
  bool IsSource() const override { return true; }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) override;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
