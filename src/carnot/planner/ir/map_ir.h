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
#include "src/carnot/planner/ir/column_expression.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The MapIR is a container for Map operators.
 * Describes a projection, which is describe in col_exprs().
 */
class MapIR : public OperatorIR {
 public:
  MapIR() = delete;
  explicit MapIR(int64_t id) : OperatorIR(id, IRNodeType::kMap) {}

  Status Init(OperatorIR* parent, const ColExpressionVector& col_exprs, bool keep_input_columns);

  const ColExpressionVector& col_exprs() const { return col_exprs_; }
  Status SetColExprs(const ColExpressionVector& exprs);
  Status AddColExpr(const ColumnExpression& expr);
  Status UpdateColExpr(std::string_view name, ExpressionIR* expr);
  Status UpdateColExpr(ExpressionIR* old_expr, ExpressionIR* new_expr);
  Status ToProto(planpb::Operator*) const override;
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  bool keep_input_columns() const { return keep_input_columns_; }
  void set_keep_input_columns(bool keep_input_columns) { keep_input_columns_ = keep_input_columns; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  Status ResolveType(CompilerState* compiler_state);

  std::string DebugString() const override {
    return absl::Substitute("Map(id=$0, $1)", id(), DebugStringExprs());
  }

  std::string DebugStringExprs() const {
    std::vector<std::string> expr_strings;
    for (const auto& expr : col_exprs_) {
      expr_strings.push_back(absl::Substitute("$0=$1", expr.name, expr.node->DebugString()));
    }
    return absl::StrJoin(expr_strings, ",");
  }

  Status UpdateOpAfterParentTypesResolvedImpl() override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) override;

 private:
  // The map from new column_names to expressions.
  ColExpressionVector col_exprs_;
  bool keep_input_columns_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
