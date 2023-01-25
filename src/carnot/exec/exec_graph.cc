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

#include "src/carnot/exec/exec_graph.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_map>

#include "src/carnot/exec/agg_node.h"
#include "src/carnot/exec/empty_source_node.h"
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
#include "src/carnot/exec/otel_export_sink_node.h"
#include "src/carnot/exec/udtf_source_node.h"
#include "src/carnot/exec/union_node.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan_state.h"
#include "src/common/perf/perf.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowDescriptor;

Status ExecutionGraph::Init(table_store::schema::Schema* schema, plan::PlanState* plan_state,
                            ExecState* exec_state, plan::PlanFragment* pf,
                            bool collect_exec_node_stats,
                            int32_t consecutive_generate_calls_per_source) {
  plan_state_ = plan_state;
  schema_ = schema;
  pf_ = pf;
  exec_state_ = exec_state;
  collect_exec_node_stats_ = collect_exec_node_stats;
  consecutive_generate_calls_per_source_ = consecutive_generate_calls_per_source;

  std::unordered_map<int64_t, ExecNode*> nodes;
  std::unordered_map<int64_t, RowDescriptor> descriptors;
  return plan::PlanFragmentWalker()
      .OnMap([&](auto& node) {
        return OnOperatorImpl<plan::MapOperator, MapNode>(node, &descriptors);
      })
      .OnMemorySink([&](auto& node) {
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
        PX_RETURN_IF_ERROR(s);
        grpc_sources_.insert(node.id());
        return exec_state->grpc_router()->AddGRPCSourceNode(
            exec_state->query_id(), node.id(), static_cast<GRPCSourceNode*>(nodes_[node.id()]),
            std::bind(&ExecutionGraph::Continue, this));
      })
      .OnGRPCSink([&](auto& node) {
        grpc_sinks_.insert(node.id());
        return OnOperatorImpl<plan::GRPCSinkOperator, GRPCSinkNode>(node, &descriptors);
      })
      .OnUDTFSource([&](auto& node) {
        return OnOperatorImpl<plan::UDTFSourceOperator, UDTFSourceNode>(node, &descriptors);
      })
      .OnEmptySource([&](auto& node) {
        return OnOperatorImpl<plan::EmptySourceOperator, EmptySourceNode>(node, &descriptors);
      })
      .OnOTelSink([&](auto& node) {
        return OnOperatorImpl<plan::OTelExportSinkOperator, OTelExportSinkNode>(node, &descriptors);
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
  {
    std::lock_guard<std::mutex> lock(execution_mutex_);
    continue_ = true;
  }
  execution_cv_.notify_one();
}

Status ExecutionGraph::CheckUpstreamGRPCConnectionHealth(GRPCSourceNode* source_node) {
  // Note: for the following logic, HasBatchesRemaining is equivalent to whether or not
  // the source node has sent a final end of stream row batch already or not.
  // It's ok if the connection is closed if the GRPC source sent EOS, but if the EOS
  // is still pending and the connection has been closed, that's an error.
  // If there are queued row batches to this GRPC source, they may contain the EOS,
  // so don't produce an error if the GRPC source has batches ready.
  if (!source_node->NextBatchReady() && source_node->HasBatchesRemaining() &&
      source_node->upstream_closed_connection()) {
    return error::Internal(
        "Error executing query $0: GRPC connection to source node closed its connection",
        exec_state_->query_id().str());
  }

  if (source_node->upstream_initiated_connection()) {
    return Status::OK();
  }

  SystemTimePoint now = std::chrono::system_clock::now();
  auto delta = now - query_start_time_;
  // Wait a certain amount of time before determining the connection has taken too long to
  // establish.
  if (delta < upstream_result_connection_timeout_ms_) {
    return Status::OK();
  }

  return error::DeadlineExceeded(
      "Error executing query $0: GRPC connection for source node $1 took longer than $2 ms to "
      "establish a connection",
      exec_state_->query_id().str(), source_node->DebugString(),
      upstream_result_connection_timeout_ms_.count());
}

Status ExecutionGraph::CheckDownstreamGRPCConnectionsHealth() {
  for (const auto& grpc_sink_id : grpc_sinks_) {
    auto node = nodes_.find(grpc_sink_id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find GRPCSinkNode $0.", grpc_sink_id);
    }
    GRPCSinkNode* grpc_sink = static_cast<GRPCSinkNode*>(node->second);
    PX_RETURN_IF_ERROR(grpc_sink->OptionallyCheckConnection(exec_state_));
  }
  return Status::OK();
}

Status ExecutionGraph::ExecuteSources() {
  absl::flat_hash_set<SourceNode*> running_sources;

  absl::flat_hash_map<SourceNode*, int64_t> source_to_id;
  for (auto node_id : sources_) {
    auto node = nodes_.find(node_id);
    if (node == nodes_.end()) {
      return error::NotFound("Could not find SourceNode $0.", node_id);
    }
    SourceNode* n = static_cast<SourceNode*>(node->second);
    running_sources.insert(n);
    source_to_id[n] = node_id;
  }

  // Run all sources to completion, or exit if the query encounters an error.
  while (running_sources.size()) {
    absl::flat_hash_set<SourceNode*> completed_sources_execute_loop;

    for (SourceNode* source : running_sources) {
      if (grpc_sources_.contains(source_to_id.at(source))) {
        auto s = CheckUpstreamGRPCConnectionHealth(static_cast<GRPCSourceNode*>(source));
        if (!s.ok()) {
          LOG(ERROR) << absl::Substitute(
              "GRPCSourceNode connection to remote sink not healthy, terminating that source and "
              "proceeding with the rest of the query. Message: $0",
              s.msg());
          PX_RETURN_IF_ERROR(source->SendEndOfStream(exec_state_));
          completed_sources_execute_loop.insert(source);
          continue;
        }
      }

      exec_state_->SetCurrentSource(source_to_id[source]);

      for (auto i = 0; i < consecutive_generate_calls_per_source_; ++i) {
        if (!source->NextBatchReady() || !exec_state_->keep_running()) {
          break;
        }
        PX_RETURN_IF_ERROR(source->GenerateNext(exec_state_));
      }

      // keep_running will be set to false when a downstream limit for this particular
      // source (set in exec_state) has been reached.
      if (!source->HasBatchesRemaining() || !exec_state_->keep_running()) {
        completed_sources_execute_loop.insert(source);
        break;
      }
    }
    PX_RETURN_IF_ERROR(CheckDownstreamGRPCConnectionsHealth());

    // Flush all of the completed sources.
    for (SourceNode* source : completed_sources_execute_loop) {
      running_sources.erase(source);
    }

    // If all sources are complete, the query is done executing.
    if (!running_sources.size()) {
      break;
    }

    // For all running sources, check to see if any of them have data
    // or if we need to yield for more data.
    bool wait_for_more_data = true;
    for (SourceNode* source : running_sources) {
      if (source->NextBatchReady()) {
        wait_for_more_data = false;
        break;
      }
    }

    while (wait_for_more_data) {
      auto timer = ElapsedTimer();
      timer.Start();
      YieldWithTimeout();
      timer.Stop();

      absl::flat_hash_set<SourceNode*> completed_sources_wait_loop;

      // This check is used for Memory sources that are waiting on data, because we don't currently
      // have a mechanism to call Yield() on them while they are waiting.
      // Once we introduce Carnot ETL, we can have the ingest phase of Carnot ETL call yield.
      for (SourceNode* source : running_sources) {
        if (source->NextBatchReady()) {
          wait_for_more_data = false;
        }
        // Check the upstream connection health of all running GRPC sources after each yield.
        if (grpc_sources_.contains(source_to_id.at(source))) {
          auto s = CheckUpstreamGRPCConnectionHealth(static_cast<GRPCSourceNode*>(source));
          if (!s.ok()) {
            LOG(ERROR) << absl::Substitute(
                "GRPCSourceNode connection to remote sink not healthy, terminating that source and "
                "proceeding with the rest of the query. Message: $0",
                s.msg());
            PX_RETURN_IF_ERROR(source->SendEndOfStream(exec_state_));
            completed_sources_wait_loop.insert(source);
            continue;
          }
        }
      }
      PX_RETURN_IF_ERROR(CheckDownstreamGRPCConnectionsHealth());

      // Flush all of the completed sources after this phase of source deletion.
      for (SourceNode* source : completed_sources_wait_loop) {
        running_sources.erase(source);
      }
      if (!running_sources.size()) {
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
  query_start_time_ = std::chrono::system_clock::now();

  // Get vector of nodes.
  std::vector<ExecNode*> nodes(nodes_.size());
  transform(nodes_.begin(), nodes_.end(), nodes.begin(), [](auto pair) { return pair.second; });

  for (auto node : nodes) {
    PX_RETURN_IF_ERROR(node->Prepare(exec_state_));
  }

  for (auto node : nodes) {
    PX_RETURN_IF_ERROR(node->Open(exec_state_));
  }

  // We don't PX_RETURN_IF_ERROR here because we want to make sure we close all of our
  // nodes, even if there was an error during execution.
  Status source_status = ExecuteSources();
  Status close_status = Status::OK();

  for (auto node : nodes) {
    auto s = node->Close(exec_state_);
    if (!s.ok()) {
      // Since we only return a single error status if there are multiple errors,
      // make sure to log all of the errors that we receive as we close down the query.
      LOG(ERROR) << absl::Substitute(
          "Error in ExecutionGraph::Execute() for query $0, could not close source: $1",
          exec_state_->query_id().str(), s.msg());
      close_status = s;
    }
  }

  if (!source_status.ok()) {
    return source_status;
  }
  return close_status;
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
    auto source_node = static_cast<SourceNode*>(node);
    bytes_processed += source_node->BytesProcessed();
    rows_processed += source_node->RowsProcessed();
  }
  return ExecutionStats({bytes_processed, rows_processed});
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
