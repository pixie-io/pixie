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

#include "src/carnot/dag/dag.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <stack>
#include <string>
#include <tuple>
#include <utility>

#include <absl/hash/hash.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace plan {

using std::begin;
using std::end;
using std::vector;

void DAG::Init(const planpb::DAG& dag) {
  for (const auto& node : dag.nodes()) {
    AddNode(node.id());
    for (int64_t child : node.sorted_children()) {
      forward_edges_by_node_[node.id()].push_back(child);
      forward_edges_map_[node.id()].insert(child);
    }
    for (int64_t parent : node.sorted_parents()) {
      reverse_edges_by_node_[node.id()].push_back(parent);
    }
  }
}

void DAG::ToProto(planpb::DAG* dag) const {
  for (int64_t i : TopologicalSort()) {
    planpb::DAG_DAGNode* node = dag->add_nodes();
    node->set_id(i);
    auto reverse_it = reverse_edges_by_node_.find(i);
    DCHECK(reverse_it != reverse_edges_by_node_.end());
    for (int64_t parent : reverse_it->second) {
      node->add_sorted_parents(parent);
    }

    auto forward_it = forward_edges_by_node_.find(i);
    DCHECK(forward_it != forward_edges_by_node_.end());
    for (int64_t child : forward_it->second) {
      node->add_sorted_children(child);
    }
  }
}

void DAG::ToProto(planpb::DAG* dag, const absl::flat_hash_set<int64_t>& ignore_ids) const {
  for (int64_t i : TopologicalSort()) {
    if (ignore_ids.contains(i)) {
      continue;
    }
    planpb::DAG_DAGNode* node = dag->add_nodes();
    node->set_id(i);
    auto reverse_it = reverse_edges_by_node_.find(i);
    DCHECK(reverse_it != reverse_edges_by_node_.end());
    for (int64_t parent : reverse_it->second) {
      if (!ignore_ids.contains(parent)) {
        node->add_sorted_parents(parent);
      }
    }

    auto forward_it = forward_edges_by_node_.find(i);
    DCHECK(forward_it != forward_edges_by_node_.end());
    for (int64_t child : forward_it->second) {
      if (!ignore_ids.contains(child)) {
        node->add_sorted_children(child);
      }
    }
  }
}

void DAG::AddNode(int64_t node) {
  DCHECK(!HasNode(node)) << absl::Substitute("Node: $0 already exists", node);
  nodes_.insert(node);

  forward_edges_by_node_[node] = {};
  reverse_edges_by_node_[node] = {};
}

bool DAG::HasNode(int64_t node) const { return nodes_.find(node) != end(nodes_); }

void DAG::DeleteNode(int64_t node) {
  if (!HasNode(node)) {
    LOG(WARNING) << absl::StrCat("Node does not exist: ", node);
  }

  DeleteParentEdges(node);
  DeleteDependentEdges(node);

  nodes_.erase(node);
}

void DAG::AddEdge(int64_t from_node, int64_t to_node) {
  CHECK(HasNode(from_node)) << absl::Substitute("from_node $0 does not exist", from_node);
  CHECK(HasNode(to_node)) << absl::Substitute("to_node $0 does not exist", to_node);

  AddForwardEdge(from_node, to_node);
  AddReverseEdge(to_node, from_node);
}

void DAG::AddForwardEdge(int64_t from_node, int64_t to_node) {
  DCHECK(std::find(forward_edges_by_node_[from_node].begin(),
                   forward_edges_by_node_[from_node].end(),
                   to_node) == forward_edges_by_node_[from_node].end())
      << absl::Substitute("Forward edge from $0 to $1 already exists", from_node, to_node);
  forward_edges_by_node_[from_node].push_back(to_node);
  // Add to the forward edges map.
  forward_edges_map_[from_node].insert(to_node);
}

void DAG::AddReverseEdge(int64_t to_node, int64_t from_node) {
  DCHECK(std::find(reverse_edges_by_node_[to_node].begin(), reverse_edges_by_node_[to_node].end(),
                   from_node) == reverse_edges_by_node_[to_node].end())
      << absl::Substitute("Reverse edge to $0 from $1 already exists", to_node, from_node);
  reverse_edges_by_node_[to_node].push_back(from_node);
}

void DAG::DeleteParentEdges(int64_t to_node) {
  // Iterate through all of the parents of to_node and delete the edges.
  auto& reverse_edges = reverse_edges_by_node_[to_node];
  auto parent_iter = reverse_edges.begin();
  while (parent_iter != reverse_edges.end()) {
    // Find the forward edge for the specific parent of to_node.
    auto& forward_edges = forward_edges_by_node_[*parent_iter];
    const auto& node = std::find(begin(forward_edges), end(forward_edges), to_node);
    if (node != end(forward_edges)) {
      // Delete parent->to_node edge.
      forward_edges.erase(node);
    }

    // Erase points to the next valid iterator.
    // Delete to_node->parent edge.
    parent_iter = reverse_edges.erase(parent_iter);
    if (parent_iter == reverse_edges.end()) {
      break;
    }
    // Remove the entry from the map for each parent of the edge.
    forward_edges_map_[*parent_iter].erase(to_node);
  }
}

void DAG::DeleteDependentEdges(int64_t from_node) {
  // Iterate through all of the dependents of from_node and delete the edges.
  auto& forward_edges = forward_edges_by_node_[from_node];
  auto child_iter = forward_edges.begin();
  while (child_iter != forward_edges.end()) {
    // Find the reverse edge for the specific dependent of from_node.
    auto& reverse_edges = reverse_edges_by_node_[*child_iter];
    const auto& node = std::find(begin(reverse_edges), end(reverse_edges), from_node);
    if (node != end(reverse_edges)) {
      // Delete dependent->from_node edge.
      reverse_edges.erase(node);
    }

    // Erase points to the next valid iterator.
    // Delete from_node->dependent edge.
    child_iter = forward_edges.erase(child_iter);
  }
  // Remove the entry from the edge map.
  forward_edges_map_.erase(from_node);
}

void DAG::DeleteEdge(int64_t from_node, int64_t to_node) {
  // If there is a dependency we need to delete both the forward and backwards dependency.
  auto& forward_edges = forward_edges_by_node_[from_node];
  const auto& node = std::find(begin(forward_edges), end(forward_edges), to_node);
  if (node != end(forward_edges)) {
    forward_edges.erase(node);
  }

  // Update the edge map. Must remake the hash set because we cannot simply remove the old edge and
  // add the new one as there might be duplicate edges before the replacement.
  forward_edges_map_[from_node] =
      absl::flat_hash_set<int64_t>(forward_edges.begin(), forward_edges.end());

  auto& reverse_edges = reverse_edges_by_node_[to_node];
  const auto& reverse_node = std::find(begin(reverse_edges), end(reverse_edges), from_node);
  if (reverse_node != end(reverse_edges)) {
    reverse_edges.erase(reverse_node);
  }
}

void DAG::ReplaceChildEdge(int64_t parent_node, int64_t old_child_node, int64_t new_child_node) {
  // If there is a dependency we need to delete both the forward and backwards dependency.
  CHECK(HasNode(parent_node)) << "from_node does not exist";
  CHECK(HasNode(old_child_node)) << "old_child_node does not exist";
  CHECK(HasNode(new_child_node)) << "new_child_node does not exist";
  auto& forward_edges = forward_edges_by_node_[parent_node];

  // Repalce the old_child_node with the new_child_node in the forward edge.
  std::replace(forward_edges.begin(), forward_edges.end(), old_child_node, new_child_node);

  // Update the edge map. Must remake the hash set because we cannot simply remove the old edge and
  // add the new one as there might be duplicate edges before the replacement.
  forward_edges_map_[parent_node] =
      absl::flat_hash_set<int64_t>(forward_edges.begin(), forward_edges.end());

  // Remove the old reverse edge (old_child_node, parent_node)
  auto& reverse_edges = reverse_edges_by_node_[old_child_node];
  const auto& reverse_node = std::find(begin(reverse_edges), end(reverse_edges), parent_node);
  if (reverse_node != end(reverse_edges)) {
    reverse_edges.erase(reverse_node);
  }

  // Add the new reverse edge (new_child_node, from_node)
  AddReverseEdge(new_child_node, parent_node);
}

void DAG::ReplaceParentEdge(int64_t child_node, int64_t old_parent_node, int64_t new_parent_node) {
  // If there is a dependency we need to delete both the forward and backwards dependency.
  CHECK(HasNode(child_node)) << "child_node does not exist";
  CHECK(HasNode(old_parent_node)) << "old_parent_node does not exist";
  CHECK(HasNode(new_parent_node)) << "new_parent_node does not exist";
  auto& reverse_edges = reverse_edges_by_node_[child_node];

  // Repalce the old_from_node with the new_from_node.
  std::replace(reverse_edges.begin(), reverse_edges.end(), old_parent_node, new_parent_node);

  // Remove the old forward edge (old_from_node, child_node)
  auto& forward_edges = forward_edges_by_node_[old_parent_node];
  const auto& forward_node = std::find(begin(forward_edges), end(forward_edges), child_node);
  if (forward_node != end(forward_edges)) {
    forward_edges.erase(forward_node);
  }

  // Update the edge map. Must remake the hash set because we cannot simply remove the old edge and
  // add the new one as there might be duplicate edges before the replacement.
  forward_edges_map_[old_parent_node] =
      absl::flat_hash_set<int64_t>(forward_edges.begin(), forward_edges.end());

  // Add the new forward edge (new_from_node, child_node)
  AddForwardEdge(new_parent_node, child_node);
}

bool DAG::HasEdge(int64_t from_node, int64_t to_node) const {
  const auto& iter = forward_edges_map_.find(from_node);
  return iter != forward_edges_map_.end() && iter->second.contains(to_node);
}

std::unordered_set<int64_t> DAG::TransitiveDepsFrom(int64_t node) {
  enum VisitStatus { kVisitStarted, kVisitComplete };
  enum NodeColor { kWhite = 0, kGray, kBlack };

  // The visit status related to if we started or completed the visit,
  // the int tracks the node id.
  std::stack<std::tuple<VisitStatus, int64_t>> s;
  std::unordered_set<int64_t> dep_list;
  std::unordered_map<int64_t, NodeColor> colors;

  s.emplace(std::tuple(kVisitStarted, node));

  while (!s.empty()) {
    auto [status, top_node] = s.top();  // NOLINT (cpplint bug)
    s.pop();

    if (status == kVisitComplete) {
      colors[top_node] = kBlack;
    } else {
      colors[top_node] = kGray;
      s.emplace(std::tuple(kVisitComplete, top_node));
      for (auto dep : DependenciesOf(top_node)) {
        CHECK(colors[dep] != kGray) << "Cycle found";
        if (colors[dep] == kWhite) {
          s.emplace(std::tuple(kVisitStarted, dep));
          dep_list.insert(dep);
        }
      }
    }
  }
  return dep_list;
}

std::unordered_set<int64_t> DAG::Orphans() {
  std::unordered_set<int64_t> orphans;
  for (const auto& node : nodes_) {
    if (forward_edges_by_node_[node].empty() && reverse_edges_by_node_[node].empty()) {
      orphans.insert(node);
    }
  }
  return orphans;
}

vector<int64_t> DAG::TopologicalSort() const {
  if (!nodes_.size()) {
    return {};
  }

  // Implements Kahn's algorithm:
  // https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm;
  std::vector<int64_t> ordered;
  ordered.reserve(nodes_.size());
  std::queue<int64_t> q;
  absl::flat_hash_map<int64_t, unsigned int> visited_count;

  // Find nodes that don't have any incoming edges.
  for (auto node : nodes_) {
    if (reverse_edges_by_node_.at(node).empty()) {
      q.push(node);
    }
  }

  CHECK(!q.empty()) << "No nodes without incoming edges, likely a cycle";

  while (!q.empty()) {
    int front_val = q.front();
    q.pop();
    ordered.push_back(front_val);

    for (auto dep : forward_edges_by_node_.at(front_val)) {
      visited_count[dep]++;
      if (visited_count.at(dep) == reverse_edges_by_node_.at(dep).size()) {
        q.push(dep);
      }
    }
  }

  CHECK_EQ(ordered.size(), nodes_.size()) << "Cycle detected in graph";
  return ordered;
}

std::string DAG::DebugString() const {
  std::string debug_string;
  for (const auto& node : nodes_) {
    debug_string += absl::Substitute("{$0} : [$1]\n", node,
                                     absl::StrJoin(forward_edges_by_node_.at(node), ", "));
  }
  return debug_string;
}

void DAG::Debug() { LOG(INFO) << "DAG Debug: \n" << DebugString(); }

}  // namespace plan
}  // namespace carnot
}  // namespace px
