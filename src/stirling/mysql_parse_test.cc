#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>

#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/mysql_parse.h"

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
  // TODO(chengruizhe): Define GenResponse to generate responses.
  MySQLReqResp kMySQLStmtPrepareMessage = {
      testutils::GenRequest(
          kComStmtPrepare,
          "SELECT COUNT(DISTINCT sock.sock_id) FROM sock JOIN sock_tag ON "
          "sock.sock_id=sock_tag.sock_id JOIN tag ON sock_tag.tag_id=tag.tag_id;"),
      // Response
      absl::StrCat(
          // Header packet
          std::string("\x0c\x00\x00\x01\x00\x1a\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00", 16),
          // Column def packet
          std::string("\x32\x00\x00\x02\x03\x64\x65\x66\x00\x00\x00\x1c\x43\x4f\x55\x4e\x54\x28\x44"
                      "\x49\x53\x54\x49\x4e\x43\x54\x20\x73\x6f\x63\x6b\x2e\x73\x6f\x63\x6b\x5f\x69"
                      "\x64\x29\x00\x0c\x3f\x00\x15\x00\x00\x00\x08\x81\x00\x00\x00\x00",
                      54),
          // EOF packet
          std::string("\x05\x00\x00\x03\xfe\x00\x00\x02\x00", 9)),
      MySQLEventType::kStmtPrepare};
  EventParser<Packet> parser_;
};

bool operator==(const Packet& lhs, const Packet& rhs) {
  if (lhs.msg.compare(rhs.msg) == 0) {
    return true;
  }
  return false;
}

TEST_F(MySQLParserTest, ParseComStmtPrepare) {
  std::string msg1 = testutils::GenRequest(kComStmtPrepare, "SELECT name FROM users WHERE id = ?");
  std::string msg2 = testutils::GenRequest(kComStmtPrepare, "SELECT age FROM users WHERE id = ?");

  Packet expected_message1;
  expected_message1.msg =
      absl::StrCat(std::string(1, kComStmtPrepare), "SELECT name FROM users WHERE id = ?");

  Packet expected_message2;
  expected_message2.msg =
      absl::StrCat(std::string(1, kComStmtPrepare), "SELECT age FROM users WHERE id = ?");

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
  std::string msg1 = testutils::GenRequest(kComStmtExecute, body);

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(std::string(1, kComStmtExecute), body);

  parser_.Append(msg1, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, ParseComStmtClose) {
  Packet expected_packet = testutils::GenStmtCloseRequest(testutils::kStmtCloseRequest);
  std::string msg = testutils::GenRawPacket(0, expected_packet.msg);

  parser_.Append(msg, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_packet));
}

TEST_F(MySQLParserTest, ParseComQuery) {
  std::string msg1 = testutils::GenRequest(kComQuery, "SELECT name FROM users");
  std::string msg2 = testutils::GenRequest(kComQuery, "SELECT age FROM users");

  Packet expected_message1;
  expected_message1.msg = absl::StrCat(std::string(1, kComQuery), "SELECT name FROM users");

  Packet expected_message2;
  expected_message2.msg = absl::StrCat(std::string(1, kComQuery), "SELECT age FROM users");

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseResponse) {
  parser_.Append(kMySQLStmtPrepareMessage.response, 0);

  std::deque<Packet> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);
  EXPECT_EQ(ParseState::kSuccess, result.state);

  Packet expected_header;
  expected_header.msg = ConstStringView("\x00\x1a\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00");

  Packet expected_col_def;
  expected_col_def.msg = ConstStringView(
      "\x03\x64\x65\x66\x00\x00\x00\x1c\x43\x4f\x55\x4e\x54\x28\x44\x49\x53\x54\x49\x4e\x43\x54\x20"
      "\x73\x6f\x63\x6b\x2e\x73\x6f\x63\x6b\x5f\x69\x64\x29\x00\x0c\x3f\x00\x15\x00\x00\x00\x08\x81"
      "\x00\x00\x00\x00");

  Packet expected_eof;
  expected_eof.msg = ConstStringView("\xfe\x00\x00\x02\x00");

  EXPECT_THAT(parsed_messages, ElementsAre(expected_header, expected_col_def, expected_eof));
}

TEST_F(MySQLParserTest, ParseMultipleRawPackets) {
  std::deque<Packet> prepare_resp_packets =
      testutils::GenStmtPrepareOKResponse(testutils::kStmtPrepareResponse);
  std::deque<Packet> execute_resp_packets =
      testutils::GenResultset(testutils::kStmtExecuteResultset);

  // Splitting packets from 2 responses into 3 different raw packet chunks.
  std::vector<std::string> packets1;
  for (size_t i = 0; i < 3; ++i) {
    packets1.push_back(testutils::GenRawPacket(i, prepare_resp_packets[i].msg));
  }

  std::vector<std::string> packets2;
  for (size_t i = 3; i < prepare_resp_packets.size(); ++i) {
    packets2.push_back(testutils::GenRawPacket(i, prepare_resp_packets[i].msg));
  }
  for (size_t i = 0; i < 2; ++i) {
    packets2.push_back(testutils::GenRawPacket(i, execute_resp_packets[i].msg));
  }

  std::vector<std::string> packets3;
  for (size_t i = 2; i < execute_resp_packets.size(); ++i) {
    packets3.push_back(testutils::GenRawPacket(i, execute_resp_packets[i].msg));
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
  std::string msg1 = testutils::GenRequest(kComStmtPrepare, "SELECT name FROM users WHERE");
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
