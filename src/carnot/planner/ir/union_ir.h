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
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

// [Column("foo", 0)] would indicate the foo column of the 0th parent relation maps to index 0 of
// the output relation.
using InputColumnMapping = std::vector<ColumnIR*>;

class UnionIR : public OperatorIR {
 public:
  UnionIR() = delete;
  explicit UnionIR(int64_t id) : OperatorIR(id, IRNodeType::kUnion) {}

  bool IsBlocking() const override { return true; }

  Status ToProto(planpb::Operator*) const override;
  Status Init(const std::vector<OperatorIR*>& parents);
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  /**
   * @brief Sets the column mapping to default no mapping. Used to save effort in creating Relation
   * from parents and input column mapping in the case where the parents are known to have the same
   * relation.
   *
   * This is part of an optimization to support huge unions of GRPCSources that would otherwise be
   * extremely costly.
   *
   * @return Status
   */
  Status SetDefaultColumnMapping();

  bool HasColumnMappings() const { return column_mappings_.size() == parents().size(); }
  Status SetColumnMappings(const std::vector<InputColumnMapping>& mappings);
  const std::vector<InputColumnMapping>& column_mappings() const { return column_mappings_; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  Status ResolveType(CompilerState* compiler_state);
  Status UpdateOpAfterParentTypesResolvedImpl() override;

  bool default_column_mapping() const { return default_column_mapping_; }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& kept_columns) override;

 private:
  /**
   * @brief Set a parent operator's column mapping.
   *
   * @param parent_idx: the parents() idx this mapping applies to.
   * @param input_column_map: the mapping from 1 columns unions to another.
   * @return Status
   */
  Status AddColumnMapping(const InputColumnMapping& input_column_map);
  // The vector is size N when there are N input tables.
  std::vector<InputColumnMapping> column_mappings_;
  // Whether we should just assume the parents have the same column_mappings.
  bool default_column_mapping_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
