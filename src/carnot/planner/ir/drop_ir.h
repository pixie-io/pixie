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
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The DropIR is a container for Drop column operators.
 * It eventually compiles down to a Map node.
 */
class DropIR : public OperatorIR {
 public:
  DropIR() = delete;
  explicit DropIR(int64_t id) : OperatorIR(id, IRNodeType::kDrop) {}
  Status Init(OperatorIR* parent, const std::vector<std::string>& drop_cols);

  Status ToProto(planpb::Operator*) const override;

  const std::vector<std::string>& col_names() const { return col_names_; }

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("Unexpected call to DropIR::RequiredInputColumns");
  }

  Status ResolveType(CompilerState* compiler_state);

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to DropIR::PruneOutputColumnsTo.");
  }

 private:
  // Names of the columns to drop.
  std::vector<std::string> col_names_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
