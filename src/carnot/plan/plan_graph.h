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

#include <memory>
#include <unordered_map>

#include "src/carnot/dag/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

template <typename TProto, typename TNode, typename TPbNode>
class PlanGraph {
 public:
  virtual ~PlanGraph() = default;
  DAG& dag() { return dag_; }
  std::unordered_map<int64_t, std::unique_ptr<TNode>>& nodes() { return nodes_; }

  bool is_initialized() const { return is_initialized_; }
  Status Init(const TProto& pb) {
    // Add all of the nodes into the DAG.
    dag_.Init(pb.dag());

    for (const auto& node : pb.nodes()) {
      nodes_.emplace(node.id(), PlanGraph::ProtoToNode(node, node.id()));
    }

    is_initialized_ = true;
    return Status::OK();
  }

  static std::unique_ptr<TNode> ProtoToNode(const planpb::PlanFragment& pb, int64_t id) {
    return TNode::FromProto(pb, id);
  }

  static std::unique_ptr<Operator> ProtoToNode(const planpb::PlanNode& pb, int64_t id) {
    return Operator::FromProto(pb.op(), id);
  }

 protected:
  DAG dag_;
  std::unordered_map<int64_t, std::unique_ptr<TNode>> nodes_;

  bool is_initialized_ = false;
};
}  // namespace plan
}  // namespace carnot
}  // namespace px
