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

#include "src/carnot/planner/distributed/tablet_rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<bool> TabletSourceConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MemorySource())) {
    return ReplaceMemorySourceWithTabletSourceGroup(static_cast<MemorySourceIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> TabletSourceConversionRule::ReplaceMemorySourceWithTabletSourceGroup(
    MemorySourceIR* mem_source_ir) {
  const std::string& table_name = mem_source_ir->table_name();

  // If a table doesn't have table info then don't do anything.
  const distributedpb::TableInfo* table_info = GetTableInfo(table_name);
  if (table_info == nullptr) {
    return false;
  }
  const std::string& tablet_key = table_info->tabletization_key();
  if (tablet_key.empty()) {
    return false;
  }

  // Get the tablet values out.
  std::vector<types::TabletID> tablets;
  for (int64_t i = 0; i < table_info->tablets_size(); ++i) {
    tablets.push_back(table_info->tablets()[i]);
  }

  IR* graph = mem_source_ir->graph();
  // Make the tablet source groups
  // Init with the memory source and tablets.
  PL_ASSIGN_OR_RETURN(TabletSourceGroupIR * tablet_source_group,
                      graph->CreateNode<TabletSourceGroupIR>(mem_source_ir->ast(), mem_source_ir,
                                                             tablets, tablet_key));

  // Replace the child's parent that originally pointed to memory source to point to
  // the new tablet source group.
  for (OperatorIR* child_op : mem_source_ir->Children()) {
    PL_RETURN_IF_ERROR(child_op->ReplaceParent(mem_source_ir, tablet_source_group));
  }
  return true;
}

const distributedpb::TableInfo* TabletSourceConversionRule::GetTableInfo(
    const std::string& table_name) {
  for (int64_t i = 0; i < carnot_info_.table_info_size(); ++i) {
    if (carnot_info_.table_info()[i].table() == table_name) {
      return &(carnot_info_.table_info()[i]);
    }
  }
  return nullptr;
}

StatusOr<bool> MemorySourceTabletRule::Apply(IRNode* ir_node) {
  // TODO(philkuz) this only matches the situation where there's one child of the tablet source
  // group.
  if (Match(ir_node, OperatorChain(TabletSourceGroup(), Filter()))) {
    return ReplaceTabletSourceGroupWithFilterChild(static_cast<TabletSourceGroupIR*>(ir_node));
  } else if (Match(ir_node, TabletSourceGroup())) {
    return ReplaceTabletSourceGroup(static_cast<TabletSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<OperatorIR*> MemorySourceTabletRule::MakeNewSources(
    const std::vector<types::TabletID>& tablets, TabletSourceGroupIR* tablet_source_group) {
  std::vector<OperatorIR*> sources;
  for (const auto& tablet : tablets) {
    PL_ASSIGN_OR_RETURN(MemorySourceIR * tablet_src,
                        CreateMemorySource(tablet_source_group->ReplacedMemorySource(), tablet));
    sources.push_back(tablet_src);
  }

  if (sources.size() == 0) {
    return tablet_source_group->CreateIRNodeError("No tablets matched for this query.");
  }
  // If there's only one operator, then just return that operator instead.
  if (sources.size() == 1) {
    return sources[0];
  }

  IR* graph = tablet_source_group->graph();
  PL_ASSIGN_OR_RETURN(UnionIR * union_op,
                      graph->CreateNode<UnionIR>(tablet_source_group->ast(), sources));
  PL_RETURN_IF_ERROR(union_op->SetRelationFromParents());
  DCHECK(union_op->HasColumnMappings());
  return union_op;
}

StatusOr<bool> MemorySourceTabletRule::ReplaceTabletSourceGroup(
    TabletSourceGroupIR* tablet_source_group) {
  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op,
                      MakeNewSources(tablet_source_group->tablets(), tablet_source_group));

  for (OperatorIR* child : tablet_source_group->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(tablet_source_group, parent_op));
  }
  DeleteNodeAndNonOperatorChildren(tablet_source_group->ReplacedMemorySource());
  DeleteNodeAndNonOperatorChildren(tablet_source_group);
  return true;
}

void MemorySourceTabletRule::DeleteNodeAndNonOperatorChildren(OperatorIR* op) {
  IR* graph = op->graph();
  const plan::DAG& dag = graph->dag();
  int64_t cur_node = op->id();
  std::queue<int64_t> q({cur_node});
  absl::flat_hash_set<int64_t> visited({cur_node});
  while (!q.empty()) {
    int64_t cur_val = q.front();
    q.pop();
    for (int64_t i : dag.DependenciesOf(cur_val)) {
      if (!graph->Get(i)->IsOperator() && !visited.contains(i)) {
        visited.insert(i);
        q.push(i);
      }
    }
    DeferNodeDeletion(cur_val);
  }
}

StatusOr<bool> MemorySourceTabletRule::ReplaceTabletSourceGroupAndFilter(
    TabletSourceGroupIR* tablet_source_group, FilterIR* filter_op,
    const absl::flat_hash_set<types::TabletID>& match_set) {
  DCHECK_EQ(tablet_source_group->Children().size(), 1UL);
  DCHECK_EQ(tablet_source_group->Children()[0]->DebugString(), filter_op->DebugString());
  std::vector<types::TabletID> matching_tablets;
  for (const auto& tablet : tablet_source_group->tablets()) {
    if (!match_set.empty() && !match_set.contains(tablet)) {
      continue;
    }
    matching_tablets.push_back(tablet);
  }
  if (matching_tablets.size() == 0) {
    return tablet_source_group->CreateIRNodeError(
        "Number of matching tablets must be greater than 0.");
  }

  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op,
                      MakeNewSources(matching_tablets, tablet_source_group));

  for (OperatorIR* child : filter_op->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(filter_op, parent_op));
  }
  DeleteNodeAndNonOperatorChildren(tablet_source_group->ReplacedMemorySource());
  DeleteNodeAndNonOperatorChildren(tablet_source_group);
  DeleteNodeAndNonOperatorChildren(filter_op);
  return true;
}

absl::flat_hash_set<types::TabletID> MemorySourceTabletRule::GetEqualityTabletValues(FuncIR* func) {
  types::TabletID value;
  DCHECK_EQ(func->args().size(), 2UL);
  if (Match(func->args()[1], ColumnNode())) {
    value = static_cast<StringIR*>(func->args()[0])->str();
  } else {
    value = static_cast<StringIR*>(func->args()[1])->str();
  }
  return {value};
}

absl::flat_hash_set<types::TabletID> MemorySourceTabletRule::GetAndTabletValues(FuncIR* func) {
  absl::flat_hash_set<types::TabletID> tablet_values;
  DCHECK(Match(func, AndFnMatchAll(Equals(ColumnNode(), TabletValue()))));

  for (ExpressionIR* expr : func->args()) {
    DCHECK(Match(expr, Func()));
    tablet_values.merge(GetEqualityTabletValues(static_cast<FuncIR*>(expr)));
  }
  return tablet_values;
}

StatusOr<bool> MemorySourceTabletRule::ReplaceTabletSourceGroupWithFilterChild(
    TabletSourceGroupIR* tablet_source_group) {
  DCHECK_EQ(tablet_source_group->Children().size(), 1UL);
  OperatorIR* child_op = tablet_source_group->Children()[0];

  DCHECK(Match(child_op, Filter()));
  FilterIR* filter = static_cast<FilterIR*>(child_op);
  ExpressionIR* expr = filter->filter_expr();

  DCHECK_EQ(expr->EvaluatedDataType(), types::BOOLEAN);
  std::string tablet_key = tablet_source_group->tablet_key();

  if (Match(expr, Equals(ColumnNode(tablet_key), TabletValue()))) {
    absl::flat_hash_set<types::TabletID> tablet_values =
        GetEqualityTabletValues(static_cast<FuncIR*>(expr));
    return ReplaceTabletSourceGroupAndFilter(tablet_source_group, filter, tablet_values);
  } else if (Match(expr, AndFnMatchAll(Equals(ColumnNode(tablet_key), TabletValue())))) {
    absl::flat_hash_set<types::TabletID> tablet_values =
        GetAndTabletValues(static_cast<FuncIR*>(expr));
    return ReplaceTabletSourceGroupAndFilter(tablet_source_group, filter, tablet_values);
  }

  // Otherwise, if the expression doesn't match then we should go through the originalpath
  return ReplaceTabletSourceGroup(tablet_source_group);
}

StatusOr<MemorySourceIR*> MemorySourceTabletRule::CreateMemorySource(
    const MemorySourceIR* original_memory_source, const types::TabletID& tablet_value) {
  DCHECK(original_memory_source->IsRelationInit());
  DCHECK(original_memory_source->column_index_map_set());
  IR* graph = original_memory_source->graph();
  // Create the Memory Source.
  PL_ASSIGN_OR_RETURN(MemorySourceIR * mem_source_ir,
                      graph->CreateNode<MemorySourceIR>(original_memory_source->ast(),
                                                        original_memory_source->table_name(),
                                                        std::vector<std::string>{}));
  // If HasTimeExpressions is true, then the time should be set.
  DCHECK(original_memory_source->IsTimeSet() || !original_memory_source->HasTimeExpressions());
  if (original_memory_source->IsTimeSet()) {
    mem_source_ir->SetTimeValuesNS(original_memory_source->time_start_ns(),
                                   original_memory_source->time_stop_ns());
  }
  PL_RETURN_IF_ERROR(mem_source_ir->SetRelation(original_memory_source->relation()));
  mem_source_ir->SetColumnIndexMap(original_memory_source->column_index_map());

  // Set the tablet value.
  mem_source_ir->SetTabletValue(tablet_value);
  return mem_source_ir;
}

StatusOr<bool> Tabletizer::Execute(const distributedpb::CarnotInfo& carnot_info, IR* ir_plan) {
  TabletSourceConversionRule rule1(carnot_info);
  MemorySourceTabletRule rule2;
  PL_ASSIGN_OR_RETURN(bool rule1_changed, rule1.Execute(ir_plan));
  PL_ASSIGN_OR_RETURN(bool rule2_changed, rule2.Execute(ir_plan));
  return rule1_changed || rule2_changed;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
