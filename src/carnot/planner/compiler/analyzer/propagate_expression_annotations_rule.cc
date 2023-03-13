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

#include "src/carnot/planner/compiler/analyzer/propagate_expression_annotations_rule.h"
#include "src/carnot/planner/ir/map_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> PropagateExpressionAnnotationsRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Operator())) {
    return false;
  }
  auto op = static_cast<OperatorIR*>(ir_node);
  operator_output_annotations_[op] = {};

  bool updated_annotation = false;
  absl::flat_hash_set<ColumnIR*> dependent_columns;
  for (int64_t dependency_id : ir_node->graph()->dag().DependenciesOf(ir_node->id())) {
    auto node = ir_node->graph()->Get(dependency_id);
    if (Match(node, Expression())) {
      PX_ASSIGN_OR_RETURN(auto expr_columns, static_cast<ExpressionIR*>(node)->InputColumns());
      dependent_columns.insert(expr_columns.begin(), expr_columns.end());
    }
  }

  // For all of the input columns that this operator uses, fetch the annotations
  // that their referenced Operator knows about for each of them and add it to these instances.
  for (ColumnIR* col : dependent_columns) {
    PX_ASSIGN_OR_RETURN(auto referenced_op, col->ReferencedOperator());
    const auto& parent_annotations = operator_output_annotations_.at(referenced_op);
    if (!parent_annotations.contains(col->col_name())) {
      continue;
    }
    auto new_annotation = ExpressionIR::Annotations::Union(parent_annotations.at(col->col_name()),
                                                           col->annotations());
    if (new_annotation != col->annotations()) {
      updated_annotation = true;
      col->set_annotations(new_annotation);
    }
  }

  // Now set the annotations for this operator for each of its output columns, to be used
  // by child operators as input to computing its columns' annotations.
  if (Match(op, Join())) {
    auto join = static_cast<JoinIR*>(op);
    for (const auto& [output_col_idx, out_col_name] : Enumerate(join->column_names())) {
      operator_output_annotations_[op][out_col_name] =
          join->output_columns()[output_col_idx]->annotations();
    }
  } else if (Match(op, Union())) {
    // For each of the union output columns, compute the annotation that is comprised of
    // all of the fields that every parent shares.
    auto out_col_names = op->resolved_table_type()->ColumnNames();
    for (const auto& out_col_name : out_col_names) {
      ExpressionIR::Annotations shared_annotation;
      // Note: assumes that unioning is done by column name with no renaming of columns.
      // supported. (same assumption as elsewhere)
      for (const auto& [parent_idx, parent] : Enumerate(op->parents())) {
        const auto& parent_annotations = operator_output_annotations_.at(parent);
        if (!parent_annotations.contains(out_col_name)) {
          continue;
        }
        auto input_annotation = parent_annotations.at(out_col_name);
        if (parent_idx == 0) {
          shared_annotation = input_annotation;
        } else {
          shared_annotation =
              ExpressionIR::Annotations::Intersection(shared_annotation, input_annotation);
        }
      }
      operator_output_annotations_[op][out_col_name] = shared_annotation;
    }
  } else if (Match(op, Map())) {
    for (const auto& col_expr : static_cast<MapIR*>(op)->col_exprs()) {
      operator_output_annotations_[op][col_expr.name] = col_expr.node->annotations();
    }
  } else if (Match(op, BlockingAgg())) {
    auto agg = static_cast<BlockingAggIR*>(op);
    for (const ColumnIR* group : agg->groups()) {
      operator_output_annotations_[op][group->col_name()] = group->annotations();
    }
    for (const ColumnExpression& expr : agg->aggregate_expressions()) {
      operator_output_annotations_[op][expr.name] = expr.node->annotations();
    }
  } else if (Match(op, Filter()) || Match(op, Limit())) {
    DCHECK_EQ(1U, op->parents().size());
    operator_output_annotations_[op] = operator_output_annotations_.at(op->parents()[0]);
  }

  return updated_annotation;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
