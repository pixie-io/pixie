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

#pragma once

#include <stddef.h>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/dag/dag.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/memory_source_node.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_state.h"
#include "src/common/base/base.h"
#include "src/common/memory/memory.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

struct ExecutionStats {
  int64_t bytes_processed;
  int64_t rows_processed;
};

constexpr std::chrono::milliseconds kDefaultYieldTimeoutMS{1000};
constexpr std::chrono::milliseconds kDefaultUpstreamResultConnectionTimeout{5000};
constexpr int32_t kDefaultConsecutiveGenerateCallsPerSource = 10;
using SystemTimePoint = std::chrono::time_point<std::chrono::system_clock>;

/**
 * An Execution Graph defines the structure of execution nodes for a given plan fragment.
 */
class ExecutionGraph {
 public:
  /**
   * Creates the Execution Graph with the specified timeouts.
   * @param yield_duration When no data is available, but the query is still running, how long to
   * yield before checking for new data points.
   * @param upstream_result_connection_timeout How long to wait for upstream GRPCSources to
   * establish a connection to this node. If this time is surpassed, the query will be cancelled.
   * @return The execution graph.
   */
  ExecutionGraph(const std::chrono::milliseconds& yield_duration,
                 const std::chrono::milliseconds& upstream_result_connection_timeout)
      : upstream_result_connection_timeout_ms_(upstream_result_connection_timeout),
        yield_timeout_ms_(yield_duration) {}

  ExecutionGraph()
      : ExecutionGraph(kDefaultYieldTimeoutMS, kDefaultUpstreamResultConnectionTimeout) {}

  /**
   * Initializes the Execution Graph by initializing the execution nodes and their children.
   * @param schema The schema of available relations.
   * @param compilerState The compiler state.
   * @param execState The execution state.
   * @param pf The plan fragment to create the execution graph from.
   * @param collect_exec_node_stats Whether or not to collect exec node stats.
   * @param consecutive_generate_calls_per_source how many times in a row to call GenerateNext
   * before switching to another available source.
   * @return The status of whether initialization succeeded.
   */
  Status Init(table_store::schema::Schema* schema, plan::PlanState* plan_state,
              ExecState* exec_state, plan::PlanFragment* pf, bool collect_exec_node_stats,
              int32_t consecutive_generate_calls_per_source);

  Status Init(table_store::schema::Schema* schema, plan::PlanState* plan_state,
              ExecState* exec_state, plan::PlanFragment* pf, bool collect_exec_node_stats) {
    return Init(schema, plan_state, exec_state, pf, collect_exec_node_stats,
                kDefaultConsecutiveGenerateCallsPerSource);
  }

  ~ExecutionGraph() {
    // We need to remove these GRPC source nodes from the GRPC router because the exec graph
    // gets destructed so that the GRPC router doesn't have stale pointers to those nodes.
    if (exec_state_->grpc_router() != nullptr) {
      for (int64_t grpc_src_id : grpc_sources()) {
        auto s =
            exec_state_->grpc_router()->DeleteGRPCSourceNode(exec_state_->query_id(), grpc_src_id);
        if (!s.ok()) {
          VLOG(1) << absl::Substitute("Error deleting GRPC Source node $0 for query $1",
                                      grpc_src_id, exec_state_->query_id().str());
        }
      }
    }
  }

  std::vector<int64_t> sources() { return sources_; }
  absl::flat_hash_set<int64_t> grpc_sources() { return grpc_sources_; }

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

  /**
   * Yields the execution of the current graph until Continue() is called or the timeout is reached.
   * @return true if the yield timed out, false if Continue() was called.
   */
  bool YieldWithTimeout();

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

  // Check the upstream connection health for a given GRPC source.
  // If it is not healthy, then we will just omit this particular source from the query,
  // since an input agent may have just been deleted or some other legitimate reason.
  Status CheckUpstreamGRPCConnectionHealth(GRPCSourceNode* source_node);
  // Check the downstream GRPC connections for the query.
  // If it is not healthy, we will cancel the query.
  Status CheckDownstreamGRPCConnectionsHealth();

 private:
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
    PX_ASSIGN_OR_RETURN(auto output_rel, node.OutputRelation(*schema_, *plan_state_, parents));
    table_store::schema::RowDescriptor output_descriptor(output_rel.col_types());
    schema_->AddRelation(node.id(), output_rel);
    descriptors->insert({node.id(), output_descriptor});

    // Create ExecNode.
    auto execNode = pool_.Add(new TNode());
    auto s = execNode->Init(node, output_descriptor, input_descriptors, collect_exec_node_stats_);

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
  ObjectPool pool_{"exec_graph_pool"};
  table_store::schema::Schema* schema_;
  plan::PlanState* plan_state_;
  plan::PlanFragment* pf_;
  std::vector<int64_t> sources_;
  absl::flat_hash_set<int64_t> grpc_sources_;
  absl::flat_hash_set<int64_t> grpc_sinks_;
  std::unordered_map<int64_t, ExecNode*> nodes_;

  SystemTimePoint query_start_time_;

  // How long to wait for any upstream result to make the initial connection to this query.
  std::chrono::milliseconds upstream_result_connection_timeout_ms_;

  // How long to yield during query execution when no data is currently available, before checking
  // for the presence of new data.
  std::chrono::milliseconds yield_timeout_ms_;

  // We alternate round robin style between running sources when calling GenerateNext to ensure
  // that no active sources are starved during a streaming query. This field sets the max number of
  // times in a row to invoke a particular source before moving on to another available source.
  // (Doesn't apply if there is only one active source.)
  int32_t consecutive_generate_calls_per_source_ = kDefaultConsecutiveGenerateCallsPerSource;

  // Whether or not the graph should continue executing or wait for more work to do.
  bool continue_ = false;
  std::mutex execution_mutex_;
  std::condition_variable execution_cv_;
  // Whether to collect stats on exec nodes.
  bool collect_exec_node_stats_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
