#include <queue>
#include <stack>
#include <string>
#include <tuple>

#include <iostream>
#include "absl/strings/str_join.h"
#include "src/carnot/plan/dag.h"

namespace pl {
namespace carnot {
namespace plan {

using std::begin;
using std::end;
using std::vector;

void DAG::AddNode(int64_t node) {
  DCHECK(!HasNode(node)) << absl::Substitute("Node: $0 already exists", node);
  nodes_.insert(node);

  forward_edges_by_node_[node] = {};
  reverse_edges_by_node_[node] = {};
}

bool DAG::HasNode(int64_t node) { return nodes_.find(node) != end(nodes_); }

void DAG::DeleteNode(int64_t node) {
  if (!HasNode(node)) {
    LOG(WARNING) << absl::StrCat("Node does not exist: ", node);
  }

  for (auto to_node : forward_edges_by_node_[node]) {
    DeleteEdge(node, to_node);
  }
  for (auto from_node : reverse_edges_by_node_[node]) {
    DeleteEdge(from_node, node);
  }
  nodes_.erase(node);
}

void DAG::AddEdge(int64_t from_node, int64_t to_node) {
  CHECK(HasNode(from_node)) << "from_node does not exist";
  CHECK(HasNode(to_node)) << "to_node does not exist";

  forward_edges_by_node_[from_node].push_back(to_node);
  reverse_edges_by_node_[to_node].push_back(from_node);
}

void DAG::DeleteEdge(int64_t from_node, int64_t to_node) {
  // If there is a dependency we need to delete both the forward and backwards dependency.
  auto& forward_edges = forward_edges_by_node_[from_node];
  const auto& node = std::find(begin(forward_edges), end(forward_edges), to_node);
  if (node != end(forward_edges)) {
    forward_edges.erase(node);
  }

  auto& reverse_edges = reverse_edges_by_node_[to_node];
  const auto& reverse_node = std::find(begin(reverse_edges), end(reverse_edges), from_node);
  if (reverse_node != end(reverse_edges)) {
    reverse_edges.erase(reverse_node);
  }
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
  // Implements Kahn's algorithm:
  // https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm;
  std::vector<int64_t> ordered;
  ordered.reserve(nodes_.size());
  std::queue<int64_t> q;
  std::unordered_map<int64_t, unsigned int> visited_count;

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

  CHECK(ordered.size() == nodes_.size()) << "Cycle detected in graph";
  return ordered;
}

std::string DAG::DebugString() {
  std::string debug_string;
  for (const auto& node : nodes_) {
    debug_string +=
        absl::Substitute("{$0} : [$1]\n", node, absl::StrJoin(forward_edges_by_node_[node], ", "));
  }
  return debug_string;
}

void DAG::Debug() { LOG(INFO) << "DAG Debug: \n" << DebugString(); }

}  // namespace plan
}  // namespace carnot
}  // namespace pl
