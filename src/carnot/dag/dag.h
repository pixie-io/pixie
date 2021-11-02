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

// Implemetation of a  Directed Acyclic Graph.
// We currently only need support for int's since we just store id's in this graph.
// If needed the DAG implementation can be made generic.
#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

class DAG {
 public:
  /**
   * @brief Optional init from a dag protobuf representation.
   */
  void Init(const planpb::DAG& dag);

  /**
   * @brief writes the protobuf representation of this DAG.
   */
  void ToProto(planpb::DAG* dag) const;

  void ToProto(planpb::DAG* dag, const absl::flat_hash_set<int64_t>& ignore_ids) const;

  void AddNode(int64_t node);
  void DeleteNode(int64_t node);

  bool HasNode(int64_t node) const;
  bool HasEdge(int64_t from_node, int64_t to_node) const;

  void AddEdge(int64_t from_node, int64_t to_node);
  void DeleteEdge(int64_t from_node, int64_t to_node);

  void ReplaceChildEdge(int64_t parent_node, int64_t old_child_node, int64_t new_child_node);
  void ReplaceParentEdge(int64_t child_node, int64_t old_parent_node, int64_t new_parent_node);

  std::string DebugString() const;
  void Debug();

  std::unordered_set<int64_t> Orphans();
  std::unordered_set<int64_t> TransitiveDepsFrom(int64_t node);
  std::vector<int64_t> TopologicalSort() const;

  std::vector<int64_t> DependenciesOf(int64_t node) const {
    if (nodes_.find(node) != std::end(nodes_)) {
      return forward_edges_by_node_.at(node);
    }
    return {};
  }

  std::vector<int64_t> ParentsOf(int64_t node) const {
    if (nodes_.find(node) != std::end(nodes_)) {
      return reverse_edges_by_node_.at(node);
    }
    return {};
  }

  const absl::flat_hash_set<int64_t>& nodes() const { return nodes_; }

 private:
  void AddForwardEdge(int64_t from_node, int64_t to_node);
  void AddReverseEdge(int64_t to_node, int64_t from_node);

  void DeleteParentEdges(int64_t to_node);
  void DeleteDependentEdges(int64_t from_node);

  // Store all the integer id's as nodes.
  absl::flat_hash_set<int64_t> nodes_;

  // This is much more efficient to do in a fstar/rstar structure, but the dual adjacency
  // list is much simpler to update and this is likely not on the critical path.
  // These are the orderered edges from this node to others.
  absl::flat_hash_map<int64_t, std::vector<int64_t>> forward_edges_by_node_;
  // These are ids of the predecesors nodes.
  absl::flat_hash_map<int64_t, std::vector<int64_t>> reverse_edges_by_node_;
  // Used for quick lookups of edges which get really expensive at scale.
  absl::flat_hash_map<int64_t, absl::flat_hash_set<int64_t>> forward_edges_map_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
