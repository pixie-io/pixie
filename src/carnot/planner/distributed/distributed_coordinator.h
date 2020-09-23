#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/planner/distributed/distributed_plan.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

using distributedpb::CarnotInfo;

struct CarnotGraph {
  plan::DAG dag;
  absl::flat_hash_map<int64_t, distributedpb::CarnotInfo> id_to_carnot_info;
};

/**
 * @brief The coordinator takes in a physical state and builds up the skeleton
 * of the physical plan graph based on the capabilities of the Carnot nodes passed in.
 */
class Coordinator : public NotCopyable {
 public:
  virtual ~Coordinator() = default;
  static StatusOr<std::unique_ptr<Coordinator>> Create(
      const distributedpb::DistributedState& distributed_state);

  /**
   * @brief Using the physical state and the current plan, assembles a proto Distributed Plan. This
   * plan is not ready to be sent out yet, but can be processed to work.
   * @param plan: the plan, pre-split along the expected lines.
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Coordinate(const IR* logical_plan);

  Status Init(const distributedpb::DistributedState& distributed_state);

 protected:
  Status ProcessConfig(const CarnotInfo& carnot_info);

  virtual Status InitImpl(const distributedpb::DistributedState& distributed_state) = 0;

  /**
   * @brief Implementation of the Coordinate function. Using the phyiscal state and the plan, should
   * output a CarnotGraph that connects the different carnot instances
   *
   * @return StatusOr<CarnotGraph>
   */
  virtual StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) = 0;

  virtual Status ProcessConfigImpl(const CarnotInfo& carnot_info) = 0;
};

/**
 * @brief This coordinator creates a plan layout with 1 remote processor getting data
 * from N sources. If the passed in plan has special conditions, it will split differntly.
 *
 */
class CoordinatorImpl : public Coordinator {
 protected:
  StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) override;
  Status InitImpl(const distributedpb::DistributedState& distributed_state) override;
  Status ProcessConfigImpl(const CarnotInfo& carnot_info) override;

 private:
  const distributedpb::CarnotInfo& GetRemoteProcessor() const;
  bool HasExecutableNodes(const IR* plan);

  /**
   * @brief Removes the sources and any operators depending on that source. Operators that depend on
   * the source not only means the Transitive dependents, but also any parents of those Transitive
   * dependents that are not dependents of the sources to be deleted but don't feed data anywhere
   * else as a result of the source being deleted.
   *
   * For example in the following graph with UDTFSrc set for
   * removal:
   *
   * UDTFSrc   MemSrc
   *        \  /
   *         \/
   *        Join
   *
   * We would delete the entire graph by first marking UDTFsrc and Join for removal, pushing
   * MemSrc into the extra_parents queue and then marking it for removal after.  However, in the
   * following graph, we would only want to delete UDTF->Join, as MemSrc->GRPCSink should still be
   * data we would want to collect.
   *
   *  UDTFSrc   MemSrc
   *        \  /      \
   *         \/        \
   *        Join       GRPCSink
   *
   */
  Status RemoveSourcesAndDependentOperators(IR* plan,
                                            const std::vector<OperatorIR*>& sources_to_remove);
  // Nodes that have a source of data.
  std::vector<CarnotInfo> data_store_nodes_;
  // Nodes that remotely prcoess data.
  std::vector<CarnotInfo> remote_processor_nodes_;
  // The distributed state object.
  const distributedpb::DistributedState* distributed_state_ = nullptr;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
