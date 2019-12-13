#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace compiler {
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
      const distributedpb::DistributedState& physical_state);

  /**
   * @brief Using the physical state and the current plan, assembles a proto Distributed Plan. This
   * plan is not ready to be sent out yet, but can be processed to work.
   * @param plan: the plan, pre-split along the expected lines.
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Coordinate(const IR* logical_plan);

  Status Init(const distributedpb::DistributedState& physical_state);

 protected:
  Status ProcessConfig(const CarnotInfo& carnot_info);

  virtual Status InitImpl(const distributedpb::DistributedState& physical_state) = 0;

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
 * from N sources.
 *
 */
class OneRemoteCoordinator : public Coordinator {
 protected:
  StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) override;
  Status InitImpl(const distributedpb::DistributedState& physical_state) override;
  Status ProcessConfigImpl(const CarnotInfo& carnot_info) override;

 private:
  // Nodes that have a source of data.
  std::vector<CarnotInfo> data_store_nodes_;
  // Nodes that remotely prcoess data.
  std::vector<CarnotInfo> remote_processor_nodes_;
};

/**
 * @brief This corodinator createsa a plan laytout with no remote processors and N data sources.
 *
 */
class NoRemoteCoordinator : public Coordinator {
 public:
  static StatusOr<std::unique_ptr<NoRemoteCoordinator>> Create(
      const distributedpb::DistributedState& physical_state);

 protected:
  StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) override;
  Status InitImpl(const distributedpb::DistributedState& physical_state) override;
  Status ProcessConfigImpl(const CarnotInfo& carnot_info) override;

 private:
  // Nodes that have a source of data.
  std::vector<CarnotInfo> data_store_nodes_;
};
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
