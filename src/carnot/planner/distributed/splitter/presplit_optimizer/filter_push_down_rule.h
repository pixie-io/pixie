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

#include "src/carnot/planner/ir/blocking_agg_ir.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief This rule pushes filters as early in the IR as possible.
 * It must run after OperatorRelationRule so that it has full context on all of the column
 * names that exist in the IR.
 *
 */
class FilterPushdownRule : public Rule {
 public:
  explicit FilterPushdownRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  using ColumnNameMapping = absl::flat_hash_map<std::string, std::string>;
  OperatorIR* HandleAggPushdown(BlockingAggIR* map, ColumnNameMapping* column_name_mapping);
  OperatorIR* HandleMapPushdown(MapIR* map, ColumnNameMapping* column_name_mapping);
  StatusOr<OperatorIR*> NextFilterLocation(OperatorIR* current_node, bool kelvin_only_filter,
                                           ColumnNameMapping* column_name_mapping);
  Status UpdateFilter(FilterIR* expr, const ColumnNameMapping& column_name_mapping);
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
