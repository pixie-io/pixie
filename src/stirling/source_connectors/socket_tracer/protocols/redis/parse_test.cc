#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

using ::testing::IsEmpty;
using ::testing::StrEq;

constexpr std::string_view kSimpleStringMsg = "+OK\r\n";
constexpr std::string_view kErrorMsg = "-Error message\r\n";
constexpr std::string_view kBulkStringMsg = "$11\r\nbulk string\r\n";
constexpr std::string_view kArrayMsg = "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string\r\n";
constexpr std::string_view kCmdMsg = "*2\r\n+ACL\r\n+LOAD\r\n";
constexpr std::string_view kPubMsg = "*3\r\n$7\r\nmessage\r\n$3\r\nfoo\r\n$4\r\ntest\r\n";

struct WellFormedTestCase {
  std::string_view input;
  std::string_view expected_payload;
  std::string_view expected_command;
};

std::ostream& operator<<(std::ostream& os, const WellFormedTestCase& test_case) {
  os << "input: " << test_case.input << " payload: " << test_case.expected_payload
     << " command: " << test_case.expected_command;
  return os;
}

std::ostream& operator<<(std::ostream& os, ParseState state) {
  os << magic_enum::enum_name(state);
  return os;
}

class ParseTest : public ::testing::TestWithParam<WellFormedTestCase> {};

TEST_P(ParseTest, ResultsAreAsExpected) {
  std::string_view req = GetParam().input;
  Message msg;

  EXPECT_EQ(ParseMessage(MessageType::kRequest, &req, &msg), ParseState::kSuccess);
  EXPECT_THAT(req, IsEmpty());
  EXPECT_THAT(msg.payload, StrEq(std::string(GetParam().expected_payload)));
  EXPECT_THAT(std::string(msg.command), StrEq(std::string(GetParam().expected_command)));
}

INSTANTIATE_TEST_SUITE_P(
    AllDataTypes, ParseTest,
    ::testing::Values(
        WellFormedTestCase{kSimpleStringMsg, R"("OK")", ""},
        WellFormedTestCase{kErrorMsg, R"("Error message")", ""},
        WellFormedTestCase{kBulkStringMsg, R"("bulk string")", ""},
        WellFormedTestCase{"$0\r\n\r\n", R"("")", ""}, WellFormedTestCase{"$-1\r\n", "<NULL>", ""},
        WellFormedTestCase{kArrayMsg, R"(["OK", "Error message", "bulk string"])", ""},
        WellFormedTestCase{kCmdMsg, R"(["ACL", "LOAD"])", "ACL LOAD"},
        WellFormedTestCase{"*1\r\n$-1\r\n", "[<NULL>]", ""},
        WellFormedTestCase{"*-1\r\n", "[NULL]", ""}, WellFormedTestCase{"*0\r\n", "[]", ""}));

TEST(ParsePubMsgTest, DetectPublishedMessage) {
  std::string_view resp = kPubMsg;
  Message msg;

  EXPECT_EQ(ParseMessage(MessageType::kResponse, &resp, &msg), ParseState::kSuccess);
  EXPECT_THAT(resp, IsEmpty());
  EXPECT_THAT(msg.payload, StrEq(R"(["message", "foo", "test"])"));
  EXPECT_TRUE(msg.is_published_message);
}

class ParseIncompleteInputTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ParseIncompleteInputTest, IncompleteInput) {
  std::string original_input = GetParam();
  std::string_view input = GetParam();
  Message msg;

  EXPECT_EQ(ParseMessage(MessageType::kRequest, &input, &msg), ParseState::kNeedsMoreData);
  EXPECT_THAT(std::string(input), StrEq(original_input));
}

INSTANTIATE_TEST_SUITE_P(
    AllDataTypes, ParseIncompleteInputTest,
    ::testing::Values("+OK\r", "+OK", "+", "-Error message\r", "-Error message", "-",
                      "$11\r\nbulk string\r", "$11\r\nbulk string", "$11\r\nbulk", "$11\r\n",
                      "$11\r", "$11", "$", "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string\r",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk string",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\nbulk ",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r\n",
                      "*3\r\n+OK\r\n-Error message\r\n$11\r", "*3\r\n+OK\r\n-Error message\r\n$11",
                      "*3\r\n+OK\r\n-Error message\r\n", "*3\r\n+OK\r\n-Error message\r",
                      "*3\r\n+OK\r\n-Error message", "*3\r\n+OK\r\n", "*3\r\n+OK\r", "*3\r\n+OK",
                      "*3\r\n", "*3\r", "*3"));

class ParseInvalidInputTest : public ::testing::TestWithParam<std::string> {};

TEST_P(ParseInvalidInputTest, InvalidInput) {
  std::string original_input = GetParam();
  std::string_view input = GetParam();
  Message msg;

  EXPECT_EQ(ParseMessage(MessageType::kRequest, &input, &msg), ParseState::kInvalid);
  EXPECT_THAT(std::string(input), StrEq(original_input));
}

INSTANTIATE_TEST_SUITE_P(AllDataTypes, ParseInvalidInputTest,
                         ::testing::Values(
                             // Invalid markers
                             "a", "b", "c",
                             // Bulk string should end with \r\n
                             "$1\r\nabc"
                             // Length cannot be <-1
                             "$-2\r\n"
                             // Length cannot be <-1
                             "*-2\r\n"));

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
