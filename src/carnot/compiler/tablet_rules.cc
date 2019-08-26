#include "src/carnot/compiler/tablet_rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

StatusOr<bool> TabletSourceConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MemorySource())) {
    return ReplaceMemorySourceWithTabletSourceGroup(static_cast<MemorySourceIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> TabletSourceConversionRule::ReplaceMemorySourceWithTabletSourceGroup(
    MemorySourceIR* mem_source_ir) {
  const std::string& table_name = mem_source_ir->table_name();
  const compilerpb::TableInfo& table_info = GetTableInfo(table_name);
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

const compilerpb::TableInfo& TabletSourceConversionRule::GetTableInfo(
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
  // TODO(philkuz) create a rule to match a filter group.
  // if (Match(ir_node, OperationChain(TabletSourceGroup(), Filter()))) {
  // } else
  if (Match(ir_node, TabletSourceGroup())) {
    return ReplaceTabletSourceGroup(static_cast<TabletSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<OperatorIR*> MakeNewSources(const std::vector<OperatorIR*>& operators,
                                     TabletSourceGroupIR* tablet_source_group) {
  DCHECK_GE(operators.size(), 1UL);

  // If there's only one operator, then just return that operator instead.
  if (operators.size() == 1) {
    return operators[0];
  }

  IR* graph = tablet_source_group->graph_ptr();
  PL_ASSIGN_OR_RETURN(UnionIR * union_op, graph->MakeNode<UnionIR>());
  PL_RETURN_IF_ERROR(union_op->Init(operators, {{}}, tablet_source_group->ast_node()));
  PL_RETURN_IF_ERROR(union_op->SetRelationFromParents());
  return union_op;
}

StatusOr<bool> MemorySourceTabletRule::ReplaceTabletSourceGroup(
    TabletSourceGroupIR* tablet_source_group) {
  std::vector<OperatorIR*> sources;
  for (const auto& tablet : tablet_source_group->tablets()) {
    PL_ASSIGN_OR_RETURN(MemorySourceIR * tablet_src,
                        CreateMemorySource(tablet_source_group->ReplacedMemorySource(), tablet));
    sources.push_back(tablet_src);
  }

  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op, MakeNewSources(sources, tablet_source_group));

  for (OperatorIR* child : tablet_source_group->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(tablet_source_group, parent_op));
  }
  DeferNodeDeletion(tablet_source_group->ReplacedMemorySource()->id());
  DeferNodeDeletion(tablet_source_group->id());
  return true;
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
  mem_source_ir->SetTime(original_memory_source->time_start_ns(),
                         original_memory_source->time_stop_ns());
  mem_source_ir->SetColumnIndexMap(original_memory_source->column_index_map());

  // Set the tablet value.
  mem_source_ir->SetTablet(tablet_value);
  return mem_source_ir;
}

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
