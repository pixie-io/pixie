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

#ifdef __linux__
#include <cstring>
#include <ctime>

#include <magic_enum.hpp>

#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

Status SourceConnector::Init() {
  if (state_ != State::kUninitialized) {
    return error::Internal("Cannot re-initialize a connector [current state = $0].",
                           magic_enum::enum_name(static_cast<State>(state_)));
  }
  Status s = InitImpl();
  state_ = s.ok() ? State::kActive : State::kErrors;
  return s;
}

void SourceConnector::InitContext(ConnectorContext* ctx) { InitContextImpl(ctx); }

void SourceConnector::TransferData(ConnectorContext* ctx, uint32_t table_num,
                                   DataTable* data_table) {
  DCHECK(ctx != nullptr);
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
  TransferDataImpl(ctx, table_num, data_table);
}

void SourceConnector::TransferData(ConnectorContext* ctx,
                                   const std::vector<DataTable*>& data_tables) {
  DCHECK(ctx != nullptr);
  TransferDataImpl(ctx, data_tables);
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

#endif
