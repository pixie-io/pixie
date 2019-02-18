#pragma once

#include <glog/logging.h>
#include <memory>
#include <unordered_map>

#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/common/status.h"

namespace pl {
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
    for (const auto& node : pb.dag().nodes()) {
      dag_.AddNode(node.id());
    }

    // Add all of the edges into the DAG.
    for (const auto& node : pb.dag().nodes()) {
      for (int64_t to_node : node.sorted_deps()) {
        dag_.AddEdge(node.id(), to_node);
      }
    }

    for (const auto& node : pb.nodes()) {
      nodes_.emplace(node.id(), PlanGraph::ProtoToNode(node, node.id()));
    }

    is_initialized_ = true;
    return Status::OK();
  }

  static std::unique_ptr<TNode> ProtoToNode(const carnotpb::PlanFragment& pb, int64_t id) {
    return TNode::FromProto(pb, id);
  }

  static std::unique_ptr<Operator> ProtoToNode(const carnotpb::PlanNode& pb, int64_t id) {
    return Operator::FromProto(pb.op(), id);
  }

 protected:
  DAG dag_;
  std::unordered_map<int64_t, std::unique_ptr<TNode>> nodes_;

  bool is_initialized_ = false;
};
}  // namespace plan
}  // namespace carnot
}  // namespace pl
