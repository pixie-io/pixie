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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/shared/types/column_wrapper.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

namespace px {
namespace stirling {
namespace testing {

//-----------------------------------------------------------------------------
// HTTP Checkers
//-----------------------------------------------------------------------------

std::vector<protocols::http::Record> ToRecordVector(const types::ColumnWrapperRecordBatch& rb,
                                                    const std::vector<size_t>& indices);

std::vector<protocols::http::Record> GetTargetRecords(
    const types::ColumnWrapperRecordBatch& record_batch, int32_t pid);

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

inline auto EqHTTPRecord(const protocols::http::Record& x) {
  using ::testing::Field;

  return AllOf(Field(&protocols::http::Record::req, EqHTTPReq(x.req)),
               Field(&protocols::http::Record::resp, EqHTTPResp(x.resp)));
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
