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
  return lhs.cmd == rhs.cmd && lhs.req_msg == rhs.req_msg &&
         lhs.req_timestamp_ns == rhs.req_timestamp_ns && lhs.resp_status == rhs.resp_status &&
         lhs.resp_msg == rhs.resp_msg;
}

TEST(StitcherTest, TestProcessStmtPrepareOK) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;
  std::deque<Packet> ok_resp_packets =
      testutils::GenStmtPrepareOKResponse(testutils::kStmtPrepareResponse);
  State state{std::map<int, PreparedStatement>(), FlagStatus::kUnknown};

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtPrepare(req, ok_resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepare_events.find(stmt_id);
  EXPECT_TRUE(iter != state.prepare_events.end());
  Entry expected_entry{MySQLEventType::kStmtPrepare,
                       std::string(testutils::kStmtPrepareRequest.msg()), MySQLRespStatus::kOK, "",
                       0};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessStmtPrepareErr) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testutils::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  int stmt_id = testutils::kStmtPrepareResponse.resp_header().stmt_id;
  std::deque<Packet> err_resp_packets;
  ErrResponse expected_response(1096, "This is an error.");
  err_resp_packets.emplace_back(testutils::GenErr(/* seq_id */ 1, expected_response));
  State state{std::map<int, PreparedStatement>(), FlagStatus::kUnknown};

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtPrepare(req, err_resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepare_events.find(stmt_id);
  EXPECT_EQ(iter, state.prepare_events.end());
  Entry expected_err_entry{MySQLEventType::kStmtPrepare,
                           std::string(testutils::kStmtPrepareRequest.msg()), MySQLRespStatus::kErr,
                           "This is an error.", 0};
  EXPECT_EQ(expected_err_entry, entry);
}

TEST(StitcherTest, TestProcessStmtExecute) {
  // Test setup.
  Packet req = testutils::GenStmtExecuteRequest(testutils::kStmtExecuteRequest);
  int stmt_id = testutils::kStmtExecuteRequest.stmt_id();
  std::deque<Packet> resultset = testutils::GenResultset(testutils::kStmtExecuteResultset);
  State state{std::map<int, PreparedStatement>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtExecute(req, resultset, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  Entry expected_resultset_entry{
      MySQLEventType::kStmtExecute,
      "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
      "JOIN sock_tag ON "
      "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE tag.name=brown "
      "GROUP "
      "BY id ORDER BY id",
      MySQLRespStatus::kOK, "Resultset rows = 2", 0};
  EXPECT_EQ(expected_resultset_entry, entry);
}

TEST(StitcherTest, TestProcessStmtClose) {
  // Test setup.
  Packet req = testutils::GenStmtCloseRequest(testutils::kStmtCloseRequest);
  int stmt_id = testutils::kStmtCloseRequest.stmt_id();
  std::deque<Packet> resp_packets = {};
  State state{std::map<int, PreparedStatement>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtClose(req, resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check the resulting entries and state.
  Entry expected_entry{MySQLEventType::kStmtClose, "", MySQLRespStatus::kOK, "", 0};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessQuery) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testutils::kQueryRequest, MySQLEventType::kQuery);
  std::deque<Packet> resultset = testutils::GenResultset(testutils::kQueryResultset);

  // Run function-under-test.
  Entry entry;
  auto s = ProcessQuery(req, resultset, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  Entry expected_resultset_entry{MySQLEventType::kQuery, "SELECT name FROM tag;",
                                 MySQLRespStatus::kOK, "Resultset rows = 3", 0};
  EXPECT_EQ(expected_resultset_entry, entry);
}

TEST(StitcherTest, ProcessRequestWithBasicResponse) {
  // Test setup.
  // Ping is a request that always has a response OK.
  Packet req = testutils::GenStringRequest(StringRequest(), MySQLEventType::kPing);
  std::deque<Packet> resp_packets = {testutils::GenOK(/*seq_id*/ 1)};

  // Run function-under-test.
  Entry entry;
  auto s = ProcessRequestWithBasicResponse(req, /* string_req */ false, resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  Entry expected_entry{MySQLEventType::kPing, "", MySQLRespStatus::kOK, "", 0};
  EXPECT_EQ(expected_entry, entry);
}

TEST(SyncTest, OldResponses) {
  uint64_t t = 0;

  // First two packets are dangling, and pre-date the request below.
  Packet resp0 = testutils::GenOK(/*seq_id*/ 1);
  resp0.timestamp_ns = t++;

  Packet resp1 = testutils::GenOK(/*seq_id*/ 1);
  resp1.timestamp_ns = t++;

  // This is the main request, with a well-formed response below.
  Packet req = testutils::GenStmtExecuteRequest(testutils::kStmtExecuteRequest);
  req.timestamp_ns = t++;

  std::deque<Packet> responses = testutils::GenResultset(testutils::kStmtExecuteResultset);
  for (auto& p : responses) {
    p.timestamp_ns = t++;
  }

  // Now prepend the two dangling packets to the responses.
  responses.push_front(resp1);
  responses.push_front(resp0);

  int stmt_id = testutils::kStmtExecuteRequest.stmt_id();
  State state{std::map<int, PreparedStatement>(), FlagStatus::kUnknown};
  state.prepare_events.emplace(stmt_id, testutils::InitStmtPrepare());

  std::deque<Packet> requests = {req};
  std::vector<Entry> entries = ProcessMySQLPackets(&requests, &responses, &state);
  EXPECT_EQ(entries.size(), 1);
  EXPECT_EQ(requests.size(), 0);
  EXPECT_EQ(responses.size(), 0);
}

// TODO(chengruizhe): Add test cases for inputs that would return Status error.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
