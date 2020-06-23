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
  std::unique_ptr<Coordinator> planner(new CoordinatorImpl());
  PL_RETURN_IF_ERROR(planner->Init(physical_state));
  return planner;
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

// TODO(nserrino): Support aliases for pruning metadata filters,
// such as p = df.ctx['pod'], then filter on p.
bool FilterExpressionMayProduceData(ExpressionIR* expr, const md::AgentMetadataFilter& md_filter) {
  if (!Match(expr, Func())) {
    return true;
  }
  auto func = static_cast<FuncIR*>(expr);
  if (func->args().size() != 2) {
    return true;
  }

  auto logical_and = Match(expr, LogicalAnd(Value(), Value()));
  auto logical_or = Match(expr, LogicalOr(Value(), Value()));
  if (logical_and || logical_or) {
    auto lhs = FilterExpressionMayProduceData(func->args()[0], md_filter);
    auto rhs = FilterExpressionMayProduceData(func->args()[1], md_filter);
    return logical_and ? lhs && rhs : lhs || rhs;
  }

  if (Match(expr, Equals(MetadataExpression(), String()))) {
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
    if (!md_filter.metadata_types().contains(metadata_type)) {
      return true;
    }
    return md_filter.ContainsEntity(metadata_type, value->str());
  }
  return true;
}

/**
 * @brief Evaluates whether an operator may produce data on a given Carnot instance.
 * Will first detect when the query filters data to metadata that is not part of the
 * input Carnot instance, but in the future can add more logic based on column contents, etc.
 */
bool OperatorMayProduceData(OperatorIR* op, const md::AgentMetadataFilter& md_filter) {
  // If the filter makes it so that no data will be produced, return false.
  if (Match(op, Filter()) &&
      !FilterExpressionMayProduceData(static_cast<FilterIR*>(op)->filter_expr(), md_filter)) {
    return false;
  }
  auto parents = op->parents();
  if (parents.size() == 0) {
    return true;
  }
  for (OperatorIR* parent : parents) {
    // If any parent may produce data, return true.
    // TODO(nserrino): Optimize this, for example inner joins would need both
    // parents to produce data. However, inner joins are not on the "before blocking"
    // side of the query, so it isn't relevant for now.
    if (OperatorMayProduceData(parent, md_filter)) {
      return true;
    }
  }

  // If none of the parents may produce data, neither will this operator.
  return false;
}

// Returns a set containing the input sink and all of its ancestor ops.
absl::flat_hash_set<OperatorIR*> GetSinkAndAncestors(OperatorIR* sink) {
  absl::flat_hash_set<OperatorIR*> result;
  std::queue<OperatorIR*> queue;
  queue.push(sink);
  while (queue.size()) {
    auto op = queue.front();
    result.insert(op);
    for (OperatorIR* parent_op : op->parents()) {
      queue.push(parent_op);
    }
    queue.pop();
  }
  return result;
}

/**
 * @brief Produces the IR for each input Carnot instance in `plan` with the relevant sinks
 * of `query` based on metadata stored on each selected carnot instance.
 *
 * 1. For the Carnot in the list, create an entry in `plan`.
 * 2. Find the parts of `query` that might produce data on each Carnot
 * 3. Copy over the relevant subgraphs of `query` for that particular carnot instance.
 * We currently filter on Carnot metadata only for now.
 * This function guarantees that even if no Carnots will produce data for a sink in `query`,
 * each sink will still be run on at least one Carnot instance in the list.
 * The work necessary to connect the output of these Carnot instances to a GRPCSink must be done
 * in the client, because this function does not assume a particular configuration.
 */
StatusOr<absl::flat_hash_map<int64_t, std::unique_ptr<IR>>> GetCarnotPlans(
    IR* query, DistributedPlan* plan, const std::vector<int64_t>& carnot_instances) {
  absl::flat_hash_map<int64_t, std::unique_ptr<IR>> carnot_plans;

  // Map each sink to its dependency operators.
  absl::flat_hash_map<GRPCSinkIR*, absl::flat_hash_set<OperatorIR*>> sinks_to_ancestor_ops;
  auto sink_nodes = query->FindNodesThatMatch(GRPCSink());
  for (IRNode* node : sink_nodes) {
    auto casted_op = static_cast<GRPCSinkIR*>(node);
    sinks_to_ancestor_ops[casted_op] = GetSinkAndAncestors(casted_op);
  }

  // Used to make sure each sink runs on at least one of the input Carnots, even if none will
  // produce data.
  absl::flat_hash_set<GRPCSinkIR*> allocated_sinks;

  for (const auto& [carnot_idx, carnot_id] : Enumerate(carnot_instances)) {
    absl::flat_hash_set<OperatorIR*> relevant_ops_for_instance;
    md::AgentMetadataFilter* md_filter = plan->Get(carnot_id)->metadata_filter();

    for (const auto& [sink, sink_and_ancestors] : sinks_to_ancestor_ops) {
      auto sink_produces_data = true;
      // If we don't have a metadata filter on this Carnot, we assume it might produce data.
      if (md_filter != nullptr) {
        sink_produces_data = OperatorMayProduceData(sink, *md_filter);
      }
      // If the sink produces data, or this is the last Carnot instance and no Carnot has
      // matched this subgraph, then add it to the current Carnot instance.
      if (!sink_produces_data &&
          (carnot_idx < carnot_instances.size() - 1 || allocated_sinks.contains(sink))) {
        continue;
      }
      allocated_sinks.insert(sink);
      relevant_ops_for_instance.insert(sink_and_ancestors.begin(), sink_and_ancestors.end());
    }

    carnot_plans[carnot_id] = std::make_unique<IR>();
    PL_RETURN_IF_ERROR(
        carnot_plans[carnot_id]->CopyOperatorSubgraph(query, relevant_ops_for_instance));
  }
  return carnot_plans;
}

StatusOr<std::unique_ptr<DistributedPlan>> CoordinatorImpl::CoordinateImpl(const IR* logical_plan) {
  // TODO(zasgar) set support_partial_agg to true to enable partial aggs.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<DistributedSplitter> splitter,
                      DistributedSplitter::Create(/* support_partial_agg */ false));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<BlockingSplitPlan> split_plan,
                      splitter->SplitKelvinAndAgents(logical_plan));
  auto physical_plan = std::make_unique<DistributedPlan>();
  PL_ASSIGN_OR_RETURN(int64_t remote_node_id, physical_plan->AddCarnot(GetRemoteProcessor()));
  // TODO(philkuz) Need to update the Blocking Split Plan to better represent what we expect.
  // TODO(philkuz) (PL-1469) Future support for grabbing data from multiple Kelvin nodes.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> remote_plan, split_plan->original_plan->Clone());
  physical_plan->Get(remote_node_id)->AddPlan(std::move(remote_plan));

  std::vector<int64_t> source_node_ids;
  for (const auto& [i, data_store_info] : Enumerate(data_store_nodes_)) {
    PL_ASSIGN_OR_RETURN(int64_t source_node_id, physical_plan->AddCarnot(data_store_info));
    physical_plan->AddEdge(source_node_id, remote_node_id);
    source_node_ids.push_back(source_node_id);
  }

  PL_ASSIGN_OR_RETURN(auto carnot_plans, GetCarnotPlans(split_plan->before_blocking.get(),
                                                        physical_plan.get(), source_node_ids));
  for (const auto carnot_id : source_node_ids) {
    physical_plan->Get(carnot_id)->AddPlan(std::move(carnot_plans.at(carnot_id)));
  }

  return physical_plan;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
