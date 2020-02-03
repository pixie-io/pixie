#pragma once
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/compiler/compiler_state/registry_info.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/planpb/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

// Forward declare here so that CarnotInstance has access.
class DistributedPlan;

/**
 * @brief Object that represents a physical entity that uses the Carnot stream engine.
 * Contains the current plan on the node as well as physical information about the node.
 *
 */
class CarnotInstance {
 public:
  CarnotInstance(int64_t id, const distributedpb::CarnotInfo& carnot_info,
                 DistributedPlan* parent_plan)
      : id_(id), carnot_info_(carnot_info), distributed_plan_(parent_plan) {}

  const std::string& QueryBrokerAddress() const { return carnot_info_.query_broker_address(); }
  int64_t id() const { return id_; }

  void AddPlan(std::unique_ptr<IR> plan) { plan_ = std::move(plan); }

  StatusOr<planpb::Plan> PlanProto() const { return plan_->ToProto(); }

  distributedpb::CarnotInfo carnot_info() const { return carnot_info_; }

  IR* plan() const { return plan_.get(); }
  DistributedPlan* distributed_plan() const { return distributed_plan_; }

  std::string DebugString() const {
    return absl::Substitute("Carnot(id=$0, qb_address=$1)", id(), QueryBrokerAddress());
  }

 private:
  // The id used by the physical plan to define the DAG.
  int64_t id_;
  // The specification of this carnot instance.
  distributedpb::CarnotInfo carnot_info_;
  std::unique_ptr<IR> plan_;
  // The distributed plan that this instance belongs to.
  DistributedPlan* distributed_plan_;
};

// TODO(nserrino): Refactor this and IR to share a common base class for shared operations like
// AddEdge, etc.
class DistributedPlan {
 public:
  /**
   * @brief Adds a Carnot instance into the graph, and assigns a new id.
   *
   * @param carnot_instance the proto representation of the Carnot instance.
   * @return the id of the added carnot instance.
   */
  int64_t AddCarnot(const distributedpb::CarnotInfo& carnot_instance);

  /**
   * @brief Gets the carnot instance at the index i.
   *
   * @param i: id to grab from the node map.
   * @return pointer to the Carnot instance at index i.
   */
  CarnotInstance* Get(int64_t i) const {
    auto id_node_iter = id_to_node_map_.find(i);
    CHECK(id_node_iter != id_to_node_map_.end()) << "Couldn't find index: " << i;
    return id_node_iter->second.get();
  }

  void AddEdge(CarnotInstance* from, CarnotInstance* to) { dag_.AddEdge(from->id(), to->id()); }
  void AddEdge(int64_t from, int64_t to) { dag_.AddEdge(from, to); }
  bool HasNode(int64_t node_id) const { return dag_.HasNode(node_id); }

  Status DeleteNode(int64_t node) {
    if (!HasNode(node)) {
      return error::InvalidArgument("No node $0 exists in graph.", node);
    }
    dag_.DeleteNode(node);
    return Status::OK();
  }

  StatusOr<distributedpb::DistributedPlan> ToProto() const;

  const plan::DAG& dag() const { return dag_; }

  void SetPlanOptions(planpb::PlanOptions plan_options) { plan_options_.CopyFrom(plan_options); }

 private:
  plan::DAG dag_;
  absl::flat_hash_map<int64_t, std::unique_ptr<CarnotInstance>> id_to_node_map_;
  int64_t id_counter_ = 0;

  planpb::PlanOptions plan_options_;
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
