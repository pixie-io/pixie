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

#include <cstring>
#include <ctime>
#include <memory>

#include <magic_enum.hpp>

#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

Status SourceConnector::Init() {
  if (state_ != State::kUninitialized) {
    return error::Internal("Cannot re-initialize a connector [current state = $0].",
                           magic_enum::enum_name(static_cast<State>(state_)));
  }
  LOG(INFO) << absl::Substitute("Initializing source connector: $0", name());
  Status s = InitImpl();
  state_ = s.ok() ? State::kActive : State::kErrors;

  DCHECK_NE(sampling_freq_mgr_.period().count(), 0) << "Sampling period has not been initialized";
  DCHECK_NE(push_freq_mgr_.period().count(), 0) << "Push period has not been initialized";

  return s;
}

void SourceConnector::InitContext(ConnectorContext* ctx) { InitContextImpl(ctx); }

void SourceConnector::TransferData(ConnectorContext* ctx) {
  DCHECK(ctx != nullptr);
  DCHECK_EQ(data_tables_.size(), table_schemas().size())
      << "DataTable objects must all be specified.";
  TransferDataImpl(ctx);
}

void SourceConnector::PushData(DataPushCallback agent_callback) {
  for (auto* data_table : data_tables_) {
    auto record_batches = data_table->ConsumeRecords();
    for (auto& record_batch : record_batches) {
      if (record_batch.records.empty()) {
        continue;
      }
      Status s = agent_callback(
          data_table->id(), record_batch.tablet_id,
          std::make_unique<types::ColumnWrapperRecordBatch>(std::move(record_batch.records)));
      LOG_IF(DFATAL, !s.ok()) << absl::Substitute("Failed to push data. Message = $0", s.msg());
    }
  }
}

Status SourceConnector::Stop() {
  if (state_ != State::kActive) {
    return Status::OK();
  }

  // Update state first, so that StopImpl() can act accordingly.
  // For example, SocketTraceConnector::AttachHTTP2probesLoop() exists loop when state_ is
  // kStopped; and SocketTraceConnector::StopImpl() joins the thread.
  state_ = State::kStopped;
  Status s = StopImpl();
  if (!s.ok()) {
    state_ = State::kErrors;
  }
  return s;
}

}  // namespace stirling
}  // namespace px
