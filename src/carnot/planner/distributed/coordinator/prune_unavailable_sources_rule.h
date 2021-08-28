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

#include <string>

#include <absl/container/flat_hash_map.h>
#include <sole.hpp>

#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/ir/udtf_source_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using SchemaToAgentsMap = absl::flat_hash_map<std::string, absl::flat_hash_set<int64_t>>;

class PruneUnavailableSourcesRule : public Rule {
 public:
  PruneUnavailableSourcesRule(int64_t agent_id, const distributedpb::CarnotInfo& carnot_info,
                              const SchemaToAgentsMap& schema_map);
  StatusOr<bool> Apply(IRNode* node) override;

  static bool UDTFMatchesFilters(UDTFSourceIR* source,
                                 const distributedpb::CarnotInfo& carnot_info);

 private:
  StatusOr<bool> RemoveSourceIfNotNecessary(OperatorIR* node);
  StatusOr<bool> MaybePruneMemorySource(MemorySourceIR* mem_src);
  StatusOr<bool> MaybePruneUDTFSource(UDTFSourceIR* udtf_src);

  bool AgentExecutesUDTF(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);

  bool AgentSupportsMemorySources();
  bool AgentHasTable(std::string table_name);

  bool IsKelvin(const distributedpb::CarnotInfo& carnot_info);
  bool IsPEM(const distributedpb::CarnotInfo& carnot_info);

  int64_t agent_id_;
  const distributedpb::CarnotInfo& carnot_info_;
  const SchemaToAgentsMap& schema_map_;
};

/**
 * @brief This rule removes sources from the plan that don't run on a particular Carnot instance.
 * For example, some UDTFSources should only run on Kelvins or run on only some PEMs.
 *
 */
class DistributedPruneUnavailableSourcesRule : public DistributedRule {
 public:
  explicit DistributedPruneUnavailableSourcesRule(const SchemaToAgentsMap& schema_map)
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        schema_map_(schema_map) {}

  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;

 protected:
  const SchemaToAgentsMap& schema_map_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
