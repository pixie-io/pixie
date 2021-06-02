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

#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class SourceRelationRule : public Rule {
 public:
  explicit SourceRelationRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  StatusOr<bool> Apply(IRNode* ir_node) override;

 private:
  StatusOr<bool> GetSourceRelation(OperatorIR* source_op) const;
  StatusOr<std::vector<ColumnIR*>> GetColumnsFromRelation(
      OperatorIR* node, std::vector<std::string> col_names,
      const table_store::schema::Relation& relation) const;
  std::vector<int64_t> GetColumnIndexMap(const std::vector<std::string>& col_names,
                                         const table_store::schema::Relation& relation) const;
  StatusOr<table_store::schema::Relation> GetSelectRelation(
      IRNode* node, const table_store::schema::Relation& relation,
      const std::vector<std::string>& columns) const;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
