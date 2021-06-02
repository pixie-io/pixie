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

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class DataTypeRule : public Rule {
  /**
   * @brief DataTypeRule evaluates the datatypes of any expressions that
   * don't have predetermined types.
   *
   * Currently resolves non-compile-time functions and column data types.
   */

 public:
  explicit DataTypeRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  /**
   * @brief Evaluates the datatype of column from the type data in the relation. Errors out if the
   * column doesn't exist in the relation.
   *
   * @param column the column to evaluate
   * @param relation the relation that should be used to evaluate the datatype of the column.
   * @return Status - error if column not contained in relation
   */
  static Status EvaluateColumnFromRelation(ColumnIR* column,
                                           const table_store::schema::Relation& relation);
  /**
   * @brief Evaluates the datatype of an input func.
   */
  static StatusOr<bool> EvaluateFunc(CompilerState* compiler_state, FuncIR* func);
  /**
   * @brief Evaluates the datatype of an input column.
   */
  static StatusOr<bool> EvaluateColumn(ColumnIR* column);

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  static StatusOr<bool> EvaluateMetadata(MetadataIR* md);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
