#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_stitcher.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace mysql {

class StitcherTest : public ::testing::Test {};

bool operator==(const Entry& lhs, const Entry& rhs) {
  return lhs.msg == rhs.msg && lhs.status == rhs.status;
}

TEST_F(StitcherTest, TestStitchStmtPrepareOK) {
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);

  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;

  std::deque<Packet> ok_resp_packets =
      testutils::GenStmtPrepareOKResponse(testutils::kStmtPrepareResponse);

  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  auto s1 = StitchStmtPrepare(req, &ok_resp_packets, &state);
  EXPECT_TRUE(s1.ok());
  auto iter1 = state.prepare_events.find(stmt_id);
  EXPECT_TRUE(iter1 != state.prepare_events.end());
}

TEST_F(StitcherTest, TestStitchStmtPrepareErr) {
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;

  std::deque<Packet> err_resp_packets;
  ErrResponse expected_response(1096, "This an error.");
  err_resp_packets.emplace_back(testutils::GenErr(expected_response));

  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  auto s2 = StitchStmtPrepare(req, &err_resp_packets, &state);
  EXPECT_TRUE(s2.ok());
  auto iter2 = state.prepare_events.find(stmt_id);
  EXPECT_EQ(iter2, state.prepare_events.end());
  Entry err_entry = s2.ValueOrDie();

  Entry expected_err_entry{absl::Substitute(R"({"Error": "$0", "Message": "$1"})", "This an error.",
                                            testutils::kStmtPrepareRequest.msg()),
                           MySQLEntryStatus::kErr, 0};

  EXPECT_EQ(expected_err_entry, err_entry);
}

TEST_F(StitcherTest, TestStitchStmtExecute) {
  Packet req = testutils::GenStmtExecuteRequest(testutils::kStmtExecuteRequest);

  int stmt_id = testutils::kStmtExecuteRequest.stmt_id();

  std::deque<Packet> resultset = testutils::GenResultset(testutils::kStmtExecuteResultset);

  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  auto s1 = StitchStmtExecute(req, &resultset, &state);
  EXPECT_TRUE(s1.ok());
  Entry resultset_entry = s1.ValueOrDie();

  Entry expected_resultset_entry{
      "{\"Message\": \"SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
      "GROUP "
      "BY id ORDER BY id\"}",
      MySQLEntryStatus::kOK, 0};

  EXPECT_EQ(expected_resultset_entry, resultset_entry);
}

TEST_F(StitcherTest, TestStitchStmtClose) {
  Packet req = testutils::GenStmtCloseRequest(testutils::kStmtCloseRequest);

  int stmt_id = testutils::kStmtCloseRequest.stmt_id();

  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  auto s1 = StitchStmtClose(req, &state);
  EXPECT_TRUE(s1.ok());
  EXPECT_EQ(0, state.prepare_events.size());
}

TEST_F(StitcherTest, TestStitchQuery) {
  Packet req = testutils::GenStringRequest(testutils::kQueryRequest, MySQLEventType::kQuery);

  std::deque<Packet> resultset = testutils::GenResultset(testutils::kQueryResultset);

  auto s1 = StitchQuery(req, &resultset);
  EXPECT_TRUE(s1.ok());
  Entry resultset_entry = s1.ValueOrDie();

  Entry expected_resultset_entry{"{\"Message\": \"SELECT name FROM tag;\"}", MySQLEntryStatus::kOK,
                                 0};

  EXPECT_EQ(expected_resultset_entry, resultset_entry);
}

// TODO(chengruizhe): Add test cases for inputs that would return Status error.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
