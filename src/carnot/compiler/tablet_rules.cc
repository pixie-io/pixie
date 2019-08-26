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

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
