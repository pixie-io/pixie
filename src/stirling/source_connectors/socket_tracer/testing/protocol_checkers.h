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

#include <string>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/shared/types/column_wrapper.h"
#include "src/stirling/source_connectors/socket_tracer/http_table.h"
#include "src/stirling/source_connectors/socket_tracer/mongodb_table.h"
#include "src/stirling/source_connectors/socket_tracer/mux_table.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"

namespace px {
namespace stirling {
namespace testing {

//-----------------------------------------------------------------------------
// HTTP Checkers
//-----------------------------------------------------------------------------
template <typename TFrameType>
std::vector<TFrameType> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                       const std::vector<size_t>& indices);

template <typename TFrameType>
std::vector<TFrameType> GetTargetRecords(const types::ColumnWrapperRecordBatch& record_batch,
                                         int32_t pid);

template <typename Record, typename... Records>
typename std::enable_if<(std::is_same<Records, Record>::value && ...), void>::type
ContainsWithRelativeOrder(std::vector<Record> actual_records, Records... expected_records) {
  using ::testing::Contains;

  std::vector<size_t> record_indices;
  for (const Record& record : {expected_records...}) {
    EXPECT_THAT(actual_records, Contains(record));

    // No need to check that it matches since previous assert will cover that
    auto result = std::find(begin(actual_records), end(actual_records), record);
    record_indices.push_back(std::distance(actual_records.begin(), result));
  }

  auto start = std::begin(record_indices);
  auto end = std::end(record_indices);
  auto is_sorted = std::is_sorted(start, end);
  auto sorted_until = std::is_sorted_until(start, end);

  EXPECT_TRUE(is_sorted) << "Element not in sorted order: " << actual_records[*sorted_until];
}

inline auto EqHTTPReq(const protocols::http::Message& x) {
  using ::testing::Field;

  return AllOf(Field(&protocols::http::Message::req_path, ::testing::Eq(x.req_path)),
               Field(&protocols::http::Message::req_method, ::testing::StrEq(x.req_method)),
               Field(&protocols::http::Message::body, ::testing::StrEq(x.body)));
}

inline auto EqHTTPResp(const protocols::http::Message& x) {
  using ::testing::Field;

  return AllOf(Field(&protocols::http::Message::resp_status, ::testing::Eq(x.resp_status)),
               Field(&protocols::http::Message::resp_message, ::testing::StrEq(x.resp_message)),
               Field(&protocols::http::Message::body, ::testing::StrEq(x.body)));
}

// TODO(yzhao): http::Record misses many fields from the records in http data table. Consider adding
// additional fields.
inline auto EqHTTPRecord(const protocols::http::Record& x) {
  using ::testing::Field;

  return AllOf(Field(&protocols::http::Record::req, EqHTTPReq(x.req)),
               Field(&protocols::http::Record::resp, EqHTTPResp(x.resp)));
}

inline auto EqMux(const protocols::mux::Frame& x) {
  using ::testing::Field;

  return Field(&protocols::mux::Frame::type, ::testing::Eq(x.type));
}

inline auto EqMuxRecord(const protocols::mux::Record& x) {
  using ::testing::Field;

  return AllOf(Field(&protocols::mux::Record::req, EqMux(x.req)),
               Field(&protocols::mux::Record::resp, EqMux(x.resp)));
}

inline std::vector<std::string> GetRemoteAddrs(const types::ColumnWrapperRecordBatch& rb,
                                               const std::vector<size_t>& indices) {
  std::vector<std::string> addrs;
  for (size_t idx : indices) {
    addrs.push_back(rb[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx));
  }
  return addrs;
}

inline std::vector<std::string> GetLocalAddrs(const types::ColumnWrapperRecordBatch& rb,
                                              const std::vector<size_t>& indices) {
  std::vector<std::string> addrs;
  for (size_t idx : indices) {
    addrs.push_back(rb[kHTTPLocalAddrIdx]->Get<types::StringValue>(idx));
  }
  return addrs;
}

inline std::vector<int64_t> GetRemotePorts(const types::ColumnWrapperRecordBatch& rb,
                                           const std::vector<size_t>& indices) {
  std::vector<int64_t> addrs;
  for (size_t idx : indices) {
    addrs.push_back(rb[kHTTPRemotePortIdx]->Get<types::Int64Value>(idx).val);
  }
  return addrs;
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
