#include "src/carnot/compiler/tablet_rules.h"

namespace pl {
namespace carnot {
namespace compiler {
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
  const distributedpb::TableInfo& table_info = GetTableInfo(table_name);
  const std::string& tablet_key = table_info.tabletization_key();
  if (tablet_key.empty()) {
    return false;
  }

  // Get the tablet values out.
  std::vector<TabletKeyType> tablets;
  for (int64_t i = 0; i < table_info.tablets_size(); ++i) {
    tablets.push_back(table_info.tablets()[i]);
  }

  // Make the tablet source groups
  IR* graph = mem_source_ir->graph_ptr();
  PL_ASSIGN_OR_RETURN(TabletSourceGroupIR * tablet_source_group,
                      graph->MakeNode<TabletSourceGroupIR>());

  // Init with the memory source and tablets.
  PL_RETURN_IF_ERROR(tablet_source_group->Init(mem_source_ir, tablets, tablet_key));

  // Replace the child's parent that originally pointed to memory source to point to
  // the new tablet source group.
  for (OperatorIR* child_op : mem_source_ir->Children()) {
    PL_RETURN_IF_ERROR(child_op->ReplaceParent(mem_source_ir, tablet_source_group));
  }
  return true;
}

const distributedpb::TableInfo& TabletSourceConversionRule::GetTableInfo(
    const std::string& table_name) {
  DCHECK_GT(carnot_info_.table_info_size(), 0);
  for (int64_t i = 0; i < carnot_info_.table_info_size(); ++i) {
    if (carnot_info_.table_info()[i].table() == table_name) {
      return carnot_info_.table_info()[i];
    }
  }
  CHECK(false) << "Couldn't find table: " << table_name;
  // for(int64_t)
  return carnot_info_.table_info()[0];
}

StatusOr<bool> MemorySourceTabletRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, OperatorChain(TabletSourceGroup(), Filter()))) {
    return ReplaceTabletSourceGroupWithFilterChild(static_cast<TabletSourceGroupIR*>(ir_node));
  } else if (Match(ir_node, TabletSourceGroup())) {
    return ReplaceTabletSourceGroup(static_cast<TabletSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<OperatorIR*> MemorySourceTabletRule::MakeNewSources(
    const std::vector<std::string>& tablets, TabletSourceGroupIR* tablet_source_group) {
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

  IR* graph = tablet_source_group->graph_ptr();
  PL_ASSIGN_OR_RETURN(UnionIR * union_op, graph->MakeNode<UnionIR>());
  PL_RETURN_IF_ERROR(union_op->Init(sources, {{}}, tablet_source_group->ast_node()));
  PL_RETURN_IF_ERROR(union_op->SetRelationFromParents());
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
  IR* graph = op->graph_ptr();
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
    const absl::flat_hash_set<TabletKeyType>& match_set) {
  DCHECK_EQ(tablet_source_group->Children().size(), 1UL);
  DCHECK_EQ(tablet_source_group->Children()[0]->DebugString(), filter_op->DebugString());
  std::vector<std::string> matching_tablets;
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

absl::flat_hash_set<TabletKeyType> MemorySourceTabletRule::GetEqualityTabletValues(FuncIR* func) {
  std::string value;
  DCHECK_EQ(func->args().size(), 2UL);
  if (Match(func->args()[1], ColumnNode())) {
    value = static_cast<StringIR*>(func->args()[0])->str();
  } else {
    value = static_cast<StringIR*>(func->args()[1])->str();
  }
  return {value};
}

absl::flat_hash_set<TabletKeyType> MemorySourceTabletRule::GetAndTabletValues(FuncIR* func) {
  DCHECK(Match(func, AndFnMatchAll(Equals(ColumnNode(), String()))));
  absl::flat_hash_set<TabletKeyType> tablet_values;
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

  if (Match(expr, Equals(ColumnNode(tablet_key), String()))) {
    absl::flat_hash_set<TabletKeyType> tablet_values =
        GetEqualityTabletValues(static_cast<FuncIR*>(expr));
    return ReplaceTabletSourceGroupAndFilter(tablet_source_group, filter, tablet_values);
  } else if (Match(expr, AndFnMatchAll(Equals(ColumnNode(tablet_key), String())))) {
    absl::flat_hash_set<TabletKeyType> tablet_values =
        GetAndTabletValues(static_cast<FuncIR*>(expr));
    return ReplaceTabletSourceGroupAndFilter(tablet_source_group, filter, tablet_values);
  }

  // Otherwise, if the expression doesn't match then we should go through the originalpath
  return ReplaceTabletSourceGroup(tablet_source_group);
}

StatusOr<MemorySourceIR*> MemorySourceTabletRule::CreateMemorySource(
    const MemorySourceIR* original_memory_source, const TabletKeyType& tablet_value) {
  DCHECK(original_memory_source->IsRelationInit());
  DCHECK(original_memory_source->column_index_map_set());
  IR* graph = original_memory_source->graph_ptr();
  // Create the Memory Source.
  PL_ASSIGN_OR_RETURN(MemorySourceIR * mem_source_ir, graph->MakeNode<MemorySourceIR>());
  PL_ASSIGN_OR_RETURN(StringIR * table_node, graph->MakeNode<StringIR>());
  PL_RETURN_IF_ERROR(
      table_node->Init(original_memory_source->table_name(), original_memory_source->ast_node()));
  PL_RETURN_IF_ERROR(mem_source_ir->Init(nullptr, {{"table", table_node}, {"select", nullptr}},
                                         original_memory_source->ast_node()));
  PL_RETURN_IF_ERROR(mem_source_ir->SetRelation(original_memory_source->relation()));
  if (mem_source_ir->IsTimeSet()) {
    mem_source_ir->SetTime(original_memory_source->time_start_ns(),
                           original_memory_source->time_stop_ns());
  }
  mem_source_ir->SetColumnIndexMap(original_memory_source->column_index_map());

  // Set the tablet value.
  mem_source_ir->SetTablet(tablet_value);
  return mem_source_ir;
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
