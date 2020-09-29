#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/removable_ops_rule.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<OperatorToAgentSet> MapRemovableOperatorsRule::GetRemovableOperators(
    DistributedPlan* plan, const SchemaToAgentsMap& agent_schema_map,
    const absl::flat_hash_set<int64_t>& pem_instances, IR* query) {
  MapRemovableOperatorsRule rule(plan, pem_instances, agent_schema_map);
  PL_ASSIGN_OR_RETURN(auto did_remove, rule.Execute(query));
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

StatusOr<AgentSet> MapRemovableOperatorsRule::FilterExpressionMayProduceData(ExpressionIR* expr) {
  if (!Match(expr, Func())) {
    return AgentSet();
  }
  auto func = static_cast<FuncIR*>(expr);
  if (func->args().size() != 2) {
    return AgentSet();
  }

  auto logical_and = Match(expr, LogicalAnd(Value(), Value()));
  auto logical_or = Match(expr, LogicalOr(Value(), Value()));
  if (logical_and || logical_or) {
    PL_ASSIGN_OR_RETURN(auto lhs, FilterExpressionMayProduceData(func->args()[0]));
    PL_ASSIGN_OR_RETURN(auto rhs, FilterExpressionMayProduceData(func->args()[1]));
    // If the expression is AND, we union the agents we want to remove.
    // otherwise, we take the intersection of those agents.
    return logical_and ? lhs.Union(rhs) : lhs.Intersection(rhs);
  }

  // We only care about those expressions that match df.ctx['pod'] == 'pl/pod_name'.
  if (!Match(expr, Equals(MetadataExpression(), String()))) {
    return AgentSet();
  }
  ExpressionIR* metadata_expr;
  StringIR* value;

  if (Match(func->args()[0], String())) {
    value = static_cast<StringIR*>(func->args()[0]);
    metadata_expr = func->args()[1];
  } else {
    metadata_expr = func->args()[0];
    value = static_cast<StringIR*>(func->args()[1]);
  }

  auto metadata_type = metadata_expr->annotations().metadata_type;
  AgentSet agents_that_remove_op;
  for (int64_t pem : pem_instances_) {
    auto pem_carnot = plan_->Get(pem);
    if (!pem_carnot) {
      return error::InvalidArgument("Cannot find pem $0 in distributed plan", pem);
    }
    auto* md_filter = pem_carnot->metadata_filter();
    if (md_filter == nullptr) {
      agents_that_remove_op.agents.insert(pem);
      continue;
    }
    // The Filter is kept if the metadata type is missing.
    if (!md_filter->metadata_types().contains(metadata_type)) {
      continue;
    }
    // The Filter is removed if we don't contain the entity.
    if (!md_filter->ContainsEntity(metadata_type, value->str())) {
      agents_that_remove_op.agents.insert(pem);
    }
  }
  return agents_that_remove_op;
}

StatusOr<bool> MapRemovableOperatorsRule::CheckFilter(FilterIR* filter_ir) {
  PL_ASSIGN_OR_RETURN(AgentSet agents_that_remove_op,
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
}  // namespace pl
