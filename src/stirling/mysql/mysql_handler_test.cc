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

bool operator==(const ColDefinition& lhs, const ColDefinition& rhs) { return lhs.msg == rhs.msg; }
bool operator!=(const ColDefinition& lhs, const ColDefinition& rhs) { return !(lhs == rhs); }

bool operator==(const ResultsetRow& lhs, const ResultsetRow& rhs) { return lhs.msg == rhs.msg; }
bool operator!=(const ResultsetRow& lhs, const ResultsetRow& rhs) { return !(lhs == rhs); }

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
  if (lhs.col_defs() != rhs.col_defs()) {
    return false;
  }
  if (lhs.results() != rhs.results()) {
    return false;
  }
  return true;
}

TEST(HandleErrMessage, Basic) {
  ErrResponse expected_response(1096, "This is an error.");
  std::deque<Packet> resp_packets = {testutils::GenErr(/* seq_id */ 1, expected_response)};
  std::unique_ptr<ErrResponse> result_response = HandleErrMessage(resp_packets);
  ASSERT_NE(nullptr, result_response);
  EXPECT_EQ(expected_response, *result_response);
}

TEST(HandleOKMessage, Basic) {
  std::deque<Packet> resp_packets = {testutils::GenOK(1)};
  std::unique_ptr<OKResponse> ok_response = HandleOKMessage(resp_packets);
  EXPECT_NE(nullptr, ok_response);
}

TEST(HandleResultsetResponse, ValidWithEOF) {
  // Test without CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  auto s = HandleResultsetResponse(resp_packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<Resultset> result = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result);
  EXPECT_EQ(testdata::kStmtExecuteResultset, *result);
}

TEST(HandleResultsetResponse, ValidNoEOF) {
  // Test with CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset, true);
  auto s = HandleResultsetResponse(resp_packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<Resultset> result = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result);
  EXPECT_EQ(testdata::kStmtExecuteResultset, *result);
}

TEST(HandleResultsetResponse, NeedsMoreData) {
  // Test for incomplete response.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.pop_back();
  auto s = HandleResultsetResponse(resp_packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<Resultset> result = s.ConsumeValueOrDie();
  ASSERT_EQ(nullptr, result);
}

TEST(HandleResultsetResponse, InvalidResponse) {
  // Test for invalid response by changing first packet.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.front() = testutils::GenErr(/* seq_id */ 1, ErrResponse(1096, "This is an error."));
  auto s = HandleResultsetResponse(resp_packets);
  EXPECT_FALSE(s.ok());
}

TEST(HandleStmtPrepareOKResponse, Valid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  auto s = HandleStmtPrepareOKResponse(packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<StmtPrepareOKResponse> result_response = s.ConsumeValueOrDie();
  ASSERT_NE(nullptr, result_response);
  EXPECT_EQ(testdata::kStmtPrepareResponse, *result_response);
}

TEST(HandleStmtPrepareOKResponse, NeedsMoreData) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  packets.pop_back();
  auto s = HandleStmtPrepareOKResponse(packets);
  EXPECT_TRUE(s.ok());
  std::unique_ptr<StmtPrepareOKResponse> result_response = s.ConsumeValueOrDie();
  ASSERT_EQ(nullptr, result_response);
}

TEST(HandleStmtPrepareOKResponse, Invalid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  packets.front() = testutils::GenErr(/* seq_id */ 1, ErrResponse(1096, "This is an error."));
  auto s = HandleStmtPrepareOKResponse(packets);
  EXPECT_FALSE(s.ok());
}

TEST(HandleStmtExecuteRequest, Basic) {
  Packet req_packet = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  PreparedStatement prepared_stmt = testdata::InitStmtPrepare();
  int stmt_id = prepared_stmt.response->resp_header().stmt_id;
  std::map<int, PreparedStatement> prepare_map;
  prepare_map.emplace(stmt_id, std::move(prepared_stmt));

  Entry entry;
  HandleStmtExecuteRequest(req_packet, &prepare_map, &entry);
  EXPECT_EQ(entry.cmd, MySQLEventType::kStmtExecute);
  EXPECT_EQ(entry.req_msg,
            "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=brown GROUP BY id ORDER BY id");
}

TEST(HandleStringRequest, Basic) {
  Packet req_packet =
      testutils::GenStringRequest(testdata::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);

  Entry entry;
  HandleStringRequest(req_packet, &entry);
  EXPECT_EQ(entry.cmd, MySQLEventType::kStmtPrepare);
  EXPECT_EQ(entry.req_msg,
            "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=? GROUP BY id ORDER BY ?");
}

// TODO(chengruizhe): Add failure test cases.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
