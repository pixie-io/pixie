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
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

#include "src/carnot/carnotpb/carnot.grpc.pb.h"

namespace px {
namespace carnot {
namespace exec {

constexpr std::chrono::milliseconds kDefaultConnectionCheckTimeoutMS{2000};
// Max request size is 1MB minus 16KB (about 1%) to account for differences between the public
// and private query result data structure size. For example, an extra string field for table ID
// is added to the public query result data structure.
constexpr size_t kMetadataMargin = 16 * 1024;
constexpr size_t kMaxBatchSize = 1024 * 1024 - kMetadataMargin;
// BatchSizeFactor is used to leave some room for encryption to increase the size of batches.
constexpr float kBatchSizeFactor = 0.9f;

// Number of times to retry connecting to grpc before giving up.
constexpr size_t kGRPCRetries = 3;

class GRPCSinkNode : public SinkNode {
 public:
  GRPCSinkNode(size_t max_batch_size, float batch_size_factor)
      : max_batch_size_(max_batch_size), batch_size_factor_(batch_size_factor) {}
  GRPCSinkNode() : GRPCSinkNode(kMaxBatchSize, kBatchSizeFactor) {}
  virtual ~GRPCSinkNode() = default;

  // Used to check the downstream connection after connection_check_timeout_ has elapsed.
  Status OptionallyCheckConnection(ExecState* exec_state);

  void testing_set_connection_check_timeout(const std::chrono::milliseconds& timeout) {
    connection_check_timeout_ = timeout;
  }
  const std::chrono::time_point<std::chrono::system_clock>& testing_last_send_time() const {
    return last_send_time_;
  }

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;
  Status ConsumeNextImplNoSplit(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                                size_t parent_index);
  Status SplitAndSendBatch(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                           size_t parent_index);
  std::vector<int64_t> SplitBatchSizes(bool has_string_col,
                                       const std::vector<int64_t>& string_col_row_sizes,
                                       int64_t other_col_row_size) const;

 private:
  Status CloseWriter(ExecState* exec_state);
  Status StartConnection(ExecState* exec_state);
  Status StartConnectionWithRetries(ExecState* exec_state, size_t n_retries);
  Status CancelledByServer(ExecState* exec_state);
  Status TryWriteRequest(ExecState* exec_state, const carnotpb::TransferResultChunkRequest& req);

  bool cancelled_ = false;

  std::unique_ptr<grpc::ClientContext> context_;
  carnotpb::TransferResultChunkResponse response_;

  carnotpb::ResultSinkService::StubInterface* stub_;
  std::unique_ptr<grpc::ClientWriterInterface<carnotpb::TransferResultChunkRequest>> writer_;

  std::unique_ptr<plan::GRPCSinkOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;

  std::chrono::milliseconds connection_check_timeout_ = kDefaultConnectionCheckTimeoutMS;
  std::chrono::time_point<std::chrono::system_clock> last_send_time_;

  size_t max_batch_size_;
  float batch_size_factor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
