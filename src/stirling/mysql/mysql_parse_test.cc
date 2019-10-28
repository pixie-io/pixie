#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/mysql/mysql_parse.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace mysql {

struct MySQLReqResp {
  std::string request;
  std::string response;
  MySQLEventType type;
};

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

class MySQLParserTest : public ::testing::Test {
 protected:
  EventParser<Packet> parser_;
};

bool operator==(const Packet& lhs, const Packet& rhs) {
  if (lhs.msg.compare(rhs.msg) != 0) {
    return false;
  }
  if (lhs.sequence_id != rhs.sequence_id) {
    return false;
  }
  return true;
}

TEST_F(MySQLParserTest, ParseRaw) {
  std::string packet0 = testutils::GenRawPacket(0, "\x03SELECT foo");
  std::string packet1 = testutils::GenRawPacket(1, "\x03SELECT bar");

  parser_.Append(packet0, 0);
  parser_.Append(packet1, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  Packet expected_message0;
  expected_message0.msg = "\x03SELECT foo";
  expected_message0.sequence_id = 0;

  Packet expected_message1;
  expected_message1.msg = "\x03SELECT bar";
  expected_message1.sequence_id = 1;

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message0, expected_message1));
}

TEST_F(MySQLParserTest, ParseComStmtPrepare) {
  std::string msg1 =
      testutils::GenRequest(MySQLEventType::kStmtPrepare, "SELECT name FROM users WHERE id = ?");
  std::string msg2 =
      testutils::GenRequest(MySQLEventType::kStmtPrepare, "SELECT age FROM users WHERE id = ?");

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(CommandToString(MySQLEventType::kStmtPrepare),
                                       "SELECT name FROM users WHERE id = ?");
  expected_message1.sequence_id = 0;

  Packet expected_message2;
  expected_message2.msg = absl::StrCat(CommandToString(MySQLEventType::kStmtPrepare),
                                       "SELECT age FROM users WHERE id = ?");
  expected_message2.sequence_id = 0;

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseComStmtExecute) {
  // body of the MySQL StmtExecute message, from
  // https://dev.mysql.com/doc/internals/en/com-stmt-execute.html.
  const std::string body(
      ConstStringView("\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x0f\x00\x03\x66\x6f\x6f"));
  std::string msg1 = testutils::GenRequest(MySQLEventType::kStmtExecute, body);

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(CommandToString(MySQLEventType::kStmtExecute), body);
  expected_message1.sequence_id = 0;

  parser_.Append(msg1, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, ParseComStmtClose) {
  Packet expected_packet = testutils::GenStmtCloseRequest(testdata::kStmtCloseRequest);
  std::string msg = testutils::GenRawPacket(expected_packet);

  parser_.Append(msg, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_packet));
}

TEST_F(MySQLParserTest, ParseComQuery) {
  std::string msg1 = testutils::GenRequest(MySQLEventType::kQuery, "SELECT name FROM users");
  std::string msg2 = testutils::GenRequest(MySQLEventType::kQuery, "SELECT age FROM users");

  Packet expected_message1;
  expected_message1.msg =
      absl::StrCat(CommandToString(MySQLEventType::kQuery), "SELECT name FROM users");
  expected_message1.sequence_id = 0;

  Packet expected_message2;
  expected_message2.msg =
      absl::StrCat(CommandToString(MySQLEventType::kQuery), "SELECT age FROM users");
  expected_message2.sequence_id = 0;

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseResponse) {
  // TODO(chengruizhe): Define GenResponse to generate responses.
  MySQLReqResp kMySQLStmtPrepareMessage = {
      testutils::GenRequest(
          MySQLEventType::kStmtPrepare,
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
      MySQLEventType::kStmtPrepare};

  parser_.Append(kMySQLStmtPrepareMessage.response, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);
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

  EXPECT_THAT(parsed_messages, ElementsAre(expected_header, expected_col_def, expected_eof));
}

TEST_F(MySQLParserTest, ParseMultipleRawPackets) {
  std::deque<Packet> prepare_resp_packets =
      testutils::GenStmtPrepareOKResponse(testdata::kStmtPrepareResponse);
  std::deque<Packet> execute_resp_packets =
      testutils::GenResultset(testdata::kStmtExecuteResultset);

  // Splitting packets from 2 responses into 3 different raw packet chunks.
  std::vector<std::string> packets1;
  for (size_t i = 0; i < 3; ++i) {
    packets1.push_back(testutils::GenRawPacket(prepare_resp_packets[i]));
  }

  std::vector<std::string> packets2;
  for (size_t i = 3; i < prepare_resp_packets.size(); ++i) {
    packets2.push_back(testutils::GenRawPacket(prepare_resp_packets[i]));
  }
  for (size_t i = 0; i < 2; ++i) {
    packets2.push_back(testutils::GenRawPacket(execute_resp_packets[i]));
  }

  std::vector<std::string> packets3;
  for (size_t i = 2; i < execute_resp_packets.size(); ++i) {
    packets3.push_back(testutils::GenRawPacket(execute_resp_packets[i]));
  }

  std::string chunk1 = absl::StrJoin(packets1, "");
  std::string chunk2 = absl::StrJoin(packets2, "");
  std::string chunk3 = absl::StrJoin(packets3, "");

  parser_.Append(chunk1, 0);
  parser_.Append(chunk2, 1);
  parser_.Append(chunk3, 2);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  std::deque<Packet> expected_packets;
  for (Packet p : prepare_resp_packets) {
    expected_packets.push_back(p);
  }
  for (Packet p : execute_resp_packets) {
    expected_packets.push_back(p);
  }

  EXPECT_EQ(expected_packets.size(), parsed_messages.size());
  EXPECT_THAT(parsed_messages, ElementsAreArray(expected_packets));
}

TEST_F(MySQLParserTest, ParseIncompleteRequest) {
  std::string msg1 =
      testutils::GenRequest(MySQLEventType::kStmtPrepare, "SELECT name FROM users WHERE");
  // Change the length of the request so that it isn't complete.
  msg1[0] = '\x24';

  parser_.Append(msg1, 0);
  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

TEST_F(MySQLParserTest, ParseInvalidInput) {
  std::string msg1 = "hello world";

  parser_.Append(msg1, 0);
  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);
  EXPECT_EQ(ParseState::kInvalid, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

TEST_F(MySQLParserTest, NoAppend) {
  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
