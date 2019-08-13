#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/compilerpb/physical_plan.pb.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/compiler/rule_executor.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

/**
 * @brief Object that represents a physical entity that uses the Carnot stream engine.
 * Contains the current plan on the node as well as physical information about the node.
 *
 */
class CarnotInstance {
 public:
  CarnotInstance(int64_t id, const compilerpb::CarnotInfo& carnot_info)
      : id_(id), carnot_info_(carnot_info) {}
  bool IsAgent() const {
    // return carnot_info_.
    return false;
  }
  const std::string& QueryBrokerAddress() const { return carnot_info_.query_broker_address(); }
  int64_t id() const { return id_; }

  compilerpb::CarnotInfo PlanProto() {
    // TODO(philkuz) Move compiler IRToLogicalPlan into IR::ToProto.
    return compilerpb::CarnotInfo();
    // return plan->ToProto();
  }

 private:
  // The id used by the physical plan to define the DAG.
  int64_t id_;
  // The specification of this carnot instance.
  compilerpb::CarnotInfo carnot_info_;
  std::unique_ptr<IR> plan;
};

class PhysicalPlan {
 public:
  const plan::DAG& dag() const { return dag_; }
  compilerpb::PhysicalPlan ToProto() const;

 private:
  plan::DAG dag_;
  absl::flat_hash_map<int64_t, CarnotInstance> id_to_node_map_;
};

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
