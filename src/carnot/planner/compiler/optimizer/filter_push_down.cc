#include "src/carnot/planner/compiler/optimizer/filter_push_down.h"

#include <algorithm>
#include <queue>

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

OperatorIR* FilterPushdownRule::HandleMapPushdown(MapIR* map,
                                                  ColumnNameMapping* column_name_mapping) {
  ColumnNameMapping reverse_column_name_mapping;
  for (const auto& [old_name, cur_name] : *column_name_mapping) {
    reverse_column_name_mapping[cur_name] = old_name;
  }

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
      (*column_name_mapping)[eventual_col_name] = column->col_name();
    }
  }
  // If we didn't run into any of the above conditions, then we are save to move the
  // filter up above the current map.
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
OperatorIR* FilterPushdownRule::NextFilterLocation(OperatorIR* current_node,
                                                   ColumnNameMapping* column_name_mapping) {
  if (current_node->parents().size() != 1) {
    return nullptr;
  }

  OperatorIR* parent = current_node->parents()[0];
  if (parent->Children().size() > 1) {
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
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_expr, filter->graph()->CopyNode(filter->filter_expr()));
  PL_ASSIGN_OR_RETURN(auto filter_input_cols, new_expr->InputColumns());
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
  PL_ASSIGN_OR_RETURN(auto involved_cols, filter->filter_expr()->InputColumnNames());
  if (!involved_cols.size()) {
    return false;
  }
  for (const auto& col : involved_cols) {
    column_name_mapping[col] = col;
  }

  // Iterate up from the current node, stopping when we reach the earliest allowable
  // new location for the filter node.
  while (OperatorIR* next_parent = NextFilterLocation(current_node, &column_name_mapping)) {
    current_node = next_parent;
  }
  // If the current_node is filter, that means we could not find a better filter location and will
  // not change.
  if (current_node == filter) {
    return false;
  }

  PL_RETURN_IF_ERROR(UpdateFilter(filter, column_name_mapping));

  // Make the filter's parent its children's new parent.
  DCHECK_EQ(1, filter->parents().size());
  OperatorIR* filter_parent = filter->parents()[0];

  for (OperatorIR* child : filter->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(filter, filter_parent));
  }
  PL_RETURN_IF_ERROR(filter->RemoveParent(filter_parent));

  DCHECK_EQ(1, current_node->parents().size());
  auto new_filter_parent = current_node->parents()[0];
  PL_RETURN_IF_ERROR(filter->AddParent(new_filter_parent));
  PL_RETURN_IF_ERROR(current_node->ReplaceParent(new_filter_parent, filter));
  PL_RETURN_IF_ERROR(filter->SetRelation(new_filter_parent->relation()));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
