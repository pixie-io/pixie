#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_splitter.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<std::unique_ptr<Coordinator>> Coordinator::Create(
    const distributedpb::DistributedState& physical_state) {
  std::unique_ptr<Coordinator> coordinator(new CoordinatorImpl());
  PL_RETURN_IF_ERROR(coordinator->Init(physical_state));
  return coordinator;
}

Status Coordinator::Init(const distributedpb::DistributedState& physical_state) {
  return InitImpl(physical_state);
}

Status Coordinator::ProcessConfig(const CarnotInfo& carnot_info) {
  return ProcessConfigImpl(carnot_info);
}

StatusOr<std::unique_ptr<DistributedPlan>> Coordinator::Coordinate(const IR* logical_plan) {
  return CoordinateImpl(logical_plan);
}

Status CoordinatorImpl::InitImpl(const distributedpb::DistributedState& physical_state) {
  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    PL_RETURN_IF_ERROR(ProcessConfig(physical_state.carnot_info()[i]));
  }
  if (data_store_nodes_.size() == 0) {
    return error::InvalidArgument(
        "Distributed state does not have a Carnot instance that satisifies the condition "
        "`has_data_store() && processes_data()`.");
  }
  if (remote_processor_nodes_.size() == 0) {
    return error::InvalidArgument(
        "Distributed state does not have a Carnot instance that satisifies the condition "
        "`processes_data() && accepts_remote_sources()`.");
  }
  return Status::OK();
}

Status CoordinatorImpl::ProcessConfigImpl(const CarnotInfo& carnot_info) {
  if (carnot_info.has_data_store() && carnot_info.processes_data()) {
    data_store_nodes_.push_back(carnot_info);
  }
  if (carnot_info.processes_data() && carnot_info.accepts_remote_sources()) {
    remote_processor_nodes_.push_back(carnot_info);
  }
  return Status::OK();
}

bool CoordinatorImpl::HasExecutableNodes(const IR* plan) {
  // TODO(philkuz) (PL-1287) figure out what nodes are leftover that prevent us from using this
  // condition.
  if (plan->dag().nodes().size() == 0) {
    return false;
  }

  return plan->FindNodesThatMatch(Operator()).size() > 0;
}

bool CoordinatorImpl::KeepSource(OperatorIR* source, const distributedpb::CarnotInfo& carnot_info) {
  DCHECK(Match(source, SourceOperator()));
  // TODO(philkuz) in the future we will handle pruning down (Src,Filters) here as well.
  // TODO(philkuz) need some way to get metadata to prune src, filters
  if (!Match(source, UDTFSource())) {
    return true;
  }

  return UDTFMatchesFilters(static_cast<UDTFSourceIR*>(source), carnot_info);
}

bool CoordinatorImpl::UDTFMatchesFilters(UDTFSourceIR* source,
                                         const distributedpb::CarnotInfo& carnot_info) {
  const auto& udtf_spec = source->udtf_spec();
  for (const auto& [idx, arg] : Enumerate(udtf_spec.args())) {
    DataIR* data = source->arg_values()[idx];

    switch (arg.semantic_type()) {
      // We do not filter on None types.
      case types::ST_NONE: {
        continue;
      }
      case types::ST_UPID: {
        // These conditions should already be checked in pl_module.
        DCHECK_EQ(arg.arg_type(), types::UINT128);
        DCHECK_EQ(data->type(), IRNodeType::kUInt128);
        UInt128IR* upid_uint128 = static_cast<UInt128IR*>(data);
        // Convert string to UPID.
        // Get the ASID out of the UPID and compare it to the ASID of the Agent.
        if (md::UPID(upid_uint128->val()).asid() != carnot_info.asid()) {
          return false;
        }
        break;
      }
      case types::ST_AGENT_UID: {
        // TODO(philkuz) need a test for this.
        DCHECK_EQ(arg.arg_type(), types::STRING);
        DCHECK_EQ(data->type(), IRNodeType::kString);
        StringIR* str = static_cast<StringIR*>(data);
        auto uuid = ParseUUID(carnot_info.agent_id()).ConsumeValueOrDie();
        if (uuid.str() != str->str()) {
          return false;
        }
        continue;
      }
      default: {
        CHECK(false) << absl::Substitute("Argument spec for UDTF '$0' set improperly for '$1'",
                                         udtf_spec.name(), arg.name());
        break;
      }
    }
  }
  return true;
}

Status CoordinatorImpl::PrunePlan(IR* plan, const distributedpb::CarnotInfo& carnot_info) {
  // Get the sources to remove.
  std::vector<OperatorIR*> sources_to_remove;
  for (OperatorIR* plan_op : plan->GetSources()) {
    DCHECK(Match(plan_op, SourceOperator()));
    if (!KeepSource(plan_op, carnot_info)) {
      sources_to_remove.push_back(plan_op);
    }
  }

  return RemoveSourcesAndDependentOperators(plan, sources_to_remove);
}

// Removes the sources and any members of their "independent graphs".
Status CoordinatorImpl::RemoveSourcesAndDependentOperators(
    IR* plan, const std::vector<OperatorIR*>& sources_to_remove) {
  absl::flat_hash_set<int64_t> nodes_to_remove;
  std::queue<OperatorIR*> to_remove_q;
  for (auto src_op : sources_to_remove) {
    DCHECK(Match(src_op, SourceOperator()));
    to_remove_q.push(src_op);
  }
  // extra_parents queue tracks parents of removed operators that are not removed themselves.
  // We need to do extra analysis to determine if we remove those parents.
  std::queue<OperatorIR*> extra_parents;
  while (!to_remove_q.empty()) {
    OperatorIR* parent_op = to_remove_q.front();
    to_remove_q.pop();

    nodes_to_remove.insert(parent_op->id());
    for (OperatorIR* child : parent_op->Children()) {
      for (OperatorIR* other_parent_of_child : child->parents()) {
        // Make sure not to check parent_op in the extra_parents loop.
        if (other_parent_of_child != parent_op) {
          extra_parents.push(other_parent_of_child);
        }
      }
      to_remove_q.push(child);
    }
  }

  // Check to see if we can delete any extra parents of nodes
  while (!extra_parents.empty()) {
    OperatorIR* parent = extra_parents.front();
    extra_parents.pop();
    // The parent might have been deleted after being added to extra_parents.
    if (nodes_to_remove.contains(parent->id())) {
      continue;
    }

    // If all of operator's children have been removed, then we remove the op.
    bool parent_keeps_children = false;
    for (OperatorIR* child : parent->Children()) {
      if (!nodes_to_remove.contains(child->id())) {
        parent_keeps_children = true;
        break;
      }
    }
    // If the parent keeps children, then we don't delete the parent.
    if (parent_keeps_children) {
      continue;
    }
    nodes_to_remove.insert(parent->id());
    // Now check if the parents of the parent can be deleted.
    for (OperatorIR* grandparent : parent->parents()) {
      extra_parents.push(grandparent);
    }
  }

  return plan->Prune(nodes_to_remove);
}

const distributedpb::CarnotInfo& CoordinatorImpl::GetRemoteProcessor() const {
  // TODO(philkuz) update this with a more sophisticated strategy in the future.
  DCHECK_GT(remote_processor_nodes_.size(), 0UL);
  return remote_processor_nodes_[0];
}

using OperatorToAgentSet = absl::flat_hash_map<OperatorIR*, absl::flat_hash_set<int64_t>>;

/**
 * @brief Data structure that tracks a set of agents that can remove an Operator and simplifies
 * the set logic to combine two such data structures when recursively evaluating subexpressions that
 * produce different sets of agents and need to be combined.
 *
 * Comes with Methods that simplify an otherwise complex set logic for combining two agent sets
 * using boolean logic. The complexity comes the fact this data structure nature: these are agents
 * to _remove_ rather than keep, so the set operations are flipped from typical And/Or set logic.
 *
 * Simply:
 * And := Union
 * Or := Intersection
 *
 */
struct AgentSet {
  AgentSet Union(const AgentSet& other) {
    // If either struct is empty(), we only trim the other side of the expression.
    if (other.agents.empty()) {
      return *this;
    }
    // If this has no_agents, we return whatever is in other.
    if (agents.empty()) {
      return other;
    }
    AgentSet unioned;
    for (const auto& agent : agents) {
      unioned.agents.insert(agent);
    }
    for (const auto& agent : other.agents) {
      unioned.agents.insert(agent);
    }
    return unioned;
  }

  AgentSet Intersection(const AgentSet& other) {
    // If either struct is empty(), that means we trim no agents so we return the empty struct.
    if (other.agents.empty() || agents.empty()) {
      return AgentSet();
    }
    AgentSet intersection;
    const absl::flat_hash_set<int64_t>* smaller = &(other.agents);
    const absl::flat_hash_set<int64_t>* bigger = &agents;
    if (smaller->size() > bigger->size()) {
      bigger = &(other.agents);
      smaller = &agents;
    }
    for (const auto& agent : *smaller) {
      if (bigger->contains(agent)) {
        intersection.agents.insert(agent);
      }
    }
    return intersection;
  }

  absl::flat_hash_set<int64_t> agents;
};

class MapRemovableOperatorsRule : public Rule {
 public:
  /**
   * @brief Returns a mapping of operators in the query that can be removed from the plans of
   * corresponding agents.
   *
   * Intended to create a set of Operators that can be removed per agent which will then be used to
   * identify unique plans that are duplicated across all agents in a distributed plan.
   *
   * @param plan The distributed plan which describes all the agents in the system.
   * @param pem_instances The Agent IDs from `plan` that we use to build OperatorToAgentSet.
   * @param query The main plan that will derive all other plans. The source of all operators.
   * @return StatusOr<OperatorToAgentSet> the mapping of removable operators to the agents whose
   * plans can remove those operators.
   */
  static StatusOr<OperatorToAgentSet> GetRemovableOperators(
      DistributedPlan* plan, const std::vector<int64_t>& pem_instances, IR* query) {
    MapRemovableOperatorsRule rule(plan, pem_instances);
    PL_ASSIGN_OR_RETURN(auto did_remove, rule.Execute(query));
    DCHECK_EQ(did_remove, !rule.op_to_agent_set.empty());
    return rule.op_to_agent_set;
  }

 protected:
  MapRemovableOperatorsRule(DistributedPlan* plan, const std::vector<int64_t>& pem_instances)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        plan_(plan),
        pem_instances_(pem_instances) {}

  StatusOr<bool> Apply(IRNode* node) override {
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

  /**
   * @brief Returns the set of agents that are removed by this expression.
   *
   * Will return agents if an expression contains a metadata equal expression predicate
   * as well as the composition of such subexpressions inside ofboolean conjunction.
   *
   * @param expr the filter expression to evaluate.
   * @return AgentSet the set of agents that are filtered out by the expression.
   */
  AgentSet FilterExpressionMayProduceData(ExpressionIR* expr) {
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
      auto lhs = FilterExpressionMayProduceData(func->args()[0]);
      auto rhs = FilterExpressionMayProduceData(func->args()[1]);
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
      auto* md_filter = plan_->Get(pem)->metadata_filter();
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

  StatusOr<bool> CheckFilter(FilterIR* filter_ir) {
    AgentSet agents_that_remove_op = FilterExpressionMayProduceData(filter_ir->filter_expr());
    // If the filter appears on all agents, we don't wanna add it.
    if (agents_that_remove_op.agents.empty()) {
      return false;
    }
    op_to_agent_set[filter_ir] = std::move(agents_that_remove_op.agents);
    return true;
  }

  StatusOr<bool> CheckMemorySource(MemorySourceIR* mem_src_ir) {
    PL_UNUSED(mem_src_ir);
    return false;
  }

  StatusOr<bool> CheckUDTFSource(UDTFSourceIR* udtf_ir) {
    PL_UNUSED(udtf_ir);
    return false;
  }

  OperatorToAgentSet op_to_agent_set;
  DistributedPlan* plan_;
  const std::vector<int64_t>& pem_instances_;
};

/**
 * @brief PlanCluster is the data structure used to each unique plan in the distributed plan.
 * Can then call CreatePlan to return a plan with the specified ops removed.
 */
struct PlanCluster {
  PlanCluster(absl::flat_hash_set<int64_t> agents, absl::flat_hash_set<OperatorIR*> ops)
      : agent_set(std::move(agents)), ops_to_remove(std::move(ops)) {}

  StatusOr<std::unique_ptr<IR>> CreatePlan(const IR* base_query) const {
    // TODO(philkuz) invert this so we don't clone everything.
    PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> new_ir, base_query->Clone());
    for (const auto& op : ops_to_remove) {
      DCHECK(Match(new_ir->Get(op->id()), Operator()));
      std::queue<OperatorIR*> ancestor_to_maybe_delete_q;
      for (const auto& p : static_cast<OperatorIR*>(new_ir->Get(op->id()))->parents()) {
        ancestor_to_maybe_delete_q.push(p);
      }

      PL_RETURN_IF_ERROR(new_ir->DeleteSubtree(op->id()));
      while (!ancestor_to_maybe_delete_q.empty()) {
        OperatorIR* ancestor = ancestor_to_maybe_delete_q.front();
        ancestor_to_maybe_delete_q.pop();
        // If all the children have been deleted, clean up the ancestor.
        if (ancestor->Children().size() != 0) {
          continue;
        }
        for (const auto& p : ancestor->parents()) {
          ancestor_to_maybe_delete_q.push(p);
        }
        PL_RETURN_IF_ERROR(new_ir->DeleteSubtree(ancestor->id()));
      }
    }
    return new_ir;
  }

  // The agents that correspond to this plan.
  absl::flat_hash_set<int64_t> agent_set;
  absl::flat_hash_set<OperatorIR*> ops_to_remove;
};

/**
 * A mapping of agent IDs to the corresponding plan.
 */
struct AgentToPlanMap {
  absl::flat_hash_map<int64_t, IR*> agent_to_plan_map;
  std::vector<std::unique_ptr<IR>> plan_pool;
};

/**
 * @brief Clusters Agents together based on similar sets of Operators to prune from the original
 * query plan.
 *
 * Finds the unique PEM plans based on the agents that remove the same set of Operators.
 *
 * @param set
 * @return std::vector<PlanCluster>
 */
std::vector<PlanCluster> ClusterOperators(const OperatorToAgentSet& set) {
  OperatorToAgentSet op_to_agents = set;
  std::vector<PlanCluster> plan_clusters;
  // While we still have agents that are in the ops_to_agents set.
  // Every loop iteration should finish with a new cluster.
  while (!op_to_agents.empty()) {
    absl::flat_hash_set<OperatorIR*> operators;
    absl::flat_hash_set<int64_t> current_set;
    for (const auto& [op, agent_set] : op_to_agents) {
      if (agent_set.empty()) {
        continue;
      }
      // If the current_set is empty, we need to start accumulating it and this operator will be the
      // first of the new cluster.
      if (current_set.empty()) {
        operators.insert(op);
        current_set = agent_set;
        continue;
      }
      absl::flat_hash_set<int64_t> intersection;
      for (const auto& c : current_set) {
        if (agent_set.contains(c)) {
          intersection.insert(c);
        }
      }
      // If the intersection is empty, we should just not include this op for now.
      if (intersection.empty()) {
        continue;
      }
      // If the intersection is non-empty that is our new set of agents for the cluster.
      current_set = std::move(intersection);
      operators.insert(op);
    }
    // Create the new cluster with the set of agents and the operators.
    plan_clusters.emplace_back(current_set, operators);
    // Remove the agents in the clusters from the OperatorToAgentSet mapping, as we know they
    // should not belong in other clusters.
    OperatorToAgentSet new_op_to_agents;
    for (const auto& [op, agents] : op_to_agents) {
      for (const auto& agent : agents) {
        if (current_set.contains(agent)) {
          continue;
        }
        new_op_to_agents[op].insert(agent);
      }
    }
    op_to_agents = std::move(new_op_to_agents);
  }
  return plan_clusters;
}

/**
 * @brief Returns the set of all_agents that don't appear in OperatorToAgentSet.
 *
 * @param op_to_agent_set The operators that can be removed on the specified agents.
 * @param all_agents Every agent that we want to do work.
 * @return absl::flat_hash_set<int64_t>
 */
absl::flat_hash_set<int64_t> RemainingAgents(const OperatorToAgentSet& op_to_agent_set,
                                             const std::vector<int64_t>& all_agents) {
  absl::flat_hash_set<int64_t> remaining_agents(all_agents.begin(), all_agents.end());
  for (const auto& [op, agent_set] : op_to_agent_set) {
    for (const auto& agent : agent_set) {
      remaining_agents.erase(agent);
    }
  }
  return remaining_agents;
}

StatusOr<AgentToPlanMap> GetPEMPlans(IR* query, DistributedPlan* plan,
                                     const std::vector<int64_t>& carnot_instances) {
  PL_ASSIGN_OR_RETURN(
      OperatorToAgentSet removable_ops_to_agents,
      MapRemovableOperatorsRule::GetRemovableOperators(plan, carnot_instances, query));
  AgentToPlanMap agent_to_plan_map;
  if (removable_ops_to_agents.empty()) {
    // Create the default single PEM map.
    PL_ASSIGN_OR_RETURN(auto default_ir_uptr, query->Clone());
    auto default_ir = default_ir_uptr.get();
    agent_to_plan_map.plan_pool.push_back(std::move(default_ir_uptr));
    // TODO(philkuz) enable this when we move over the Distributed analyzer.
    // plan->AddPlan(std::move(default_ir_uptr));
    for (int64_t carnot_i : carnot_instances) {
      agent_to_plan_map.agent_to_plan_map[carnot_i] = default_ir;
    }
    return agent_to_plan_map;
  }

  std::vector<PlanCluster> clusters = ClusterOperators(removable_ops_to_agents);
  // Cluster representing the original plan if any exist.
  auto remaining_agents = RemainingAgents(removable_ops_to_agents, carnot_instances);
  if (!remaining_agents.empty()) {
    clusters.emplace_back(remaining_agents, absl::flat_hash_set<OperatorIR*>{});
  }
  for (const auto& c : clusters) {
    PL_ASSIGN_OR_RETURN(auto cluster_plan_uptr, c.CreatePlan(query));
    auto cluster_plan = cluster_plan_uptr.get();
    if (cluster_plan->FindNodesThatMatch(Operator()).empty()) {
      continue;
    }
    agent_to_plan_map.plan_pool.push_back(std::move(cluster_plan_uptr));
    // TODO(philkuz) enable this when we move over the Distributed analyzer.
    // plan->AddPlan(std::move(cluster_plan_uptr));
    for (const auto& agent : c.agent_set) {
      agent_to_plan_map.agent_to_plan_map[agent] = cluster_plan;
    }
  }
  return agent_to_plan_map;
}

StatusOr<std::unique_ptr<DistributedPlan>> CoordinatorImpl::CoordinateImpl(const IR* logical_plan) {
  // TODO(zasgar) set support_partial_agg to true to enable partial aggs.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<DistributedSplitter> splitter,
                      DistributedSplitter::Create(/* support_partial_agg */ false));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<BlockingSplitPlan> split_plan,
                      splitter->SplitKelvinAndAgents(logical_plan));
  auto distributed_plan = std::make_unique<DistributedPlan>();
  PL_ASSIGN_OR_RETURN(int64_t remote_node_id, distributed_plan->AddCarnot(GetRemoteProcessor()));
  // TODO(philkuz) Need to update the Blocking Split Plan to better represent what we expect.
  // TODO(philkuz) (PL-1469) Future support for grabbing data from multiple Kelvin nodes.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> remote_plan, split_plan->original_plan->Clone());
  distributed_plan->Get(remote_node_id)->AddPlan(std::move(remote_plan));
  // TODO(philkuz) enable this when we move over the Distributed analyzer.
  // distributed_plan->Get(remote_node_id)->AddPlan(remote_plan.get());
  // distributed_plan->AddPlan(std::move(remote_plan));

  std::vector<int64_t> source_node_ids;
  for (const auto& [i, data_store_info] : Enumerate(data_store_nodes_)) {
    PL_ASSIGN_OR_RETURN(int64_t source_node_id, distributed_plan->AddCarnot(data_store_info));
    distributed_plan->AddEdge(source_node_id, remote_node_id);
    source_node_ids.push_back(source_node_id);
  }

  PL_ASSIGN_OR_RETURN(auto agent_to_plan_map, GetPEMPlans(split_plan->before_blocking.get(),
                                                          distributed_plan.get(), source_node_ids));
  for (const auto carnot_id : source_node_ids) {
    if (!agent_to_plan_map.agent_to_plan_map.contains(carnot_id)) {
      PL_RETURN_IF_ERROR(distributed_plan->DeleteNode(carnot_id));
      continue;
    }
    PL_ASSIGN_OR_RETURN(auto clone, agent_to_plan_map.agent_to_plan_map[carnot_id]->Clone());
    distributed_plan->Get(carnot_id)->AddPlan(std::move(clone));
    // TODO(philkuz) enable this when we move over the Distributed analyzer.
    // distributed_plan->Get(carnot_id)->AddPlan(agent_to_plan_map.agent_to_plan_map[carnot_id]);
  }

  return distributed_plan;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
