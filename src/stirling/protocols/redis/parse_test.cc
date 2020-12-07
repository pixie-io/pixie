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

TEST(ParseTest, SimpleString) {
  std::string_view req = kOKMsg;
  Message msg;

  EXPECT_EQ(ParseMessage(&req, &msg), ParseState::kSuccess);
  EXPECT_THAT(req, IsEmpty());
  EXPECT_EQ(msg.data_type, DataType::kSimpleString);
  EXPECT_THAT(std::string(msg.payload), StrEq("OK"));
}

TEST(ParseTest, ErrorString) {
  std::string_view req = kErrorMsg;
  Message msg;

  EXPECT_EQ(ParseMessage(&req, &msg), ParseState::kSuccess);
  EXPECT_THAT(req, IsEmpty());
  EXPECT_EQ(msg.data_type, DataType::kErrors);
  EXPECT_THAT(std::string(msg.payload), StrEq("Error message"));
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
