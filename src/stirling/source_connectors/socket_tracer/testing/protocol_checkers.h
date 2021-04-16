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
  return AllOf(
      ::testing::Field(&protocols::http::Message::req_path, ::testing::Eq(x.req_path)),
      ::testing::Field(&protocols::http::Message::req_method, ::testing::StrEq(x.req_method)),
      ::testing::Field(&protocols::http::Message::body, ::testing::StrEq(x.body)));
}

inline auto EqHTTPResp(const protocols::http::Message& x) {
  return AllOf(
      ::testing::Field(&protocols::http::Message::resp_status, ::testing::Eq(x.resp_status)),
      ::testing::Field(&protocols::http::Message::resp_message, ::testing::StrEq(x.resp_message)),
      ::testing::Field(&protocols::http::Message::body, ::testing::StrEq(x.body)));
}

inline auto EqHTTPRecord(const protocols::http::Record& x) {
  return AllOf(::testing::Field(&protocols::http::Record::req, EqHTTPReq(x.req)),
               ::testing::Field(&protocols::http::Record::resp, EqHTTPResp(x.resp)));
}

}  // namespace testing
}  // namespace stirling
}  // namespace px
