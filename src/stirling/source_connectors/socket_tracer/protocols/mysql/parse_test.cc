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

#include <absl/container/flat_hash_map.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/test_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

struct MySQLReqResp {
  std::string request;
  std::string response;
  Command type;
};

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

bool operator==(const Packet& lhs, const Packet& rhs) {
  if (lhs.msg.compare(rhs.msg) != 0) {
    return false;
  }
  if (lhs.sequence_id != rhs.sequence_id) {
    return false;
  }
  return true;
}

// TODO(yzhao/oazizi): Fix this test:
//   src/stirling/source_connectors/socket_tracer/protocols/mysql/parse_test.cc:47:35: error:
//   object backing the pointer will be destroyed at the end of the full-expression
// TEST(MySQLParseFrame, Basics) {
//   Packet packet;
//   ParseState parse_state;
//
//   const std::string kEmptyReq = testutils::GenRawPacket(0, "");
//   const std::string kQueryReq = testutils::GenRawPacket(0, "\x03 SELECT * FROM foo;");
//
//   std::string_view empty_req_view(kEmptyReq);
//   parse_state = ParseFrame(message_type_t::kRequest, &empty_req_view, &packet);
//   EXPECT_EQ(parse_state, ParseState::kInvalid);
//
//   std::string_view short_req_view(kEmptyReq.substr(0, kPacketHeaderLength - 1));
//   parse_state = ParseFrame(message_type_t::kRequest, &short_req_view, &packet);
//   EXPECT_EQ(parse_state, ParseState::kNeedsMoreData);
//
//   std::string_view query_req_view(kQueryReq);
//   parse_state = ParseFrame(message_type_t::kRequest, &query_req_view, &packet);
//   EXPECT_EQ(parse_state, ParseState::kSuccess);
// }

class MySQLParserTest : public ::testing::Test {};

TEST_F(MySQLParserTest, ParseRaw) {
  const std::string buf = absl::StrCat(testutils::GenRawPacket(0, "\x03SELECT foo"),
                                       testutils::GenRawPacket(1, "\x03SELECT bar"));
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, buf, &parsed_messages, &state);

  Packet expected_message0;
  expected_message0.msg = "\x03SELECT foo";
  expected_message0.sequence_id = 0;

  Packet expected_message1;
  expected_message1.msg = "\x03SELECT bar";
  expected_message1.sequence_id = 1;

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message0, expected_message1));
}

TEST_F(MySQLParserTest, ParseComStmtPrepare) {
  std::string msg1 =
      testutils::GenRequestPacket(Command::kStmtPrepare, "SELECT name FROM users WHERE id = ?");
  std::string msg2 =
      testutils::GenRequestPacket(Command::kStmtPrepare, "SELECT age FROM users WHERE id = ?");

  Packet expected_message1;
  expected_message1.msg =
      absl::StrCat(CommandToString(Command::kStmtPrepare), "SELECT name FROM users WHERE id = ?");
  expected_message1.sequence_id = 0;

  Packet expected_message2;
  expected_message2.msg =
      absl::StrCat(CommandToString(Command::kStmtPrepare), "SELECT age FROM users WHERE id = ?");
  expected_message2.sequence_id = 0;

  const std::string buf = absl::StrCat(msg1, msg2);
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, buf, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseComStmtExecute) {
  // body of the MySQL StmtExecute message, from
  // https://dev.mysql.com/doc/internals/en/com-stmt-execute.html.
  const std::string body(
      ConstStringView("\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x0f\x00\x03\x66\x6f\x6f"));
  std::string msg1 = testutils::GenRequestPacket(Command::kStmtExecute, body);
  StateWrapper state{};

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(CommandToString(Command::kStmtExecute), body);
  expected_message1.sequence_id = 0;

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, msg1, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, ParseComStmtClose) {
  Packet expected_packet = testutils::GenStmtCloseRequest(testdata::kStmtCloseRequest);
  std::string msg = testutils::GenRawPacket(expected_packet);
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, msg, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_packet));
}

TEST_F(MySQLParserTest, ParseComQuery) {
  std::string msg1 = testutils::GenRequestPacket(Command::kQuery, "SELECT name FROM users");
  std::string msg2 = testutils::GenRequestPacket(Command::kQuery, "SELECT age FROM users");

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(CommandToString(Command::kQuery), "SELECT name FROM users");
  expected_message1.sequence_id = 0;

  Packet expected_message2;
  expected_message2.msg = absl::StrCat(CommandToString(Command::kQuery), "SELECT age FROM users");
  expected_message2.sequence_id = 0;

  const std::string buf = absl::StrCat(msg1, msg2);
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, buf, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseResponse) {
  // TODO(chengruizhe): Define GenResponse to generate responses.
  MySQLReqResp kMySQLStmtPrepareMessage = {
      testutils::GenRequestPacket(
          Command::kStmtPrepare,
          "SELECT COUNT(DISTINCT sock.sock_id) FROM sock JOIN sock_tag ON "
          "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id;"),
      // Response
      absl::StrCat(
          // Header packet
          ConstStringView("\x0c\x00\x00\x01\x00\x1a\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"),
          // Column def packet
          ConstStringView(
              "\x32\x00\x00\x02\x03\x64\x65\x66\x00\x00\x00\x1c\x43\x4f\x55\x4e\x54\x28\x44"
              "\x49\x53\x54\x49\x4e\x43\x54\x20\x73\x6f\x63\x6b\x2e\x73\x6f\x63\x6b\x5f\x69"
              "\x64\x29\x00\x0c\x3f\x00\x15\x00\x00\x00\x08\x81\x00\x00\x00\x00"),
          // EOF packet
          ConstStringView("\x05\x00\x00\x03\xfe\x00\x00\x02\x00")),
      Command::kStmtPrepare};
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result = ParseFramesLoop(
      message_type_t::kResponse, kMySQLStmtPrepareMessage.response, &parsed_messages, &state);
  EXPECT_EQ(ParseState::kSuccess, result.state);

  Packet expected_header;
  expected_header.msg = ConstStringView("\x00\x1a\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00");
  expected_header.sequence_id = 1;

  Packet expected_col_def;
  expected_col_def.msg = ConstStringView(
      "\x03\x64\x65\x66\x00\x00\x00\x1c\x43\x4f\x55\x4e\x54\x28\x44\x49\x53\x54\x49\x4e\x43\x54\x20"
      "\x73\x6f\x63\x6b\x2e\x73\x6f\x63\x6b\x5f\x69\x64\x29\x00\x0c\x3f\x00\x15\x00\x00\x00\x08\x81"
      "\x00\x00\x00\x00");
  expected_col_def.sequence_id = 2;

  Packet expected_eof;
  expected_eof.msg = ConstStringView("\xfe\x00\x00\x02\x00");
  expected_eof.sequence_id = 3;

  EXPECT_THAT(parsed_messages[0], ElementsAre(expected_header, expected_col_def, expected_eof));
}

TEST_F(MySQLParserTest, ParseMultipleRawPackets) {
  connection_id_t conn_id = 0;
  std::deque<Packet> prepare_resp_packets_deque =
      testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  absl::flat_hash_map<connection_id_t, std::deque<Packet>> prepare_resp_packets;
  prepare_resp_packets[conn_id] = prepare_resp_packets_deque;
  std::deque<Packet> execute_resp_packets_deque =
      testutils::GenResultset(testdata::kStmtExecuteResultset);
  absl::flat_hash_map<connection_id_t, std::deque<Packet>> execute_resp_packets;
  execute_resp_packets[conn_id] = execute_resp_packets_deque;

  // Splitting packets from 2 responses into 3 different raw packet chunks.
  std::vector<std::string> packets1;
  for (size_t i = 0; i < 3; ++i) {
    packets1.push_back(testutils::GenRawPacket(prepare_resp_packets[conn_id][i]));
  }

  std::vector<std::string> packets2;
  for (size_t i = 3; i < prepare_resp_packets[conn_id].size(); ++i) {
    packets2.push_back(testutils::GenRawPacket(prepare_resp_packets[conn_id][i]));
  }
  for (size_t i = 0; i < 2; ++i) {
    packets2.push_back(testutils::GenRawPacket(execute_resp_packets[conn_id][i]));
  }

  std::vector<std::string> packets3;
  for (size_t i = 2; i < execute_resp_packets[conn_id].size(); ++i) {
    packets3.push_back(testutils::GenRawPacket(execute_resp_packets[conn_id][i]));
  }

  std::string chunk1 = absl::StrJoin(packets1, "");
  std::string chunk2 = absl::StrJoin(packets2, "");
  std::string chunk3 = absl::StrJoin(packets3, "");

  const std::string buf = absl::StrCat(chunk1, chunk2, chunk3);
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kResponse, buf, &parsed_messages, &state);

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> expected_packets;
  for (const Packet& p : prepare_resp_packets[conn_id]) {
    expected_packets[conn_id].push_back(p);
  }
  for (const Packet& p : execute_resp_packets[conn_id]) {
    expected_packets[conn_id].push_back(p);
  }

  EXPECT_EQ(expected_packets[conn_id].size(), parsed_messages[conn_id].size());
  EXPECT_THAT(parsed_messages[conn_id], ::testing::ElementsAreArray(expected_packets[conn_id]));
}

TEST_F(MySQLParserTest, ParseIncompleteRequest) {
  std::string msg1 =
      testutils::GenRequestPacket(Command::kStmtPrepare, "SELECT name FROM users WHERE");
  // Change the length of the request so that it isn't complete.
  msg1[0] = '\x24';
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, msg1, &parsed_messages, &state);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre());
}

TEST_F(MySQLParserTest, ParseInvalidInput) {
  std::string msg1 = "hello world";
  StateWrapper state{};

  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kRequest, msg1, &parsed_messages, &state);
  EXPECT_EQ(ParseState::kInvalid, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre());
}

TEST_F(MySQLParserTest, Empty) {
  StateWrapper state{};
  absl::flat_hash_map<connection_id_t, std::deque<Packet>> parsed_messages;
  ParseResult<connection_id_t> result =
      ParseFramesLoop(message_type_t::kResponse, "", &parsed_messages, &state);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages[0], ElementsAre());
}

//=============================================================================
// MySQL FindFrameBoundary Tests
//=============================================================================

TEST_F(MySQLParserTest, FindReqBoundaryAligned) {
  const std::string buf =
      absl::StrCat(testutils::GenRequestPacket(Command::kQuery, "SELECT foo"),
                   testutils::GenRequestPacket(Command::kStmtPrepare, "blahblahblah"));
  StateWrapper state{};

  size_t pos = FindFrameBoundary<mysql::Packet>(message_type_t::kRequest, buf, 0, &state);
  ASSERT_EQ(pos, 0);
}

TEST_F(MySQLParserTest, FindReqBoundaryUnaligned) {
  const std::string buf =
      absl::StrCat(ConstStringView("some garbage leftover content\x03\x00\x00\x00"),
                   testutils::GenRequestPacket(Command::kQuery, "SELECT foo"),
                   testutils::GenRequestPacket(Command::kStmtPrepare, "blahblahblah"));
  StateWrapper state{};

  // FindFrameBoundary() should cut out the garbage text.
  size_t pos = FindFrameBoundary<mysql::Packet>(message_type_t::kRequest, buf, 0, &state);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos),
            absl::StrCat(testutils::GenRequestPacket(Command::kQuery, "SELECT foo"),
                         testutils::GenRequestPacket(Command::kStmtPrepare, "blahblahblah")));
}

TEST_F(MySQLParserTest, FindReqBoundaryWithStartPos) {
  const std::string buf =
      absl::StrCat(testutils::GenRequestPacket(Command::kQuery, "SELECT foo"),
                   testutils::GenRequestPacket(Command::kStmtPrepare, "blahblahblah"));
  StateWrapper state{};

  size_t pos = FindFrameBoundary<mysql::Packet>(message_type_t::kRequest, buf, 1, &state);
  ASSERT_NE(pos, std::string::npos);
  EXPECT_EQ(buf.substr(pos), testutils::GenRequestPacket(Command::kStmtPrepare, "blahblahblah"));
}

TEST_F(MySQLParserTest, FindNoBoundary) {
  const std::string_view buf = ConstStringView(
      "This is a bogus string in which there is no MySQL "
      "protocol\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00");
  StateWrapper state{};

  size_t pos = FindFrameBoundary<mysql::Packet>(message_type_t::kRequest, buf, 0, &state);
  EXPECT_EQ(pos, std::string::npos);
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
