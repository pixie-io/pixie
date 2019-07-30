#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <random>
#include <utility>
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

class MySQLParserTest : public ::testing::Test {
 protected:
  // TODO(chengruizhe): Define GenResponse to generate responses.
  MySQLReqResp kMySQLStmtPrepareMessage = {
      testutils::GenRequest(
          MySQLParser::kComStmtPrepare,
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
      MySQLEventType::kComStmtPrepare};
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
  expected_message1.type = MySQLEventType::kComStmtPrepare;
  expected_message1.msg =
      absl::StrCat(MySQLParser::kComStmtPrepare, "SELECT name FROM users WHERE id = ?");

  MySQLMessage expected_message2;
  expected_message2.type = MySQLEventType::kComStmtPrepare;
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
  expected_message1.type = MySQLEventType::kComStmtExecute;
  expected_message1.msg = absl::StrCat(MySQLParser::kComStmtExecute, body);

  parser_.Append(msg1, 0);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(MySQLParserTest, ParseComQuery) {
  std::string msg1 = testutils::GenRequest(MySQLParser::kComQuery, "SELECT name FROM users");
  std::string msg2 = testutils::GenRequest(MySQLParser::kComQuery, "SELECT age FROM users");

  MySQLMessage expected_message1;
  expected_message1.type = MySQLEventType::kComQuery;
  expected_message1.msg = absl::StrCat(MySQLParser::kComQuery, "SELECT name FROM users");

  MySQLMessage expected_message2;
  expected_message2.type = MySQLEventType::kComQuery;
  expected_message2.msg = absl::StrCat(MySQLParser::kComQuery, "SELECT age FROM users");

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequests, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}
// TODO(chengruizhe): Add test cases for failure scenarios.

TEST_F(MySQLParserTest, ParseResponse) {
  parser_.Append(kMySQLStmtPrepareMessage.response, 0);

  std::deque<MySQLMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponses, &parsed_messages);
  EXPECT_EQ(ParseState::kSuccess, result.state);

  MySQLMessage expected_header;
  expected_header.msg = std::string("\x00\x1a\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00", 12);

  MySQLMessage expected_col_def;
  expected_col_def.msg = std::string(
      "\x03\x64\x65\x66\x00\x00\x00\x1c\x43\x4f\x55\x4e\x54\x28\x44\x49\x53\x54\x49\x4e\x43\x54\x20"
      "\x73\x6f\x63\x6b\x2e\x73\x6f\x63\x6b\x5f\x69\x64\x29\x00\x0c\x3f\x00\x15\x00\x00\x00\x08\x81"
      "\x00\x00\x00\x00",
      50);

  MySQLMessage expected_eof;
  expected_eof.msg = std::string("\xfe\x00\x00\x02\x00", 5);

  EXPECT_THAT(parsed_messages, ElementsAre(expected_header, expected_col_def, expected_eof));
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
