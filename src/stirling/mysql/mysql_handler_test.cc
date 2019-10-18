#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_handler.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace mysql {

using ::testing::SizeIs;

class HandlerTest : public ::testing::Test {};

bool operator==(const ErrResponse& lhs, const ErrResponse& rhs) {
  return (lhs.error_code() == rhs.error_code() && lhs.error_message() == rhs.error_message());
}

bool operator==(const StmtPrepareRespHeader& lhs, const StmtPrepareRespHeader& rhs) {
  return lhs.stmt_id == rhs.stmt_id && lhs.num_columns == rhs.num_columns &&
         lhs.num_params == rhs.num_params && lhs.warning_count == rhs.warning_count;
}

bool operator!=(const StmtPrepareRespHeader& lhs, const StmtPrepareRespHeader& rhs) {
  return !(lhs == rhs);
}

bool operator!=(const ColDefinition& lhs, const ColDefinition& rhs) { return lhs.msg != rhs.msg; }

bool operator!=(const ResultsetRow& lhs, const ResultsetRow& rhs) { return lhs.msg != rhs.msg; }

bool operator==(const StmtPrepareOKResponse& lhs, const StmtPrepareOKResponse& rhs) {
  if (lhs.resp_header() != rhs.resp_header()) {
    return false;
  }
  if (lhs.col_defs().size() != rhs.col_defs().size()) {
    return false;
  }
  if (lhs.param_defs().size() != rhs.param_defs().size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.col_defs().size(); ++i) {
    if (lhs.col_defs()[i] != rhs.col_defs()[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < lhs.param_defs().size(); ++i) {
    if (lhs.param_defs()[i] != rhs.param_defs()[i]) {
      return false;
    }
  }
  return true;
}

bool operator==(const StmtExecuteRequest& lhs, const StmtExecuteRequest& rhs) {
  if ((lhs.stmt_id() != rhs.stmt_id()) || (lhs.params().size() != rhs.params().size())) {
    return false;
  }
  for (size_t i = 0; i < lhs.params().size(); ++i) {
    if ((lhs.params()[i].type != rhs.params()[i].type) ||
        (lhs.params()[i].value != rhs.params()[i].value)) {
      return false;
    }
  }
  return true;
}

bool operator==(const StringRequest& lhs, const StringRequest& rhs) {
  return lhs.msg() == rhs.msg() && lhs.type() == rhs.type();
}

bool operator==(const Resultset& lhs, const Resultset& rhs) {
  if (lhs.num_col() != rhs.num_col()) {
    return false;
  }
  if (lhs.col_defs().size() != rhs.col_defs().size()) {
    return false;
  }
  if (lhs.results().size() != rhs.results().size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.col_defs().size(); ++i) {
    if (lhs.col_defs()[i] != rhs.col_defs()[i]) {
      return false;
    }
  }
  for (size_t i = 0; i < lhs.results().size(); ++i) {
    if (lhs.results()[i] != rhs.results()[i]) {
      return false;
    }
  }
  return true;
}

TEST_F(HandlerTest, TestHandleErrMessage) {
  ErrResponse expected_response(1096, "This an error.");

  std::deque<Packet> resp_packets;
  Packet packet = testutils::GenErr(/* seq_id */ 1, expected_response);
  resp_packets.emplace_back(packet);
  std::unique_ptr<ErrResponse> result_response = HandleErrMessage(&resp_packets);
  ASSERT_NE(nullptr, result_response);
  ASSERT_THAT(resp_packets, SizeIs(0));
  EXPECT_EQ(expected_response, *result_response);
}

TEST_F(HandlerTest, TestHandleOKMessage) {
  std::deque<Packet> resp_packets;
  Packet packet = testutils::GenOK(1);
  resp_packets.emplace_back(packet);
  std::unique_ptr<OKResponse> ok_response = HandleOKMessage(&resp_packets);
  EXPECT_NE(nullptr, ok_response);
}

TEST_F(HandlerTest, TestHandleResultset) {
  {
    // Test without CLIENT_DEPRECATE_EOF.
    std::deque<Packet> packets = testutils::GenResultset(testutils::kStmtExecuteResultset);
    auto s = HandleResultset(&packets);
    EXPECT_TRUE(s.ok());
    std::unique_ptr<Resultset> result = s.ConsumeValueOrDie();
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(testutils::kStmtExecuteResultset, *result);
  }

  {
    // Test with CLIENT_DEPRECATE_EOF.
    std::deque<Packet> packets = testutils::GenResultset(testutils::kStmtExecuteResultset, true);
    auto s = HandleResultset(&packets);
    EXPECT_TRUE(s.ok());
    std::unique_ptr<Resultset> result = s.ConsumeValueOrDie();
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(testutils::kStmtExecuteResultset, *result);
  }
}

TEST_F(HandlerTest, TestHandleStmtPrepareOKResponse) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testutils::kStmtPrepareResponse);
  auto s = HandleStmtPrepareOKResponse(&packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<StmtPrepareOKResponse> result_response = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result_response);
  EXPECT_EQ(testutils::kStmtPrepareResponse, *result_response);
}

TEST_F(HandlerTest, TestHandleStmtExecuteRequest) {
  Packet req_packet = testutils::GenStmtExecuteRequest(testutils::kStmtExecuteRequest);
  ReqRespEvent e = testutils::InitStmtPrepare();
  int stmt_id = static_cast<StmtPrepareOKResponse*>(e.response())->resp_header().stmt_id;
  std::map<int, ReqRespEvent> prepare_map;
  prepare_map.emplace(stmt_id, std::move(e));
  auto s = HandleStmtExecuteRequest(req_packet, &prepare_map);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<StmtExecuteRequest> result_request = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result_request);
  EXPECT_EQ(testutils::kStmtExecuteRequest, *result_request);
}

TEST_F(HandlerTest, TestHandleStringRequest) {
  Packet req_packet =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  auto s = HandleStringRequest(req_packet);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<StringRequest> result_request = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result_request);
  EXPECT_EQ(testutils::kStmtPrepareRequest, *result_request);
}

// TODO(chengruizhe): Add failure test cases.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
