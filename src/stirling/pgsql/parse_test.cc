#include "src/stirling/pgsql/parse.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace pgsql {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::SizeIs;

auto IsRegularMessage(char tag, int32_t len, std::string_view payload) {
  return AllOf(Field(&RegularMessage::tag, tag), Field(&RegularMessage::len, len),
               Field(&RegularMessage::payload, payload));
}

TEST(PGSQLParseTest, BasicMessage) {
  std::string_view data = CreateStringView<char>("Q\000\000\000\033select * from account;\000");
  RegularMessage msg = {};
  EXPECT_EQ(ParseState::kSuccess, ParseRegularMessage(&data, &msg));
  EXPECT_THAT(msg, IsRegularMessage('Q', 27, CreateStringView<char>("select * from account;\0")));
}

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
