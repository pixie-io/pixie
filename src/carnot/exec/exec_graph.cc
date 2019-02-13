#include <algorithm>
#include <memory>
#include <unordered_map>

#include "src/carnot/exec/blocking_agg_node.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/plan/schema.h"
#include "src/common/object_pool.h"

namespace pl {
namespace carnot {
namespace exec {

Status ExecutionGraph::Init(std::shared_ptr<plan::Schema> schema,
                            std::shared_ptr<plan::CompilerState> compiler_state,
                            std::shared_ptr<ExecState> exec_state,
                            std::shared_ptr<plan::PlanFragment> pf) {
  compiler_state_ = compiler_state;
  schema_ = schema;
  pf_ = pf;
  exec_state_ = exec_state;

  std::unordered_map<int64_t, ExecNode*> nodes;
  std::unordered_map<int64_t, RowDescriptor> descriptors;
  plan::PlanFragmentWalker()
      .OnMap([&](auto& node) {
        return OnOperatorImpl<plan::MapOperator, MapNode>(node, &descriptors);
      })
      .OnMemorySink([&](auto& node) {
        return OnOperatorImpl<plan::MemorySinkOperator, MemorySinkNode>(node, &descriptors);
      })
      .OnBlockingAggregate([&](auto& node) {
        return OnOperatorImpl<plan::BlockingAggregateOperator, BlockingAggNode>(node, &descriptors);
      })
      .OnMemorySource([&](auto& node) {
        sources_.push_back(node.id());
        return OnOperatorImpl<plan::MemorySourceOperator, MemorySourceNode>(node, &descriptors);
      })
      .Walk(pf.get());
  return Status::OK();
}

/**
 * Execute the graph starting at all of the sources.
 * @return a status of whether execution succeeded.
 */
Status ExecutionGraph::Execute() {
  // Get vector of nodes.
  std::vector<ExecNode*> nodes(nodes_.size());
  transform(nodes_.begin(), nodes_.end(), nodes.begin(), [](auto pair) { return pair.second; });

  for (auto node : nodes) {
    PL_RETURN_IF_ERROR(node->Prepare(exec_state_.get()));
  }

  for (auto node : nodes) {
    PL_RETURN_IF_ERROR(node->Open(exec_state_.get()));
  }

  // For each source, generate rowbatches until none are remaining.
  for (auto node_id : sources_) {
    auto node = nodes_.find(node_id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find SourceNode.");
    } else {
      do {
        // TODO(michelle): Determine if there are ways that this can hit deadlock.
        PL_RETURN_IF_ERROR(node->second->GenerateNext(exec_state_.get()));
      } while (static_cast<SourceNode*>(node->second)->ChunksRemaining());
    }
  }

  for (auto node : nodes) {
    PL_RETURN_IF_ERROR(node->Close(exec_state_.get()));
  }

  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
