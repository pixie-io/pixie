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

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

#include "blockingconcurrentqueue.h"

namespace px {
namespace carnot {
namespace exec {

class GRPCSourceNode : public SourceNode {
 public:
  GRPCSourceNode() = default;
  virtual ~GRPCSourceNode() = default;

  bool NextBatchReady() override;
  virtual Status EnqueueRowBatch(std::unique_ptr<carnotpb::TransferResultChunkRequest> row_batch);

  // Tracks whether the upstream sink node has successfully initiated the connection to
  // this remote source. Used by the exec graph to determine whether or not any sources have
  // taken too long for their connection to be established with the sinks.
  void set_upstream_initiated_connection() { upstream_initiated_connection_ = true; }
  bool upstream_initiated_connection() const { return upstream_initiated_connection_; }

  // Tracks whether the upstream sink node has closed or cancelled the connection to
  // this remote source. Used by the exec graph to determine whether or not any sources have
  // unexpectedly had their connections closed with their remote sinks.
  void set_upstream_closed_connection() { upstream_closed_connection_ = true; }
  bool upstream_closed_connection() const { return upstream_closed_connection_; }

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  Status PopRowBatch();

  std::unique_ptr<table_store::schema::RowBatch> rb_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<carnotpb::TransferResultChunkRequest>>
      row_batch_queue_;

  std::unique_ptr<plan::GRPCSourceOperator> plan_node_;
  bool upstream_initiated_connection_ = false;
  bool upstream_closed_connection_ = false;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
