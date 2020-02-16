#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/compiler/distributed/distributed_coordinator.h"
#include "src/carnot/compiler/distributed/distributed_splitter.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace compiler {
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

Status OneRemoteCoordinator::InitImpl(const distributedpb::DistributedState& physical_state) {
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

Status OneRemoteCoordinator::ProcessConfigImpl(const CarnotInfo& carnot_info) {
  if (carnot_info.has_data_store() && carnot_info.processes_data()) {
    data_store_nodes_.push_back(carnot_info);
  }
  if (carnot_info.processes_data() && carnot_info.accepts_remote_sources()) {
    remote_processor_nodes_.push_back(carnot_info);
  }
  return Status::OK();
}

StatusOr<std::unique_ptr<DistributedPlan>> OneRemoteCoordinator::CoordinateImpl(
    const IR* logical_plan) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<BlockingSplitPlan> split_plan,
                      DistributedSplitter::SplitAtBlockingNode(logical_plan));
  auto physical_plan = std::make_unique<DistributedPlan>();
  DCHECK_GT(remote_processor_nodes_.size(), 0UL);
  int64_t remote_node_id = physical_plan->AddCarnot(remote_processor_nodes_[0]);

  // TODO(philkuz) in the future save the remote_processor_nodes that are not being used
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> remote_plan, split_plan->after_blocking->Clone());
  physical_plan->Get(remote_node_id)->AddPlan(std::move(remote_plan));

  for (const auto& data_store_info : data_store_nodes_) {
    int64_t source_node_id = physical_plan->AddCarnot(data_store_info);
    PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> source_plan, split_plan->before_blocking->Clone());
    physical_plan->Get(source_node_id)->AddPlan(std::move(source_plan));
    physical_plan->AddEdge(source_node_id, remote_node_id);
  }
  return physical_plan;
}

StatusOr<std::unique_ptr<NoRemoteCoordinator>> NoRemoteCoordinator::Create(
    const distributedpb::DistributedState& physical_state) {
  std::unique_ptr<NoRemoteCoordinator> coordinator(new NoRemoteCoordinator());
  PL_RETURN_IF_ERROR(coordinator->Init(physical_state));
  return coordinator;
}

Status NoRemoteCoordinator::InitImpl(const distributedpb::DistributedState& physical_state) {
  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    PL_RETURN_IF_ERROR(ProcessConfig(physical_state.carnot_info()[i]));
  }
  if (data_store_nodes_.size() == 0) {
    return error::InvalidArgument(
        "Distributed state does not have a Carnot instance that satisfies the condition "
        "`has_data_store() && processes_data()`.");
  }
  return Status::OK();
}

Status NoRemoteCoordinator::ProcessConfigImpl(const CarnotInfo& carnot_info) {
  if (carnot_info.has_data_store() && carnot_info.processes_data()) {
    data_store_nodes_.push_back(carnot_info);
  }
  return Status::OK();
}

StatusOr<std::unique_ptr<DistributedPlan>> NoRemoteCoordinator::CoordinateImpl(
    const IR* logical_plan) {
  auto physical_plan = std::make_unique<DistributedPlan>();
  for (const auto& data_store_info : data_store_nodes_) {
    int64_t source_node_id = physical_plan->AddCarnot(data_store_info);
    PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> source_plan, logical_plan->Clone());
    physical_plan->Get(source_node_id)->AddPlan(std::move(source_plan));
  }
  return physical_plan;
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
        if (carnot_info.query_broker_address() != str->str()) {
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

StatusOr<std::unique_ptr<DistributedPlan>> CoordinatorImpl::CoordinateImpl(const IR* logical_plan) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<BlockingSplitPlan> split_plan,
                      DistributedSplitter::SplitKelvinAndAgents(logical_plan));
  auto physical_plan = std::make_unique<DistributedPlan>();
  int64_t remote_node_id = physical_plan->AddCarnot(GetRemoteProcessor());

  // TODO(philkuz) Need to update the Blocking Split Plan to better represent what we expect.
  // TODO(philkuz) (PL-1469) Future support for grabbing data from multiple Kelvin nodes.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> remote_plan, split_plan->original_plan->Clone());
  physical_plan->Get(remote_node_id)->AddPlan(std::move(remote_plan));

  for (const auto& data_store_info : data_store_nodes_) {
    PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> source_plan, split_plan->before_blocking->Clone());

    int64_t source_node_id = physical_plan->AddCarnot(data_store_info);
    physical_plan->Get(source_node_id)->AddPlan(std::move(source_plan));
    physical_plan->AddEdge(source_node_id, remote_node_id);
  }
  return physical_plan;
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
