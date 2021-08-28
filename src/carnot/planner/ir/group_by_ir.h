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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class GroupByIR : public OperatorIR {
 public:
  GroupByIR() = delete;
  explicit GroupByIR(int64_t id) : OperatorIR(id, IRNodeType::kGroupBy) {}
  Status Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups);
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  std::vector<ColumnIR*> groups() const { return groups_; }

  // GroupBy does not exist as a protobuf object.
  Status ToProto(planpb::Operator*) const override {
    return error::Unimplemented("ToProto not implemented.");
  }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("Unexpected call to GroupByIR::RequiredInputColumns");
  }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to GroupByIR::PruneOutputColumnsTo.");
  }

 private:
  Status SetGroups(const std::vector<ColumnIR*>& groups);
  // contains group_names and groups columns.
  std::vector<ColumnIR*> groups_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
