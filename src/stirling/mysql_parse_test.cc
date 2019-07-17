#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>
#include "src/stirling/mysql_parse.h"
#include "src/stirling/mysql_test_utils.h"

namespace pl {
namespace stirling {

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

TEST_F(MySQLParserTest, ParseComStmtPrepare) {
  std::string msg1 =
      testutils::GenRequest(MySQLParser::kComStmtPrepare, "SELECT name FROM users WHERE id = ?");
  std::string msg2 =
      testutils::GenRequest(MySQLParser::kComStmtPrepare, "SELECT age FROM users WHERE id = ?");

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
  std::string msg1 = testutils::GenRequest(MySQLParser::kComStmtExecute, body);

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kMySQLComStmtExecute;
  expected_message1.msg = absl::StrCat(MySQLParser::kComStmtExecute, body);

  parser_.Append(msg1, 0);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, DISABLED_ParseComQuery) {
  std::string msg1 = testutils::GenRequest(MySQLParser::kComQuery, "SELECT name FROM users");
  std::string msg2 = testutils::GenRequest(MySQLParser::kComQuery, "SELECT age FROM users");

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
