#include "src/stirling/utils/utils.h"

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

TEST(RemoveRepeatingSuffixTest, RemoveChars) {
  {
    std::string_view str = "testaaa";

    RemoveRepeatingSuffix(&str, 'b');
    EXPECT_EQ("testaaa", str);

    RemoveRepeatingSuffix(&str, 'a');
    EXPECT_EQ("test", str);
  }
  {
    std::string_view str = CreateStringView<char>("test\0\0\0");

    RemoveRepeatingSuffix(&str, 'b');
    EXPECT_EQ(CreateStringView<char>("test\0\0\0"), str);

    RemoveRepeatingSuffix(&str, '\0');
    EXPECT_EQ("test", str);
  }
}

}  // namespace stirling
}  // namespace pl
