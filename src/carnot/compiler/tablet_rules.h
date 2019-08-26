#pragma once
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/compiler/compilerpb/physical_plan.pb.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

/**
 * @brief Expands memory sources to use tablets if they have tabletization keys
 *
 */
class TabletSourceConversionRule : public Rule {
 public:
  explicit TabletSourceConversionRule(const compilerpb::CarnotInfo& carnot_info)
      : Rule(nullptr), carnot_info_(carnot_info) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceMemorySourceWithTabletSourceGroup(MemorySourceIR* mem_source_ir);
  const compilerpb::TableInfo& GetTableInfo(const std::string& table_name);

  const compilerpb::CarnotInfo& carnot_info_;
};

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
