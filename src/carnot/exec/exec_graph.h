#pragma once

#include <stddef.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_state.h"
#include "src/common/base/base.h"
#include "src/common/memory/memory.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

struct ExecutionStats {
  int64_t bytes_processed;
  int64_t rows_processed;
};

constexpr std::chrono::milliseconds kDefaultYieldTimeoutMS{100};

/**
 * An Execution Graph defines the structure of execution nodes for a given plan fragment.
 */
class ExecutionGraph {
 public:
  /**
   * Initializes the Execution Graph by initializing the execution nodes and their children.
   * @param schema The schema of available relations.
   * @param compilerState The compiler state.
   * @param execState The execution state.
   * @param pf The plan fragment to create the execution graph from.
   * @return The status of whether initialization succeeded.
   */
  Status Init(std::shared_ptr<table_store::schema::Schema> schema, plan::PlanState* plan_state,
              ExecState* exec_state, plan::PlanFragment* pf);

  std::vector<int64_t> sources() { return sources_; }
  StatusOr<ExecNode*> node(int64_t id) {
    auto node = nodes_.find(id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find ExecNode.");
    }
    return node->second;
  }

  /**
   * Executes the current graph until there is no more work that can be done synchronously.
   */
  Status Execute();
  /**
   * Re-awakens Execute() when there is more work available to do.
   */
  void Continue();

  std::vector<std::string> OutputTables() const;
  ExecutionStats GetStats() const;

  void AddNode(int64_t id, ExecNode* node) {
    nodes_[id] = node;
    if (node->IsSource()) {
      sources_.push_back(id);
    }
  }

  /**
   * For unit testing, set exec_state_ in the cases where the normal Init() hasn't been called.
   */
  void testing_set_exec_state(ExecState* exec_state) { exec_state_ = exec_state; }

 private:
  /**
   * Yields the execution of the current graph until Continue() is called or the timeout is reached.
   */
  bool YieldWithTimeout();

  /**
   * For the given operator type, creates the corresponding execution node and updates the structure
   * of the execution graph.
   * @param node The operator to convert to an execution node.
   * @param nodes A map of node ids to execution nodes in the graph.
   * @param descriptors The descriptors of the execution nodes in the graph.
   * @return A status of whether the initialization of the operator has succeeded.
   */
  template <typename TOp, typename TNode>
  Status OnOperatorImpl(
      TOp node, std::unordered_map<int64_t, table_store::schema::RowDescriptor>* descriptors) {
    std::vector<table_store::schema::RowDescriptor> input_descriptors;

    auto parents = pf_->dag().ParentsOf(node.id());

    // Get input descriptors for this operator.
    for (const int64_t& parent_id : parents) {
      auto input_desc = descriptors->find(parent_id);
      if (input_desc == descriptors->end()) {
        return error::NotFound("Could not find RowDescriptor.");
      }
      input_descriptors.push_back(input_desc->second);
    }
    // Get output descriptor.
    PL_ASSIGN_OR_RETURN(auto output_rel, node.OutputRelation(*schema_, *plan_state_, parents));
    table_store::schema::RowDescriptor output_descriptor(output_rel.col_types());
    // TODO(michelle) (PL-400) causes excessive warnings.
    schema_->AddRelation(node.id(), output_rel);
    descriptors->insert({node.id(), output_descriptor});

    // Create ExecNode.
    auto execNode = pool_.Add(new TNode());
    auto s = execNode->Init(node, output_descriptor, input_descriptors);

    AddNode(node.id(), execNode);

    // Update parents' children.
    for (size_t i = 0; i < parents.size(); ++i) {
      auto parent = nodes_.find(parents[i]);
      // Error if can't find parent in nodes.
      if (parent == nodes_.end()) {
        return error::NotFound("Could not find parent ExecNode.");
      }
      parent->second->AddChild(execNode, i);
    }
    return Status::OK();
  }

  Status ExecuteSources();

  ExecState* exec_state_;
  ObjectPool pool_;
  std::shared_ptr<table_store::schema::Schema> schema_;
  plan::PlanState* plan_state_;
  plan::PlanFragment* pf_;
  std::vector<int64_t> sources_;
  std::vector<int64_t> sinks_;
  std::unordered_map<int64_t, ExecNode*> nodes_;

  // How long in milliseconds to wait on a node when there is no other work to do.
  std::chrono::milliseconds yield_timeout_ms_{kDefaultYieldTimeoutMS};
  // Whether or not the graph should continue executing or wait for more work to do.
  bool continue_ = false;
  std::mutex execution_mutex_;
  std::condition_variable execution_cv_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
