#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_splitter.h"
#include "src/carnot/compiler/rules.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

StatusOr<std::unique_ptr<Coordinator>> Coordinator::Create(
    const compilerpb::DistributedState& physical_state) {
  std::unique_ptr<Coordinator> planner(new OneRemoteCoordinator());
  PL_RETURN_IF_ERROR(planner->Init(physical_state));
  return planner;
}

Status Coordinator::Init(const compilerpb::DistributedState& physical_state) {
  return InitImpl(physical_state);
}

Status Coordinator::ProcessConfig(const CarnotInfo& carnot_info) {
  return ProcessConfigImpl(carnot_info);
}

StatusOr<std::unique_ptr<DistributedPlan>> Coordinator::Coordinate(const IR* logical_plan) {
  return CoordinateImpl(logical_plan);
}

Status OneRemoteCoordinator::InitImpl(const compilerpb::DistributedState& physical_state) {
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
  // TODO(philkuz) (PL-861) remove the reliance on compiler_state for rules, or at least physical
  // splitter.
  PL_ASSIGN_OR_RETURN(std::unique_ptr<BlockingSplitPlan> split_plan,
                      DistributedSplitter(nullptr).SplitAtBlockingNode(logical_plan));
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
    const compilerpb::DistributedState& physical_state) {
  std::unique_ptr<NoRemoteCoordinator> coordinator(new NoRemoteCoordinator());
  PL_RETURN_IF_ERROR(coordinator->Init(physical_state));
  return coordinator;
}

Status NoRemoteCoordinator::InitImpl(const compilerpb::DistributedState& physical_state) {
  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    PL_RETURN_IF_ERROR(ProcessConfig(physical_state.carnot_info()[i]));
  }
  if (data_store_nodes_.size() == 0) {
    return error::InvalidArgument(
        "Distributed state does not have a Carnot instance that satisifies the condition "
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

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
