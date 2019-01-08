// Implemetation of a  Directed Acyclic Graph.
// We currently only need support for int's since we just store id's in this graph.
// If needed the DAG implementation can be made generic.
#pragma once

#include <glog/logging.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace pl {
namespace carnot {
namespace plan {

class DAG {
 public:
  void AddNode(int node);
  void DeleteNode(int node);

  bool HasNode(int node);

  void AddEdge(int from_node, int to_node);
  void DeleteEdge(int from_node, int to_node);
  std::string DebugString();
  void Debug();

  std::unordered_set<int> Orphans();
  std::unordered_set<int> TransitiveDepsFrom(int node);
  std::vector<int> TopologicalSort();

  std::vector<int> DependenciesOf(int node) {
    if (nodes_.find(node) != std::end(nodes_)) {
      return forward_edges_by_node_[node];
    }
    return {};
  }

  const std::unordered_set<int>& nodes() { return nodes_; }

 private:
  // Store all the integer id's as nodes.
  std::unordered_set<int> nodes_;

  // This is much more efficient to do in a fstar/rstar structure, but the dual adjacency
  // list is much simpler to update and this is likely not on the critical path.
  // These are the orderered edges from this node to others.
  std::unordered_map<int, std::vector<int>> forward_edges_by_node_;
  // These are ids of the predecesors nodes.
  std::unordered_map<int, std::vector<int>> reverse_edges_by_node_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
