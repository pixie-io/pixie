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

#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"

#include "src/stirling/source_connectors/socket_tracer/http_table.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace testing {

namespace http = protocols::http;
namespace mux = protocols::mux;
namespace mongodb = protocols::mongodb;

//-----------------------------------------------------------------------------
// HTTP Checkers
//-----------------------------------------------------------------------------

template <>
std::vector<http::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                         const std::vector<size_t>& indices) {
  std::vector<http::Record> result;

  for (const auto& idx : indices) {
    http::Record r;
    r.req.req_path = rb[kHTTPReqPathIdx]->Get<types::StringValue>(idx);
    r.req.req_method = rb[kHTTPReqMethodIdx]->Get<types::StringValue>(idx);
    r.req.body = rb[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);

    r.resp.resp_status = rb[kHTTPRespStatusIdx]->Get<types::Int64Value>(idx).val;
    r.resp.resp_message = rb[kHTTPRespMessageIdx]->Get<types::StringValue>(idx);
    r.resp.body = rb[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
    result.push_back(r);
  }
  return result;
}

template <>
std::vector<protocols::mux::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                                   const std::vector<size_t>& indices) {
  std::vector<mux::Record> result;

  for (const auto& idx : indices) {
    mux::Record r;
    r.req.type = static_cast<int8_t>(rb[kMuxReqTypeIdx]->Get<types::Int64Value>(idx).val);
    result.push_back(r);
  }
  return result;
}

template <>
std::vector<mongodb::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                            const std::vector<size_t>& indices) {
  std::vector<mongodb::Record> result;

  for (const auto& idx : indices) {
    mongodb::Record r;
    r.req.op_msg_type = std::string(rb[kMongoDBReqCmdIdx]->Get<types::StringValue>(idx));
    r.req.frame_body = std::string(rb[kMongoDBReqBodyIdx]->Get<types::StringValue>(idx));
    r.resp.op_msg_type = std::string(rb[kMongoDBRespStatusIdx]->Get<types::StringValue>(idx));
    r.resp.frame_body = std::string(rb[kMongoDBRespBodyIdx]->Get<types::StringValue>(idx));
    result.push_back(r);
  }
  return result;
}

template <>
std::vector<http::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                           int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, pid);
  return ToRecordVector<http::Record>(record_batch, target_record_indices);
}

template <>
std::vector<mux::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                          int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kMuxUPIDIdx, pid);
  return ToRecordVector<mux::Record>(record_batch, target_record_indices);
}

template <>
std::vector<mongodb::Record> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                              int32_t pid) {
  std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kMongoDBUPIDIdx, pid);
  return ToRecordVector<mongodb::Record>(record_batch, target_record_indices);
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
