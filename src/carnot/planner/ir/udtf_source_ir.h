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
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class UDTFSourceIR : public OperatorIR {
 public:
  UDTFSourceIR() = delete;
  explicit UDTFSourceIR(int64_t id) : OperatorIR(id, IRNodeType::kUDTFSource) {}

  Status Init(std::string_view func_name,
              const absl::flat_hash_map<std::string, ExpressionIR*>& arg_values,
              const udfspb::UDTFSourceSpec& udtf_spec);

  /**
   * @brief Manages the overhead of setting argument values of the UDTFSourceOperator.
   *
   * @param expr
   * @return OK or error if one occurs.
   */
  Status SetArgValues(const std::vector<ExpressionIR*>& arg_values);

  Status InitArgValues(const absl::flat_hash_map<std::string, ExpressionIR*>& arg_values,
                       const udfspb::UDTFSourceSpec& udtf_spec);

  Status ToProto(planpb::Operator*) const override;
  std::string func_name() const { return func_name_; }
  udfspb::UDTFSourceSpec udtf_spec() const { return udtf_spec_; }

  Status CopyFromNodeImpl(const IRNode* source,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  const std::vector<DataIR*>& arg_values() const { return arg_values_; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return std::vector<absl::flat_hash_set<std::string>>{};
  }

  bool IsSource() const override { return true; }

  // Type is resolved on Init so no need to do anything here.
  Status ResolveType(CompilerState*) { return Status::OK(); }

 protected:
  // We currently don't support materializing a subset of UDTF output columns.
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    DCHECK(is_type_resolved());
    auto cols = resolved_table_type()->ColumnNames();
    return absl::flat_hash_set<std::string>(cols.begin(), cols.end());
  }

 private:
  /**
   * @brief Subroutine for SetArgValues that converts expression into a vector.
   *
   * @param expr
   * @return StatusOr<DataIR*> the data that the expr represents, or an error if it
   * doesn't fit a format.
   */
  StatusOr<DataIR*> ProcessArgValue(ExpressionIR* expr);

  std::string func_name_;
  std::vector<DataIR*> arg_values_;
  udfspb::UDTFSourceSpec udtf_spec_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
