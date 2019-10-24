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
  return lhs.req.cmd == rhs.req.cmd && lhs.req.msg == rhs.req.msg &&
         lhs.req.timestamp_ns == rhs.req.timestamp_ns && lhs.resp.status == rhs.resp.status &&
         lhs.resp.msg == rhs.resp.msg && lhs.resp.timestamp_ns == rhs.resp.timestamp_ns;
}

TEST(StitcherTest, TestProcessStmtPrepareOK) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testdata::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  std::deque<Packet> ok_resp_packets =
      testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  State state{std::map<int, PreparedStatement>()};

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtPrepare(req, ok_resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepared_statements.find(testdata::kStmtID);
  EXPECT_TRUE(iter != state.prepared_statements.end());
  Entry expected_entry{.req = {MySQLEventType::kStmtPrepare, testdata::kStmtPrepareRequest.msg, 0},
                       .resp = {MySQLRespStatus::kOK, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessStmtPrepareErr) {
  // Test setup.
  Packet req =
      testutils::GenStringRequest(testdata::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);
  std::deque<Packet> err_resp_packets;
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  err_resp_packets.emplace_back(testutils::GenErr(/* seq_id */ 1, err_resp));
  State state{std::map<int, PreparedStatement>()};

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtPrepare(req, err_resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  auto iter = state.prepared_statements.find(testdata::kStmtID);
  EXPECT_EQ(iter, state.prepared_statements.end());
  Entry expected_err_entry{
      .req = {MySQLEventType::kStmtPrepare, testdata::kStmtPrepareRequest.msg, 0},
      .resp = {MySQLRespStatus::kErr, "This is an error.", 0}};
  EXPECT_EQ(expected_err_entry, entry);
}

TEST(StitcherTest, TestProcessStmtExecute) {
  // Test setup.
  Packet req = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  std::deque<Packet> resultset = testutils::GenResultset(testdata::kStmtExecuteResultset);
  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtExecute(req, resultset, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  Entry expected_resultset_entry{
      .req = {MySQLEventType::kStmtExecute,
              "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock "
              "JOIN sock_tag ON "
              "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
              "tag.name=brown "
              "GROUP "
              "BY id ORDER BY id",
              0},
      .resp = {MySQLRespStatus::kOK, "Resultset rows = 2", 0}};
  EXPECT_EQ(expected_resultset_entry, entry);
}

TEST(StitcherTest, TestProcessStmtClose) {
  // Test setup.
  Packet req = testutils::GenStmtCloseRequest(testdata::kStmtCloseRequest);
  std::deque<Packet> resp_packets = {};
  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

  // Run function-under-test.
  Entry entry;
  StatusOr<ParseState> s = ProcessStmtClose(req, resp_packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check the resulting entries and state.
  Entry expected_entry{.req = {MySQLEventType::kStmtClose, "", 0},
                       .resp = {MySQLRespStatus::kOK, "", 0}};
  EXPECT_EQ(expected_entry, entry);
}

TEST(StitcherTest, TestProcessQuery) {
  // Test setup.
  Packet req = testutils::GenStringRequest(testdata::kQueryRequest, MySQLEventType::kQuery);
  std::deque<Packet> resultset = testutils::GenResultset(testdata::kQueryResultset);

  // Run function-under-test.
  Entry entry;
  auto s = ProcessQuery(req, resultset, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);

  // Check resulting state and entries.
  Entry expected_resultset_entry{.req = {MySQLEventType::kQuery, "SELECT name FROM tag;", 0},
                                 .resp = {MySQLRespStatus::kOK, "Resultset rows = 3", 0}};
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
  Entry expected_entry{.req = {MySQLEventType::kPing, "", 0},
                       .resp = {MySQLRespStatus::kOK, "", 0}};
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
  Packet req = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  req.timestamp_ns = t++;

  std::deque<Packet> responses = testutils::GenResultset(testdata::kStmtExecuteResultset);
  for (auto& p : responses) {
    p.timestamp_ns = t++;
  }

  // Now prepend the two dangling packets to the responses.
  responses.push_front(resp1);
  responses.push_front(resp0);

  State state{std::map<int, PreparedStatement>()};
  state.prepared_statements.emplace(testdata::kStmtID, testdata::kPreparedStatement);

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
