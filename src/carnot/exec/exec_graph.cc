#include <algorithm>
#include <memory>
#include <unordered_map>

#include "src/carnot/exec/blocking_agg_node.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/schema/relation.h"
#include "src/carnot/schema/schema.h"
#include "src/common/memory/memory.h"

namespace pl {
namespace carnot {
namespace exec {

using schema::RowDescriptor;

Status ExecutionGraph::Init(std::shared_ptr<schema::Schema> schema, plan::PlanState* plan_state,
                            ExecState* exec_state, plan::PlanFragment* pf) {
  plan_state_ = plan_state;
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
        sinks_.push_back(node.id());
        return OnOperatorImpl<plan::MemorySinkOperator, MemorySinkNode>(node, &descriptors);
      })
      .OnBlockingAggregate([&](auto& node) {
        return OnOperatorImpl<plan::BlockingAggregateOperator, BlockingAggNode>(node, &descriptors);
      })
      .OnMemorySource([&](auto& node) {
        sources_.push_back(node.id());
        return OnOperatorImpl<plan::MemorySourceOperator, MemorySourceNode>(node, &descriptors);
      })
      .Walk(pf_);
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
    PL_RETURN_IF_ERROR(node->Prepare(exec_state_));
  }

  for (auto node : nodes) {
    PL_RETURN_IF_ERROR(node->Open(exec_state_));
  }

  // For each source, generate rowbatches until none are remaining.
  for (auto node_id : sources_) {
    auto node = nodes_.find(node_id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find SourceNode.");
    }
    if (static_cast<SourceNode*>(node->second)->HasBatchesRemaining()) {
      do {
        // TODO(michelle): Determine if there are ways that this can hit deadlock.
        PL_RETURN_IF_ERROR(node->second->GenerateNext(exec_state_));
      } while (static_cast<SourceNode*>(node->second)->HasBatchesRemaining());
    }
  }

  for (auto node : nodes) {
    PL_RETURN_IF_ERROR(node->Close(exec_state_));
  }

  return Status::OK();
}
std::vector<std::string> ExecutionGraph::OutputTables() const {
  std::vector<std::string> output_tables;
  // Go through the sinks.
  for (int64_t sink_id : sinks_) {
    // Grab the nodes.
    auto res = nodes_.find(sink_id);
    CHECK(res != nodes_.end());
    ExecNode* node = res->second;
    CHECK(node->type() == ExecNodeType::kSinkNode);
    // Grab the names.
    auto sink_node = static_cast<MemorySinkNode*>(node);
    output_tables.push_back(sink_node->TableName());
  }
  return output_tables;
}

ExecutionStats ExecutionGraph::GetStats() const {
  int64_t bytes_processed = 0;
  int64_t rows_processed = 0;
  for (int64_t src_id : sources_) {
    // Grab the nodes.
    auto res = nodes_.find(src_id);
    CHECK(res != nodes_.end());
    ExecNode* node = res->second;
    CHECK(node->type() == ExecNodeType::kSourceNode);
    auto source_node = static_cast<MemorySourceNode*>(node);
    bytes_processed += source_node->BytesProcessed();
    rows_processed += source_node->RowsProcessed();
  }
  return ExecutionStats({bytes_processed, rows_processed});
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
