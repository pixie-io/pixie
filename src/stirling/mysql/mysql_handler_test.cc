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

TEST(ProcessLengthEncodedInt, Basic) {
  int64_t num;
  size_t offset;

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\x12"), &offset).ValueOrDie();
  EXPECT_EQ(num, 18);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfa"), &offset).ValueOrDie();
  EXPECT_EQ(num, 250);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfc\xfb\x00"), &offset).ValueOrDie();
  EXPECT_EQ(num, 251);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23\x45"), &offset).ValueOrDie();
  EXPECT_EQ(num, 0x452301);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset)
            .ValueOrDie();
  EXPECT_EQ(num, 0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, WithOffset) {
  int64_t num;
  size_t offset;

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\x12"), &offset).ValueOrDie();
  EXPECT_EQ(num, 18);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfa"), &offset).ValueOrDie();
  EXPECT_EQ(num, 250);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfc\xfb\x00"), &offset).ValueOrDie();
  EXPECT_EQ(num, 251);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfd\x01\x23\x45"), &offset).ValueOrDie();
  EXPECT_EQ(num, 0x452301);

  offset = 1;
  num =
      ProcessLengthEncodedInt(ConstStringView("\x77\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset)
          .ValueOrDie();
  EXPECT_EQ(num, 0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, NotEnoughBytes) {
  StatusOr<int64_t> s;
  size_t offset;

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView(""), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfc\xfb"), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23"), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd"), &offset);
  EXPECT_NOT_OK(s);
}

TEST(HandleErrMessage, Basic) {
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  std::deque<Packet> resp_packets = {testutils::GenErr(/* seq_id */ 1, err_resp)};

  Record entry;
  HandleErrMessage(resp_packets, &entry);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kErr);
  EXPECT_EQ(entry.resp.msg, "This is an error.");
}

TEST(HandleOKMessage, Basic) {
  std::deque<Packet> resp_packets = {testutils::GenOK(1)};

  Record entry;
  HandleOKMessage(resp_packets, &entry);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kOK);
}

TEST(HandleResultsetResponse, ValidWithEOF) {
  // Test without CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);

  Record entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kOK);
  EXPECT_EQ(entry.resp.msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, ValidNoEOF) {
  // Test with CLIENT_DEPRECATE_EOF.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset, true);

  Record entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kOK);
  EXPECT_EQ(entry.resp.msg, "Resultset rows = 2");
}

TEST(HandleResultsetResponse, NeedsMoreData) {
  // Test for incomplete response.
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.pop_back();

  Record entry;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleResultsetResponse, InvalidResponse) {
  // Test for invalid response by changing first packet.
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  std::deque<Packet> resp_packets = testutils::GenResultset(testdata::kStmtExecuteResultset);
  resp_packets.front() = testutils::GenErr(/* seq_id */ 1, err_resp);

  Record entry;
  State state;
  auto s = HandleResultsetResponse(resp_packets, &entry);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, Valid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);

  Record entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kSuccess);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kOK);
  EXPECT_EQ(state.prepared_statements.size(), 1);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, NeedsMoreData) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  packets.pop_back();

  Record entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.ValueOrDie(), ParseState::kNeedsMoreData);
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(state.prepared_statements.size(), 0);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtPrepareOKResponse, Invalid) {
  std::deque<Packet> packets = testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  ErrResponse err_resp = {.error_code = 1096, .error_message = "This is an error."};
  packets.front() = testutils::GenErr(/* seq_id */ 1, err_resp);

  Record entry;
  State state;
  auto s = HandleStmtPrepareOKResponse(packets, &state, &entry);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(entry.resp.status, MySQLRespStatus::kUnknown);
  EXPECT_EQ(state.prepared_statements.size(), 0);
  EXPECT_EQ(entry.resp.msg, "");
}

TEST(HandleStmtExecuteRequest, Basic) {
  Packet req_packet = testutils::GenStmtExecuteRequest(testdata::kStmtExecuteRequest);
  PreparedStatement prepared_stmt = testdata::kPreparedStatement;
  int stmt_id = prepared_stmt.response.header.stmt_id;
  std::map<int, PreparedStatement> prepare_map;
  prepare_map.emplace(stmt_id, std::move(prepared_stmt));

  Record entry;
  HandleStmtExecuteRequest(req_packet, &prepare_map, &entry);
  EXPECT_EQ(entry.req.cmd, MySQLEventType::kStmtExecute);
  EXPECT_EQ(entry.req.msg,
            "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=brown GROUP BY id ORDER BY id");
}

TEST(HandleStringRequest, Basic) {
  Packet req_packet =
      testutils::GenStringRequest(testdata::kStmtPrepareRequest, MySQLEventType::kStmtPrepare);

  Record entry;
  HandleStringRequest(req_packet, &entry);
  EXPECT_EQ(entry.req.cmd, MySQLEventType::kStmtPrepare);
  EXPECT_EQ(entry.req.msg,
            "SELECT sock.sock_id AS id, GROUP_CONCAT(tag.name) AS tag_name FROM sock JOIN sock_tag "
            "ON sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id WHERE "
            "tag.name=? GROUP BY id ORDER BY ?");
}

// TODO(chengruizhe): Add failure test cases.

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
