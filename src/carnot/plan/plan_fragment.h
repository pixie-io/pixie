#pragma once

#include <memory>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace plan {

class PlanFragment final : public PlanGraph<planpb::PlanFragment, Operator, planpb::PlanNode> {
 public:
  explicit PlanFragment(int64_t id) : id_(id) {}
  static std::unique_ptr<PlanFragment> FromProto(const planpb::PlanFragment& pb, int64_t id);
  int64_t id() const { return id_; }

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
 *     .OnAggregate([&](auto& blocking_agg) {
 *       agg_call_count += 1;
 *     })
 *     .OnMemorySink([&](auto& mem_sink) {
 *       mem_sink_call_count += 1;
 *     })
 *     .Walk(&plan_fragment_);
 */
class PlanFragmentWalker {
 public:
  using MemorySourceWalkFn = std::function<void(const MemorySourceOperator&)>;
  using MapWalkFn = std::function<void(const MapOperator&)>;
  using AggregateWalkFn = std::function<void(const AggregateOperator&)>;
  using MemorySinkWalkFn = std::function<void(const MemorySinkOperator&)>;
  using FilterWalkFn = std::function<void(const FilterOperator&)>;
  using LimitWalkFn = std::function<void(const LimitOperator&)>;
  using UnionWalkFn = std::function<void(const UnionOperator&)>;
  using JoinWalkFn = std::function<void(const JoinOperator&)>;

  /**
   * Register callback for when a memory source operator is encountered.
   * @param fn The function to call when a MemorySourceOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnMemorySource(const MemorySourceWalkFn& fn) {
    on_memory_source_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a map operator is encountered.
   * @param fn The function to call when a MapOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnMap(const MapWalkFn& fn) {
    on_map_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when an aggregate operator is encountered.
   * @param fn The function to call when a AggregateOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnAggregate(const AggregateWalkFn& fn) {
    on_aggregate_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a memory sink operator is encountered.
   * @param fn The function to call when a MemorySinkOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnMemorySink(const MemorySinkWalkFn& fn) {
    on_memory_sink_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a filter operator is encountered.
   * @param fn The function to call when a FilterOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnFilter(const FilterWalkFn& fn) {
    on_filter_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a limit operator is encountered.
   * @param fn The function to call when a LimitOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnLimit(const LimitWalkFn& fn) {
    on_limit_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a union operator is encountered.
   * @param fn The function to call when a UnionOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnUnion(const UnionWalkFn& fn) {
    on_union_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a join operator is encountered.
   * @param fn The function to call when a JoinOperator is encountered.
   * @return self to allow chaining
   */
  PlanFragmentWalker& OnJoin(const JoinWalkFn& fn) {
    on_join_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a walk of the plan fragment operators in a topologically-sorted order.
   * @param plan_fragment The plan fragment to walk.
   */
  void Walk(PlanFragment* plan_fragment);

 private:
  void CallWalkFn(const Operator& op);

  template <typename T, typename TWalkFunc>
  void CallAs(TWalkFunc const& fn, const Operator& op);

  MemorySourceWalkFn on_memory_source_walk_fn_;
  MapWalkFn on_map_walk_fn_;
  AggregateWalkFn on_aggregate_walk_fn_;
  MemorySinkWalkFn on_memory_sink_walk_fn_;
  FilterWalkFn on_filter_walk_fn_;
  LimitWalkFn on_limit_walk_fn_;
  UnionWalkFn on_union_walk_fn_;
  JoinWalkFn on_join_walk_fn_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
