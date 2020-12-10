#include "src/stirling/protocols/redis/parse.h"

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

using ::testing::IsEmpty;
using ::testing::StrEq;

constexpr std::string_view kOKMsg = "+OK\r\n";
constexpr std::string_view kErrorMsg = "-Error message\r\n";
constexpr std::string_view kBulkStringMsg = "$11\r\nbulk string\r\n";

struct TestCase {
  std::string_view input;
  DataType expected_data_type;
  std::string_view expected_payload;
};

class ParseTest : public ::testing::TestWithParam<TestCase> {};

TEST_P(ParseTest, ResultsAreAsExpected) {
  std::string_view req = GetParam().input;
  Message msg;

  EXPECT_EQ(ParseMessage(&req, &msg), ParseState::kSuccess);
  EXPECT_THAT(req, IsEmpty());
  EXPECT_EQ(msg.data_type, GetParam().expected_data_type);
  EXPECT_THAT(std::string(msg.payload), StrEq(std::string(GetParam().expected_payload)));
}

INSTANTIATE_TEST_SUITE_P(AllDataTypes, ParseTest,
                         ::testing::Values(TestCase{kOKMsg, DataType::kSimpleString, "OK"},
                                           TestCase{kErrorMsg, DataType::kError, "Error message"},
                                           TestCase{kBulkStringMsg, DataType::kBulkString,
                                                    "bulk string"}));

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
