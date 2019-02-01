#pragma once

#include <glog/logging.h>
#include <memory>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace plan {

class PlanFragment final : public PlanGraph<carnotpb::PlanFragment, Operator, carnotpb::PlanNode> {
 public:
  explicit PlanFragment(int64_t id) : id_(id) {}
  static std::unique_ptr<PlanFragment> FromProto(const carnotpb::PlanFragment &pb, int64_t id);

 protected:
  int64_t id_;
};

/**
 * A walker that walks the operators of a plan fragment in a topologically-sorted order.
 * A sample usage is:
 * To get the count of each operator type:
 *   PlanFragmentWalker()
 *      .OnMemorySource([&](auto& mem_src) {
 *       mem_src_call_count += 1;
 *     })
 *     .OnMap([&](auto& map) {
 *       map_call_count += 1;
 *     })
 *     .OnBlockingAggregate([&](auto& blocking_agg) {
 *       blocking_agg_call_count += 1;
 *     })
 *     .OnMemorySink([&](auto& mem_sink) {
 *       mem_sink_call_count += 1;
 *     })
 *     .Walk(&plan_fragment_);
 */
class PlanFragmentWalker {
 public:
  using MemorySourceWalkFn = std::function<void(const MemorySourceOperator &)>;
  using MapWalkFn = std::function<void(const MapOperator &)>;
  using BlockingAggregateWalkFn = std::function<void(const BlockingAggregateOperator &)>;
  using MemorySinkWalkFn = std::function<void(const MemorySinkOperator &)>;

  /**
   * Register callback for when a memory source operator is encountered.
   * @param fn The function to call when a MemorySourceOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker &OnMemorySource(const MemorySourceWalkFn &fn) {
    on_memory_source_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a map operator is encountered.
   * @param fn The function to call when a MapOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker &OnMap(const MapWalkFn &fn) {
    on_map_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a blocking aggregate operator is encountered.
   * @param fn The function to call when a BlockingAggregateOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker &OnBlockingAggregate(const BlockingAggregateWalkFn &fn) {
    on_blocking_aggregate_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a memory sink operator is encountered.
   * @param fn The function to call when a MemorySinkOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker &OnMemorySink(const MemorySinkWalkFn &fn) {
    on_memory_sink_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a walk of the plan fragment operators in a topologically-sorted order.
   * @param plan_fragment The plan fragment to walk.
   */
  void Walk(PlanFragment *plan_fragment);

 private:
  void CallWalkFn(const Operator &op);

  template <typename T, typename TWalkFunc>
  void CallAs(TWalkFunc const &fn, const Operator &op);

  MemorySourceWalkFn on_memory_source_walk_fn_;
  MapWalkFn on_map_walk_fn_;
  BlockingAggregateWalkFn on_blocking_aggregate_walk_fn_;
  MemorySinkWalkFn on_memory_sink_walk_fn_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
