#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler/optimizer/merge_nodes.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule pushes filters as early in the IR as possible.
 * It must run after OperatorRelationRule so that it has full context on all of the column
 * names that exist in the IR.
 *
 */
class FilterPushdownRule : public Rule {
 public:
  FilterPushdownRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  using ColumnNameMapping = absl::flat_hash_map<std::string, std::string>;
  OperatorIR* HandleAggPushdown(BlockingAggIR* map, ColumnNameMapping* column_name_mapping);
  OperatorIR* HandleMapPushdown(MapIR* map, ColumnNameMapping* column_name_mapping);
  OperatorIR* NextFilterLocation(OperatorIR* current_node, ColumnNameMapping* column_name_mapping);
  Status UpdateFilter(FilterIR* expr, const ColumnNameMapping& column_name_mapping);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
