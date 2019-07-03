#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>
#include "src/stirling/mysql_parse.h"

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

TEST_F(MySQLParserTest, Parse_COM_STMT_PREPARE) {
  // Packet Length [3]; Packet Number [1];
  std::string msg1 = std::string{"\x24\x00\x00\x00\x16", 5};
  msg1 += "SELECT name FROM users WHERE id = ?";

  std::string msg2 = std::string{"\x23\x00\x00\x00\x16", 5};
  msg2 += "SELECT age FROM users WHERE id = ?";

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kMySQLComStmtPrepare;
  expected_message1.msg = COM_STMT_PREPARE + "SELECT name FROM users WHERE id = ?";

  MySQLMessage expected_message2;
  expected_message2.type = MySQLEventType::kMySQLComStmtPrepare;
  expected_message2.msg = COM_STMT_PREPARE + "SELECT age FROM users WHERE id = ?";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

}  // namespace stirling
}  // namespace pl
