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

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief Splits Filters or Maps containing both Kelvin-only and PEM-only UDFs
 * into a map containing PEM-only UDFs followed by a Filter/Map with the rest.
 * This rule prevents PEM-only UDFs and Kelvin-only UDFs from being scheduled on
 * the same operator.
 */
class SplitPEMAndKelvinOnlyUDFOperatorRule : public Rule {
 public:
  explicit SplitPEMAndKelvinOnlyUDFOperatorRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* node) override;

 private:
  StatusOr<absl::flat_hash_set<std::string>> OptionallyUpdateExpression(
      IRNode* expr_parent, ExpressionIR* expr, MapIR* pem_only_map,
      const absl::flat_hash_set<std::string>& used_column_names);
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
