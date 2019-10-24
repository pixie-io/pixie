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

TEST(HandleErrMessage, Basic) {
  std::deque<Packet> resp_packets = {
      testutils::GenErr(/* seq_id */ 1, ErrResponse(1096, "This is an error."))};

  Entry entry;
  HandleErrMessage(resp_packets, &entry);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kErr);
  EXPECT_EQ(entry.resp_msg, "This is an error.");
}

TEST(HandleOKMessage, Basic) {
  std::deque<Packet> resp_packets = {testutils::GenOK(1)};

  Entry entry;
  HandleOKMessage(resp_packets, &entry);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kOK);
}

TEST(HandleResultsetResponse, ValidWithEOF) {
  // Test without CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);

  Entry entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kOK);
  EXPECT_EQ(entry.resp_msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, ValidNoEOF) {
  // Test with CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset, true);

  Entry entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kOK);
  EXPECT_EQ(entry.resp_msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, NeedsMoreData) {
  // Test for incomplete response.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.pop_back();

  Entry entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(entry.resp_msg, "");
}

TEST(HandleResultsetResponse, InvalidResponse) {
  // Test for invalid response by changing first packet.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.front() = testutils::GenErr(/* seq_id */ 1, ErrResponse(1096, "This is an error."));

  Entry entry;
  State state;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(entry.resp_msg, "");
}

TEST(HandleStmtPrepareOKResponse, Valid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);

  Entry entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kOK);
  EXPECT_EQ(state.prepare_events.size(), 1);
  EXPECT_EQ(entry.resp_msg, "");
}

TEST(HandleStmtPrepareOKResponse, NeedsMoreData) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  packets.pop_back();

  Entry entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(state.prepare_events.size(), 0);
  EXPECT_EQ(entry.resp_msg, "");
}

TEST(HandleStmtPrepareOKResponse, Invalid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  packets.front() =
      testutils::GenErr(/* seq_id */ 1, ErrResponse(/* error_code */ 1096, "This is an error."));

  Entry entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(entry.resp_status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(state.prepare_events.size(), 0);
  EXPECT_EQ(entry.resp_msg, "");
}

TEST(HandleStmtExecuteRequest, Basic) {
  Packet req_packet = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  PreparedStatement prepared_stmt = testdata::kPreparedStatement;
  int stmt_id = prepared_stmt.response.header().stmt_id;
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
