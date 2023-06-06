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

#include "src/carnot/planner/ir/blocking_agg_ir.h"
#include "src/carnot/planner/ir/func_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

std::string BlockingAggIR::DebugString() const {
  return absl::Substitute(
      "$0(id=$1, groups=[$2], aggs={\n$3\n})", type_string(), id(),
      absl::StrJoin(groups(), ", ",
                    [](std::string* out, const ColumnIR* col) { *out = col->DebugString(); }),
      absl::StrJoin(aggregate_expressions_, ", ",
                    [](std::string* out, const ColumnExpression& expr) {
                      *out = absl::Substitute("$0=$1", expr.name, expr.node->DebugString());
                    }));
}

Status BlockingAggIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups,
                           const ColExpressionVector& agg_expr) {
  PX_RETURN_IF_ERROR(AddParent(parent));
  PX_RETURN_IF_ERROR(SetGroups(groups));
  return SetAggExprs(agg_expr);
}

Status BlockingAggIR::SetAggExprs(const ColExpressionVector& agg_exprs) {
  auto old_agg_expressions = aggregate_expressions_;
  for (const ColumnExpression& agg_expr : aggregate_expressions_) {
    ExpressionIR* expr = agg_expr.node;
    PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, expr));
  }
  aggregate_expressions_.clear();

  for (const auto& agg_expr : agg_exprs) {
    PX_ASSIGN_OR_RETURN(auto updated_expr, graph()->OptionallyCloneWithEdge(this, agg_expr.node));
    aggregate_expressions_.emplace_back(agg_expr.name, updated_expr);
  }

  for (const auto& old_agg_expr : old_agg_expressions) {
    PX_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_agg_expr.node->id()));
  }

  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> BlockingAggIR::RequiredInputColumns()
    const {
  absl::flat_hash_set<std::string> required;
  for (const auto& group : groups()) {
    required.insert(group->col_name());
  }
  for (const auto& agg_expr : aggregate_expressions_) {
    PX_ASSIGN_OR_RETURN(auto ret, agg_expr.node->InputColumnNames());
    required.insert(ret.begin(), ret.end());
  }
  return std::vector<absl::flat_hash_set<std::string>>{required};
}

StatusOr<absl::flat_hash_set<std::string>> BlockingAggIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  absl::flat_hash_set<std::string> kept_columns = output_colnames;

  ColExpressionVector new_aggs;
  for (const auto& expr : aggregate_expressions_) {
    if (output_colnames.contains(expr.name)) {
      new_aggs.push_back(expr);
    }
  }
  PX_RETURN_IF_ERROR(SetAggExprs(new_aggs));

  // We always need to keep the group columns based on the current specification of aggregate,
  // otherwise the result will change.
  for (const ColumnIR* group : groups()) {
    kept_columns.insert(group->col_name());
  }
  return kept_columns;
}

Status BlockingAggIR::EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                                  const ExpressionIR& ir_node) const {
  DCHECK(ir_node.type() == IRNodeType::kFunc);
  auto casted_ir = static_cast<const FuncIR&>(ir_node);
  expr->set_name(casted_ir.func_name());
  expr->set_id(casted_ir.func_id());
  for (types::DataType dt : casted_ir.registry_arg_types()) {
    expr->add_args_data_types(dt);
  }
  for (auto ir_arg : casted_ir.args()) {
    auto arg_pb = expr->add_args();
    if (ir_arg->IsColumn()) {
      PX_RETURN_IF_ERROR(static_cast<ColumnIR*>(ir_arg)->ToProto(arg_pb->mutable_column()));
    } else if (ir_arg->IsData()) {
      PX_RETURN_IF_ERROR(static_cast<DataIR*>(ir_arg)->ToProto(arg_pb->mutable_constant()));
    } else {
      return CreateIRNodeError("$0 is an invalid aggregate value", ir_arg->type_string());
    }
  }
  return Status::OK();
}

Status BlockingAggIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_agg_op();
  if (!partial_agg_) {
    (*pb->mutable_values()) = pre_split_proto_.values();
    (*pb->mutable_value_names()) = pre_split_proto_.value_names();
  } else {
    for (const auto& agg_expr : aggregate_expressions_) {
      auto expr = pb->add_values();
      PX_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *agg_expr.node));
      pb->add_value_names(agg_expr.name);
    }
  }
  for (ColumnIR* group : groups()) {
    auto group_pb = pb->add_groups();
    PX_RETURN_IF_ERROR(group->ToProto(group_pb));
    pb->add_group_names(group->col_name());
  }

  pb->set_windowed(false);
  pb->set_partial_agg(partial_agg_);
  pb->set_finalize_results(finalize_results_);

  op->set_op_type(planpb::AGGREGATE_OPERATOR);
  return Status::OK();
}

Status BlockingAggIR::CopyFromNodeImpl(
    const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const BlockingAggIR* blocking_agg = static_cast<const BlockingAggIR*>(node);

  ColExpressionVector new_agg_exprs;
  for (const ColumnExpression& col_expr : blocking_agg->aggregate_expressions_) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                        graph()->CopyNode(col_expr.node, copied_nodes_map));
    new_agg_exprs.push_back({col_expr.name, new_node});
  }

  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : blocking_agg->groups()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }

  PX_RETURN_IF_ERROR(SetAggExprs(new_agg_exprs));
  PX_RETURN_IF_ERROR(SetGroups(new_groups));

  finalize_results_ = blocking_agg->finalize_results_;
  partial_agg_ = blocking_agg->partial_agg_;
  pre_split_proto_ = blocking_agg->pre_split_proto_;

  return Status::OK();
}

Status BlockingAggIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(1U, parent_types().size());
  auto new_table = TableType::Create();
  for (const auto& group_col : groups()) {
    PX_RETURN_IF_ERROR(ResolveExpressionType(group_col, compiler_state, parent_types()));
    new_table->AddColumn(group_col->col_name(), group_col->resolved_type());
  }
  for (const auto& col_expr : aggregate_expressions_) {
    PX_RETURN_IF_ERROR(ResolveExpressionType(col_expr.node, compiler_state, parent_types()));
    new_table->AddColumn(col_expr.name, col_expr.node->resolved_type());
  }
  return SetResolvedType(new_table);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
