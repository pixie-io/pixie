#include "src/carnot/exec/exec_graph.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>

#include "src/carnot/exec/agg_node.h"
#include "src/carnot/exec/equijoin_node.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/filter_node.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/exec/grpc_sink_node.h"
#include "src/carnot/exec/grpc_source_node.h"
#include "src/carnot/exec/limit_node.h"
#include "src/carnot/exec/map_node.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/exec/udtf_source_node.h"
#include "src/carnot/exec/union_node.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_state.h"
#include "src/common/perf/perf.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowDescriptor;

Status ExecutionGraph::Init(std::shared_ptr<table_store::schema::Schema> schema,
                            plan::PlanState* plan_state, ExecState* exec_state,
                            plan::PlanFragment* pf, bool collect_exec_node_stats) {
  plan_state_ = plan_state;
  schema_ = schema;
  pf_ = pf;
  exec_state_ = exec_state;
  collect_exec_node_stats_ = collect_exec_node_stats;

  std::unordered_map<int64_t, ExecNode*> nodes;
  std::unordered_map<int64_t, RowDescriptor> descriptors;
  return plan::PlanFragmentWalker()
      .OnMap([&](auto& node) {
        return OnOperatorImpl<plan::MapOperator, MapNode>(node, &descriptors);
      })
      .OnMemorySink([&](auto& node) {
        sinks_.push_back(node.id());
        return OnOperatorImpl<plan::MemorySinkOperator, MemorySinkNode>(node, &descriptors);
      })
      .OnAggregate([&](auto& node) {
        return OnOperatorImpl<plan::AggregateOperator, AggNode>(node, &descriptors);
      })
      .OnMemorySource([&](auto& node) {
        return OnOperatorImpl<plan::MemorySourceOperator, MemorySourceNode>(node, &descriptors);
      })
      .OnFilter([&](auto& node) {
        return OnOperatorImpl<plan::FilterOperator, FilterNode>(node, &descriptors);
      })
      .OnLimit([&](auto& node) {
        return OnOperatorImpl<plan::LimitOperator, LimitNode>(node, &descriptors);
      })
      .OnUnion([&](auto& node) {
        return OnOperatorImpl<plan::UnionOperator, UnionNode>(node, &descriptors);
      })
      .OnJoin([&](auto& node) {
        return OnOperatorImpl<plan::JoinOperator, EquijoinNode>(node, &descriptors);
      })
      .OnGRPCSource([&](auto& node) {
        auto s = OnOperatorImpl<plan::GRPCSourceOperator, GRPCSourceNode>(node, &descriptors);
        PL_RETURN_IF_ERROR(s);
        grpc_sources_.push_back(node.id());
        return exec_state->grpc_router()->AddGRPCSourceNode(
            exec_state->query_id(), node.id(), static_cast<GRPCSourceNode*>(nodes_[node.id()]),
            std::bind(&ExecutionGraph::Continue, this));
      })
      .OnGRPCSink([&](auto& node) {
        return OnOperatorImpl<plan::GRPCSinkOperator, GRPCSinkNode>(node, &descriptors);
      })
      .OnUDTFSource([&](auto& node) {
        return OnOperatorImpl<plan::UDTFSourceOperator, UDTFSourceNode>(node, &descriptors);
      })
      .Walk(pf_);
}

bool ExecutionGraph::YieldWithTimeout() {
  std::unique_lock<std::mutex> lock(execution_mutex_);
  if (continue_) {
    continue_ = false;
    return false;
  }
  auto timed_out = !(execution_cv_.wait_for(lock, yield_timeout_ms_, [this] { return continue_; }));
  return timed_out;
}

void ExecutionGraph::Continue() {
  std::lock_guard<std::mutex> lock(execution_mutex_);
  continue_ = true;
  execution_cv_.notify_one();
}

Status ExecutionGraph::ExecuteSources() {
  std::set<SourceNode*> running_sources;
  std::unordered_map<SourceNode*, int64_t> source_to_id;
  for (auto node_id : sources_) {
    auto node = nodes_.find(node_id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find SourceNode $0.", node_id);
    }
    SourceNode* n = static_cast<SourceNode*>(node->second);
    running_sources.insert(n);
    source_to_id[n] = node_id;
  }

  while (running_sources.size() > 0) {
    std::vector<SourceNode*> completed_sources;
    for (SourceNode* source : running_sources) {
      exec_state_->SetCurrentSource(source_to_id[source]);
      while (exec_state_->keep_running() && source->NextBatchReady()) {
        PL_RETURN_IF_ERROR(source->GenerateNext(exec_state_));
      }
      if (!source->HasBatchesRemaining() || !exec_state_->keep_running()) {
        completed_sources.push_back(source);
      }
    }

    for (SourceNode* source : completed_sources) {
      running_sources.erase(source);
    }

    if (running_sources.size()) {
      auto timer = ElapsedTimer();
      timer.Start();
      bool timed_out = YieldWithTimeout();
      timer.Stop();
      if (timed_out) {
        LOG(ERROR) << absl::Substitute("Timed out loading source data after $0 ms",
                                       timer.ElapsedTime_us() / 1000.0);
        for (SourceNode* source : running_sources) {
          PL_RETURN_IF_ERROR(source->SendEndOfStream(exec_state_));
        }
        return Status::OK();
      }
    }
  }

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

  PL_RETURN_IF_ERROR(ExecuteSources());

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
    CHECK(res != nodes_.end()) << absl::Substitute("sink_id not found $0", sink_id);
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
