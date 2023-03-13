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

#include <rapidjson/document.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/coordinator/prune_unavailable_sources_rule.h"
#include "src/carnot/planner/distributed/coordinator/removable_ops_rule.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<OperatorToAgentSet> MapRemovableOperatorsRule::GetRemovableOperators(
    DistributedPlan* plan, const SchemaToAgentsMap& agent_schema_map,
    const absl::flat_hash_set<int64_t>& pem_instances, IR* query) {
  MapRemovableOperatorsRule rule(plan, pem_instances, agent_schema_map);
  PX_ASSIGN_OR_RETURN(auto did_remove, rule.Execute(query));
  DCHECK_EQ(did_remove, !rule.op_to_agent_set.empty());
  return rule.op_to_agent_set;
}

StatusOr<bool> MapRemovableOperatorsRule::Apply(IRNode* node) {
  if (Match(node, Filter())) {
    return CheckFilter(static_cast<FilterIR*>(node));
  }
  if (Match(node, MemorySource())) {
    return CheckMemorySource(static_cast<MemorySourceIR*>(node));
  }
  if (Match(node, UDTFSource())) {
    return CheckUDTFSource(static_cast<UDTFSourceIR*>(node));
  }
  return false;
}

StatusOr<AgentSet> FilterExpressionMatcher::AgentsPruned(ExpressionIR* expr,
                                                         const absl::flat_hash_set<int64_t>& agents,
                                                         DistributedPlan* plan) {
  AgentSet agents_that_remove_op;
  ParseExpression(expr);
  for (int64_t agent : agents) {
    auto carnot = plan->Get(agent);
    if (!carnot) {
      return error::InvalidArgument("Cannot find agent $0 in distributed plan", agent);
    }
    if (!CanAgentRun(carnot)) {
      agents_that_remove_op.agents.insert(agent);
    }
  }
  return agents_that_remove_op;
}

class ASIDMatcher : public FilterExpressionMatcher {
 public:
  static bool MatchExpr(ExpressionIR* expr) {
    return Match(expr, Equals(ASID(), Int())) && Match(expr, Equals(Func("asid"), Int()));
  }

  static std::unique_ptr<FilterExpressionMatcher> Create() {
    return std::make_unique<ASIDMatcher>();
  }

  bool CanAgentRun(CarnotInstance* carnot) const override {
    return carnot->carnot_info().asid() == asid_;
  }

  void ParseExpression(ExpressionIR* expr) override {
    auto func = static_cast<FuncIR*>(expr);
    int64_t arg_idx = 0;
    if (Match(func->args()[1], Int())) {
      arg_idx = 1;
    }
    asid_ = static_cast<IntIR*>(func->args()[arg_idx])->val();
  }

 private:
  int64_t asid_;
};

class StringBloomFilterMatcher : public FilterExpressionMatcher {
 public:
  static bool MatchExpr(ExpressionIR* expr) {
    auto string_equality = Match(expr, Equals(MetadataExpression(), String()));
    auto service_matcher = Match(expr, ServiceMatcher());
    return string_equality || service_matcher;
  }

  static std::unique_ptr<FilterExpressionMatcher> Create() {
    return std::make_unique<StringBloomFilterMatcher>();
  }

  bool CanAgentRun(CarnotInstance* carnot) const override {
    auto* md_filter = carnot->metadata_filter();
    if (md_filter == nullptr) {
      return false;
    }
    // The filter is kept if the metadata type is missing.
    if (!md_filter->metadata_types().contains(md_type_)) {
      return true;
    }

    // For cases like the following,
    // df.ctx['service'] == '["pl/svc1", "pl/svc2"]',
    // We would still like the expression to work.
    // However, the metadata filters will only store individual services,
    // not the JSON array. As a result, in the planner we will check for the
    // presence for either service in the Carnot instance when pruning the plan.

    rapidjson::Document doc;
    doc.Parse(val_.c_str());
    if (!doc.IsArray()) {
      return md_filter->ContainsEntity(md_type_, val_);
    }

    for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
      if (!doc[i].IsString()) {
        return false;
      }
      auto str = doc[i].GetString();
      if (md_filter->ContainsEntity(md_type_, str)) {
        return true;
      }
    }
    return false;
  }

  void ParseExpression(ExpressionIR* expr) override {
    auto func = static_cast<FuncIR*>(expr);
    int64_t val_idx = 0;
    int64_t md_idx = 1;
    if (Match(func->args()[1], String())) {
      val_idx = 1;
      md_idx = 0;
    }
    val_ = static_cast<StringIR*>(func->args()[val_idx])->str();
    md_type_ = static_cast<ExpressionIR*>(func->args()[md_idx])->annotations().metadata_type;
  }

 private:
  std::string val_;
  MetadataType md_type_;
};

MapRemovableOperatorsRule::MapRemovableOperatorsRule(
    DistributedPlan* plan, const absl::flat_hash_set<int64_t>& pem_instances,
    const SchemaToAgentsMap& schema_map)
    : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
      plan_(plan),
      pem_instances_(pem_instances),
      schema_map_(schema_map) {
  matchers_factory_.Add<ASIDMatcher>();
  matchers_factory_.Add<StringBloomFilterMatcher>();
}

StatusOr<AgentSet> MapRemovableOperatorsRule::FilterExpressionMayProduceData(ExpressionIR* expr) {
  // Check for nested expressions.
  auto logical_and = Match(expr, LogicalAnd(Value(), Value()));
  auto logical_or = Match(expr, LogicalOr(Value(), Value()));
  if (logical_and || logical_or) {
    FuncIR* func = static_cast<FuncIR*>(expr);
    PX_ASSIGN_OR_RETURN(auto lhs, FilterExpressionMayProduceData(func->args()[0]));
    PX_ASSIGN_OR_RETURN(auto rhs, FilterExpressionMayProduceData(func->args()[1]));
    // If the expression is AND, we union the agents we want to remove.
    // otherwise, we take the intersection of those agents.
    return logical_and ? lhs.Union(rhs) : lhs.Intersection(rhs);
  }

  // Check expressions that exist in the metadata factory.
  return matchers_factory_.AgentsPrunedByFilterExpression(expr, pem_instances_, plan_);
}

StatusOr<bool> MapRemovableOperatorsRule::CheckFilter(FilterIR* filter_ir) {
  PX_ASSIGN_OR_RETURN(AgentSet agents_that_remove_op,
                      FilterExpressionMayProduceData(filter_ir->filter_expr()));
  // If the filter appears on all agents, we don't wanna add it.
  if (agents_that_remove_op.agents.empty()) {
    return false;
  }
  op_to_agent_set[filter_ir] = std::move(agents_that_remove_op.agents);
  return true;
}

StatusOr<bool> MapRemovableOperatorsRule::CheckMemorySource(MemorySourceIR* mem_src_ir) {
  absl::flat_hash_set<int64_t> agent_ids;
  // Find the set difference of pem_instances and Agents that have the table.
  if (!schema_map_.contains(mem_src_ir->table_name())) {
    return mem_src_ir->CreateIRNodeError("Table '$0' not found in coordinator",
                                         mem_src_ir->table_name());
  }
  const auto& mem_src_ids = schema_map_.find(mem_src_ir->table_name())->second;
  for (const auto& pem : pem_instances_) {
    if (!mem_src_ids.contains(pem)) {
      agent_ids.insert(pem);
    }
  }
  if (agent_ids.empty()) {
    return false;
  }
  op_to_agent_set[mem_src_ir] = std::move(agent_ids);
  return true;
}

StatusOr<bool> MapRemovableOperatorsRule::CheckUDTFSource(UDTFSourceIR* udtf_ir) {
  const auto& spec = udtf_ir->udtf_spec();
  // Keep those that run on all agent or all pems.
  if (spec.executor() == udfspb::UDTF_ALL_AGENTS || spec.executor() == udfspb::UDTF_ALL_PEM) {
    return false;
  }
  // Remove the UDTF Sources that only appear on a Kelvin.
  if (spec.executor() == udfspb::UDTF_ALL_KELVIN || spec.executor() == udfspb::UDTF_ONE_KELVIN ||
      spec.executor() == udfspb::UDTF_SUBSET_KELVIN) {
    op_to_agent_set[udtf_ir] = pem_instances_;
    return true;
  }
  if (spec.executor() == udfspb::UDTF_SUBSET_PEM) {
    for (int64_t agent : pem_instances_) {
      if (!PruneUnavailableSourcesRule::UDTFMatchesFilters(udtf_ir,
                                                           plan_->Get(agent)->carnot_info())) {
        op_to_agent_set[udtf_ir].insert(agent);
      }
    }
    return op_to_agent_set[udtf_ir].size();
  }
  return false;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
