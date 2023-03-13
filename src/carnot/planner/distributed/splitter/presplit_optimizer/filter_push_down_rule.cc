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

#include <algorithm>
#include <queue>

#include "src/carnot/planner/distributed/splitter/executor_utils.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/filter_push_down_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

OperatorIR* FilterPushdownRule::HandleMapPushdown(MapIR* map,
                                                  ColumnNameMapping* column_name_mapping) {
  ColumnNameMapping reverse_column_name_mapping;
  for (const auto& [old_name, cur_name] : *column_name_mapping) {
    reverse_column_name_mapping[cur_name] = old_name;
  }

  // Don't actually update column_name_mapping until we know that this map is getting pushed down.
  ColumnNameMapping column_name_updates;

  for (const auto& col_expr : map->col_exprs()) {
    // Ignore columns that aren't relevant to the particular filter func.
    if (!reverse_column_name_mapping.contains(col_expr.name)) {
      continue;
    }
    // If an involved column is actually created here, not the result of a reassignment
    // or rename, then we must stop walking up the tree and place the filter below
    // this parent Map.
    if (!Match(col_expr.node, ColumnNode())) {
      return nullptr;
    }

    // Check to see if the column has been renamed.
    // Update our column name map if it has.
    ColumnIR* column = static_cast<ColumnIR*>(col_expr.node);
    if (col_expr.name != column->col_name()) {
      auto eventual_col_name = reverse_column_name_mapping.at(col_expr.name);
      column_name_updates.insert({eventual_col_name, column->col_name()});
    }
  }
  // If we didn't run into any of the above conditions, then we are save to move the
  // filter up above the current map. And we can update the column name mapping safely.
  for (const auto& [k, v] : column_name_updates) {
    (*column_name_mapping)[k] = v;
  }
  return map;
}

OperatorIR* FilterPushdownRule::HandleAggPushdown(BlockingAggIR* agg,
                                                  ColumnNameMapping* column_name_mapping) {
  ColumnNameMapping reverse_column_name_mapping;
  for (const auto& [old_name, cur_name] : *column_name_mapping) {
    reverse_column_name_mapping[cur_name] = old_name;
  }

  for (const auto& agg_expr : agg->aggregate_expressions()) {
    // If any of the filter columns come from the output of an aggregate expression,
    // don't push the filter up any further. For certain aggregate functions like min or max,
    // it is actually possible to push the aggregate up further. In order to support this,
    // we would need a way of identifying whether or not a given aggregate func outputs the
    // same type of values that it receives (max(foo) returns an instance of a "foo" data point,
    // whereas count(foo) does not) and only push the filter up with that type.
    if (reverse_column_name_mapping.contains(agg_expr.name)) {
      return nullptr;
    }
  }

  // If all of the filter columns come from the group by column in an agg, then we are
  // safe to push the filter above the agg.
  return agg;
}

// Currently only supports single-parent, single-child operators.
StatusOr<OperatorIR*> FilterPushdownRule::NextFilterLocation(
    OperatorIR* current_node, bool filter_has_kelvin_only_udf,
    ColumnNameMapping* column_name_mapping) {
  if (current_node->parents().size() != 1) {
    return nullptr;
  }

  OperatorIR* parent = current_node->parents()[0];
  if (parent->Children().size() > 1) {
    return nullptr;
  }

  PX_ASSIGN_OR_RETURN(
      auto parent_has_pem_only_udf,
      HasFuncWithExecutor(compiler_state_, parent, udfspb::UDFSourceExecutor::UDF_PEM));

  // If the filter has a Kelvin-only UDF, and the operator we are looking at
  // has a PEM-only UDF, then it isn't safe to raise the filter anymore because
  // PEM-only UDFs need to be scheduled before Kelvin-only UDFs in the plan.
  if (parent_has_pem_only_udf && filter_has_kelvin_only_udf) {
    return nullptr;
  }
  if (Match(parent, Filter()) || Match(parent, Limit())) {
    return parent;
  }
  if (Match(parent, BlockingAgg())) {
    return HandleAggPushdown(static_cast<BlockingAggIR*>(parent), column_name_mapping);
  }
  if (Match(parent, Map())) {
    return HandleMapPushdown(static_cast<MapIR*>(parent), column_name_mapping);
  }
  return nullptr;
}

Status FilterPushdownRule::UpdateFilter(FilterIR* filter,
                                        const ColumnNameMapping& column_name_mapping) {
  PX_ASSIGN_OR_RETURN(ExpressionIR * new_expr, filter->graph()->CopyNode(filter->filter_expr()));
  PX_ASSIGN_OR_RETURN(auto filter_input_cols, new_expr->InputColumns());
  for (ColumnIR* col : filter_input_cols) {
    auto new_col_name = column_name_mapping.at(col->col_name());
    col->UpdateColumnName(new_col_name);
  }
  return filter->SetFilterExpr(new_expr);
}

StatusOr<bool> FilterPushdownRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Filter())) {
    return false;
  }

  FilterIR* filter = static_cast<FilterIR*>(ir_node);
  OperatorIR* current_node = filter;

  // Tracks the name of each involved column we have in this filter func,
  // as of the current position of current_node.
  absl::flat_hash_map<std::string, std::string> column_name_mapping;
  PX_ASSIGN_OR_RETURN(auto involved_cols, filter->filter_expr()->InputColumnNames());
  for (const auto& col : involved_cols) {
    column_name_mapping[col] = col;
  }

  PX_ASSIGN_OR_RETURN(
      auto kelvin_only_filter,
      HasFuncWithExecutor(compiler_state_, filter, udfspb::UDFSourceExecutor::UDF_KELVIN));

  // Iterate up from the current node, stopping when we reach the earliest allowable
  // new location for the filter node.
  while (true) {
    PX_ASSIGN_OR_RETURN(OperatorIR * next_parent,
                        NextFilterLocation(current_node, kelvin_only_filter, &column_name_mapping));
    if (next_parent == nullptr) {
      break;
    }
    current_node = next_parent;
  }
  // If the current_node is filter, that means we could not find a better filter location and will
  // not change.
  if (current_node == filter) {
    return false;
  }

  PX_RETURN_IF_ERROR(UpdateFilter(filter, column_name_mapping));

  // Make the filter's parent its children's new parent.
  DCHECK_EQ(1U, filter->parents().size());
  OperatorIR* filter_parent = filter->parents()[0];

  for (OperatorIR* child : filter->Children()) {
    PX_RETURN_IF_ERROR(child->ReplaceParent(filter, filter_parent));
  }
  PX_RETURN_IF_ERROR(filter->RemoveParent(filter_parent));

  DCHECK_EQ(1U, current_node->parents().size());
  auto new_filter_parent = current_node->parents()[0];
  PX_RETURN_IF_ERROR(filter->AddParent(new_filter_parent));
  PX_RETURN_IF_ERROR(current_node->ReplaceParent(new_filter_parent, filter));
  PX_RETURN_IF_ERROR(filter->SetResolvedType(new_filter_parent->resolved_type()));
  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
