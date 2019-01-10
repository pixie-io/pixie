#pragma once

#include <glog/logging.h>
#include <unordered_set>

#include "src/carnot/plan/dag.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace plan {

template <typename TProto, typename TNode>
class PlanGraph {
 public:
  virtual ~PlanGraph() = default;

  int64_t id() const { return id_; }
  const DAG& dag() const { return dag_; }
  std::unordered_set<TNode> nodes() const { return nodes_; }

  bool is_initialized() const { return is_initialized_; }
  Status Init(const TProto& pb);

 protected:
  DAG dag_;
  std::unordered_set<TNode> nodes_;

  int64_t id_;
  bool is_initialized_ = false;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
