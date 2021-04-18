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

#pragma once
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief Expands memory sources to use tablets if they have tabletization keys
 *
 */
class TabletSourceConversionRule : public Rule {
 public:
  explicit TabletSourceConversionRule(const distributedpb::CarnotInfo& carnot_info)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        carnot_info_(carnot_info) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceMemorySourceWithTabletSourceGroup(MemorySourceIR* mem_source_ir);
  const distributedpb::TableInfo* GetTableInfo(const std::string& table_name);

  distributedpb::CarnotInfo carnot_info_;
};

/**
 * @brief Converts TabletSourceGroups into MemorySources with unions.
 */
class MemorySourceTabletRule : public Rule {
 public:
  MemorySourceTabletRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 private:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ReplaceTabletSourceGroup(TabletSourceGroupIR* tablet_source_group);
  StatusOr<bool> ReplaceTabletSourceGroupAndFilter(
      TabletSourceGroupIR* tablet_source_group, FilterIR* filter_op,
      const absl::flat_hash_set<types::TabletID>& match_tablets);
  StatusOr<MemorySourceIR*> CreateMemorySource(const MemorySourceIR* original_memory_source,
                                               const types::TabletID& tablet_value);

  StatusOr<bool> ReplaceTabletSourceGroupWithFilterChild(TabletSourceGroupIR* tablet_source_group);
  void DeleteNodeAndNonOperatorChildren(OperatorIR* op);
  StatusOr<OperatorIR*> MakeNewSources(const std::vector<types::TabletID>& tablets,
                                       TabletSourceGroupIR* tablet_source_group);
  /**
   * @brief Get the tablet keys that match an equality condition.
   *
   * @param func: the ir for a function that contains an equality condition where one argument is a
   * tablet value.
   * @return absl::flat_hash_set<types::TabletID>: the set of tablet values that appear in this
   * function.
   */
  absl::flat_hash_set<types::TabletID> GetEqualityTabletValues(FuncIR* func);

  /**
   * @brief Get the tablet values that match a series of equality conditions combined with AND
   * TODO(philkuz) this should be OR not And.
   *
   * @param func: the
   * @return absl::flat_hash_set<TabletKeyType>: the set of tablet values that appear in this
   * function.
   */
  absl::flat_hash_set<types::TabletID> GetAndTabletValues(FuncIR* func);
};

class Tabletizer {
 public:
  static StatusOr<bool> Execute(const distributedpb::CarnotInfo& carnot_info, IR* ir_plan);
};

class DistributedTabletizerRule : public DistributedRule {
 public:
  DistributedTabletizerRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override {
    TabletSourceConversionRule rule1(node->carnot_info());
    MemorySourceTabletRule rule2;
    PL_ASSIGN_OR_RETURN(bool rule1_changed, rule1.Execute(node->plan()));
    PL_ASSIGN_OR_RETURN(bool rule2_changed, rule2.Execute(node->plan()));
    return rule1_changed || rule2_changed;
  }
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
