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

bool operator==(const Entry& lhs, const Entry& rhs) {
  return lhs.msg == rhs.msg && lhs.status == rhs.status;
}

TEST(StitcherTest, TestProcessStmtPrepareOK) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;
  std::deque<Packet> ok_resp_packets =
      testutils::GenStmtPrepareOKResponse(testutils::kStmtPrepareResponse);
  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};

  // Run function-under-test.
  std::vector<Entry> entries;
  StatusOr<bool> s = ProcessStmtPrepare(req, &ok_resp_packets, &state, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check resulting state and entries.
  auto iter = state.prepare_events.find(stmt_id);
  EXPECT_TRUE(iter != state.prepare_events.end());
  EXPECT_EQ(entries.size(), 0);
}

TEST(StitcherTest, TestProcessStmtPrepareErr) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;
  std::deque<Packet> err_resp_packets;
  ErrResponse expected_response(1096, "This an error.");
  err_resp_packets.emplace_back(testutils::GenErr(expected_response));
  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};

  // Run function-under-test.
  std::vector<Entry> entries;
  StatusOr<bool> s = ProcessStmtPrepare(req, &err_resp_packets, &state, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check resulting state and entries.
  auto iter = state.prepare_events.find(stmt_id);
  EXPECT_EQ(iter, state.prepare_events.end());
  ASSERT_EQ(1, entries.size());
  Entry& err_entry = entries[0];
  Entry expected_err_entry{absl::Substitute(R"({"Error": "$0", "Message": "$1"})", "This an error.",
                                            testutils::kStmtPrepareRequest.msg()),
                           MySQLEntryStatus::kErr, 0};
  EXPECT_EQ(expected_err_entry, err_entry);
}

TEST(StitcherTest, TestProcessStmtExecute) {
  // Test setup.
  Packet req = testutils::GenStmtExecuteRequest(testutils::kStmtExecuteRequest);
  int stmt_id = testutils::kStmtExecuteRequest.stmt_id();
  std::deque<Packet> resultset = testutils::GenResultset(testutils::kStmtExecuteResultset);
  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  // Run function-under-test.
  std::vector<Entry> entries;
  StatusOr<bool> s = ProcessStmtExecute(req, &resultset, &state, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check resulting state and entries.
  ASSERT_EQ(1, entries.size());
  Entry& resultset_entry = entries[0];
  Entry expected_resultset_entry{
      "{\"Message\": \"SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
      "GROUP "
      "BY id ORDER BY id\"}",
      MySQLEntryStatus::kOK, 0};
  EXPECT_EQ(expected_resultset_entry, resultset_entry);
}

TEST(StitcherTest, TestProcessStmtClose) {
  // Test setup.
  Packet req = testutils::GenStmtCloseRequest(testutils::kStmtCloseRequest);
  int stmt_id = testutils::kStmtCloseRequest.stmt_id();
  std::deque<Packet> resp_packets = {};
  State state{std::map<int, ReqRespEvent>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  // Run function-under-test.
  std::vector<Entry> entries;
  StatusOr<bool> s = ProcessStmtClose(req, &resp_packets, &state, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check the resulting entries and state.
  EXPECT_EQ(0, state.prepare_events.size());
  EXPECT_EQ(0, entries.size());
}

TEST(StitcherTest, TestProcessQuery) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testutils::kQueryRequest, MySQLEventType::kQuery);
  std::deque<Packet> resultset = testutils::GenResultset(testutils::kQueryResultset);

  // Run function-under-test.
  std::vector<Entry> entries;
  auto s = ProcessQuery(req, &resultset, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check resulting state and entries.
  ASSERT_EQ(1, entries.size());
  Entry& resultset_entry = entries[0];
  Entry expected_resultset_entry{"{\"Message\": \"SELECT name FROM tag;\"}", MySQLEntryStatus::kOK,
                                 0};
  EXPECT_EQ(expected_resultset_entry, resultset_entry);
}

TEST(StitcherTest, ProcessRequestWithBasicResponse) {
  // Test setup.
  // Ping is a request that always has a response OK.
  Packet req = testutils::GenStringRequest(StringRequest(), kComPing);
  std::deque<Packet> response_packets = {testutils::GenOK()};

  // Run function-under-test.
  std::vector<Entry> entries;
  auto s = ProcessRequestWithBasicResponse(req, &response_packets, &entries);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(s.ValueOrDie());

  // Check resulting state and entries.
  EXPECT_EQ(0, entries.size());
}

// TODO(chengruizhe): Add test cases for inputs that would return Status error.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
