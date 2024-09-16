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
#include <utility>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class ConvertMetadataRule : public Rule {
  /**
   * @brief Converts the MetadataIR into the expression that generates it.
   */
 public:
  explicit ConvertMetadataRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  /**
   * @brief Updates any parents of the metadata node to point to the new metadata expression.
   */
  Status UpdateMetadataContainer(IRNode* container, MetadataIR* metadata,
                                 ExpressionIR* metadata_expr) const;
  StatusOr<std::string> FindKeyColumn(std::shared_ptr<TableType> parent_type,
                                      MetadataProperty* property, IRNode* node_for_error) const;

  /**
   *
   * This function aids in applying a fallback to a metadata conversion expression.
   * It works by adding two conversion maps to the graph. The first map contains a column expression
   * that contains the metadata expression result. The second map contains a column expression that
   * contains the fallback expression result. It is intended to be used as a short term workaround
   * for gh#1638 where pod name lookups fail for short lived processes. This fallback expression
   * allows for an alternative mechanism if the primary lookup fails. See the example below:
   *
   * Before applying the rule:
   *
   * MemorySource -> MapIR (containing MetadataIR)
   *
   * After applying the rule:
   *
   * MemorySource
   *   -> MapIR (col_names[0]: metadata_expr)
   *     -> MapIR (col_names[1]: fallback_expr)
   *       -> MapIR (col_names[1] & existing cols)
   */

  Status AddPodNameConversionMapsWithFallback(
      IR* graph, IRNode* container, ExpressionIR* metadata_expr, ExpressionIR* fallback_expr,
      const std::pair<std::string, std::string>& col_names) const;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
