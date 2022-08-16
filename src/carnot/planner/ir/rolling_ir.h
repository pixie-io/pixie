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
#include "src/carnot/planner/ir/group_acceptor_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class RollingIR : public GroupAcceptorIR {
 public:
  RollingIR() = delete;
  explicit RollingIR(int64_t id) : GroupAcceptorIR(id, IRNodeType::kRolling) {}
  Status Init(OperatorIR* parent, ColumnIR* window_col, int64_t window_size);

  Status ToProto(planpb::Operator*) const override;
  ColumnIR* window_col() const { return window_col_; }
  int64_t window_size() const { return window_size_; }

  Status CopyFromNodeImpl(const IRNode* source,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& kept_columns) override;

 private:
  Status SetWindowCol(ColumnIR* window_col);

  ColumnIR* window_col_;
  int64_t window_size_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
