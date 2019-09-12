#pragma once
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

/**
 * @brief Expands memory sources to use tablets if they have tabletization keys
 *
 */
class TabletSourceConversionRule : public Rule {
 public:
  explicit TabletSourceConversionRule(const distributedpb::CarnotInfo& carnot_info)
      : Rule(nullptr), carnot_info_(carnot_info) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceMemorySourceWithTabletSourceGroup(MemorySourceIR* mem_source_ir);
  const distributedpb::TableInfo& GetTableInfo(const std::string& table_name);

  const distributedpb::CarnotInfo& carnot_info_;
};

/**
 * @brief Converts TabletSourceGroups into MemorySources with unions.
 */
class MemorySourceTabletRule : public Rule {
 public:
  MemorySourceTabletRule() : Rule(nullptr) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceTabletSourceGroup(TabletSourceGroupIR* tablet_source_group);
  StatusOr<bool> ReplaceTabletSourceGroupAndFilter(
      TabletSourceGroupIR* tablet_source_group, FilterIR* filter_op,
      const absl::flat_hash_set<TabletKeyType>& match_tablets);
  StatusOr<MemorySourceIR*> CreateMemorySource(const MemorySourceIR* original_memory_source,
                                               const TabletKeyType& tablet_value);

  StatusOr<bool> ReplaceTabletSourceGroupWithFilterChild(TabletSourceGroupIR* tablet_source_group);
  void DeleteNodeAndNonOperatorChildren(OperatorIR* op);
  StatusOr<OperatorIR*> MakeNewSources(const std::vector<std::string>& tablets,
                                       TabletSourceGroupIR* tablet_source_group);
  absl::flat_hash_set<TabletKeyType> GetEqualityTabletValues(FuncIR* func);
  absl::flat_hash_set<TabletKeyType> GetAndTabletValues(FuncIR* func);
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
