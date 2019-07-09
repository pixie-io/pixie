#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>
#include "src/stirling/mysql_parse.h"

namespace pl {
namespace stirling {

namespace {

template <size_t N>
void IntToLEBytes(int num, char result[N]) {
  for (size_t i = 0; i < N; i++) {
    result[i] = (num >> (i * 8));
  }
}

std::string GenPacket(const std::string& msg, const ConstStrView& command) {
  char len_bytes[4];
  IntToLEBytes<4>(msg.size() + 1, len_bytes);
  return absl::StrCat(std::string(len_bytes, 4), "\x00", command, msg);
}
}  // namespace

using ::testing::ElementsAre;

class MySQLParserTest : public ::testing::Test {
 protected:
  EventParser<MySQLMessage> parser_;
};

bool operator==(const MySQLMessage& lhs, const MySQLMessage& rhs) {
  if (lhs.type == rhs.type && lhs.msg.compare(rhs.msg) == 0) {
    return true;
  }
  return false;
}

TEST_F(MySQLParserTest, IntToBytes) {
  char result[4];
  IntToLEBytes<4>(24, result);
  char expected_bytes[] = {'\x18', '\x00', '\x00', '\x00'};
  EXPECT_FALSE(strcmp(result, expected_bytes));
}

TEST_F(MySQLParserTest, ParseComStmtPrepare) {
  std::string msg1 = GenPacket("SELECT name FROM users WHERE id = ?", MySQLParser::kComStmtPrepare);
  std::string msg2 = GenPacket("SELECT age FROM users WHERE id = ?", MySQLParser::kComStmtPrepare);

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kMySQLComStmtPrepare;
  expected_message1.msg =
      absl::StrCat(MySQLParser::kComStmtPrepare, "SELECT name FROM users WHERE id = ?");

  MySQLMessage expected_message2;
  expected_message2.type = MySQLEventType::kMySQLComStmtPrepare;
  expected_message2.msg =
      absl::StrCat(MySQLParser::kComStmtPrepare, "SELECT age FROM users WHERE id = ?");

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST_F(MySQLParserTest, ParseComStmtExecute) {
  // body of the MySQL StmtExecute message, from
  // https://dev.mysql.com/doc/internals/en/com-stmt-execute.html.
  const std::string body =
      std::string("\x01\x00\x00\x00\x00\x01\x00\x00\x00\x00\x01\x0f\x00\x03\x66\x6f\x6f", 17);
  std::string msg1 = GenPacket(body, MySQLParser::kComStmtExecute);

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kMySQLComStmtExecute;
  expected_message1.msg = absl::StrCat(MySQLParser::kComStmtExecute, body);

  parser_.Append(msg1, 0);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, ParseComQuery) {
  std::string msg1 = GenPacket("SELECT name FROM users", MySQLParser::kComQuery);
  std::string msg2 = GenPacket("SELECT age FROM users", MySQLParser::kComQuery);

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kMySQLComQuery;
  expected_message1.msg = absl::StrCat(MySQLParser::kComQuery, "SELECT name FROM users");

  MySQLMessage expected_message2;
  expected_message2.type = MySQLEventType::kMySQLComQuery;
  expected_message2.msg = absl::StrCat(MySQLParser::kComQuery, "SELECT age FROM users");

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}
// TODO(chengruizhe): Add test cases for failure scenarios

}  // namespace stirling
}  // namespace pl
