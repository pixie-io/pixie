/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <functional>
#include <memory>

#include "src/carnot/dag/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
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
  using MemorySourceWalkFn = std::function<Status(const MemorySourceOperator&)>;
  using MapWalkFn = std::function<Status(const MapOperator&)>;
  using AggregateWalkFn = std::function<Status(const AggregateOperator&)>;
  using MemorySinkWalkFn = std::function<Status(const MemorySinkOperator&)>;
  using FilterWalkFn = std::function<Status(const FilterOperator&)>;
  using LimitWalkFn = std::function<Status(const LimitOperator&)>;
  using UnionWalkFn = std::function<Status(const UnionOperator&)>;
  using JoinWalkFn = std::function<Status(const JoinOperator&)>;
  using GRPCSinkWalkFn = std::function<Status(const GRPCSinkOperator&)>;
  using GRPCSourceWalkFn = std::function<Status(const GRPCSourceOperator&)>;
  using UDTFSourceWalkFn = std::function<Status(const UDTFSourceOperator&)>;
  using EmptySourceWalkFn = std::function<Status(const EmptySourceOperator&)>;
  using OTelSinkWalkFn = std::function<Status(const OTelExportSinkOperator&)>;

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

  PlanFragmentWalker& OnGRPCSource(const GRPCSourceWalkFn& fn) {
    on_grpc_source_walk_fn_ = fn;
    return *this;
  }

  PlanFragmentWalker& OnGRPCSink(const GRPCSinkWalkFn& fn) {
    on_grpc_sink_walk_fn_ = fn;
    return *this;
  }

  PlanFragmentWalker& OnUDTFSource(const UDTFSourceWalkFn& fn) {
    on_udtf_source_walk_fn_ = fn;
    return *this;
  }

  PlanFragmentWalker& OnEmptySource(const EmptySourceWalkFn& fn) {
    on_empty_source_walk_fn_ = fn;
    return *this;
  }

  PlanFragmentWalker& OnOTelSink(const OTelSinkWalkFn& fn) {
    on_otel_sink_walk_fn_ = fn;
    return *this;
  }
  /**
   * Perform a walk of the plan fragment operators in a topologically-sorted order.
   * @param plan_fragment The plan fragment to walk.
   */
  Status Walk(PlanFragment* plan_fragment);

 private:
  Status CallWalkFn(const Operator& op);

  template <typename T, typename TWalkFunc>
  Status CallAs(TWalkFunc const& fn, const Operator& op);

  MemorySourceWalkFn on_memory_source_walk_fn_;
  MapWalkFn on_map_walk_fn_;
  AggregateWalkFn on_aggregate_walk_fn_;
  MemorySinkWalkFn on_memory_sink_walk_fn_;
  FilterWalkFn on_filter_walk_fn_;
  LimitWalkFn on_limit_walk_fn_;
  UnionWalkFn on_union_walk_fn_;
  JoinWalkFn on_join_walk_fn_;
  GRPCSinkWalkFn on_grpc_sink_walk_fn_;
  GRPCSourceWalkFn on_grpc_source_walk_fn_;
  UDTFSourceWalkFn on_udtf_source_walk_fn_;
  EmptySourceWalkFn on_empty_source_walk_fn_;
  OTelSinkWalkFn on_otel_sink_walk_fn_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
