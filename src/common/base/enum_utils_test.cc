#include "src/common/base/enum_utils.h"
#include "src/common/testing/testing.h"

using ::testing::ElementsAre;
using ::testing::Pair;

enum class Color { kRed = 2, kBlue = 4, kGreen = 8 };

TEST(EnumUtils, EnumDef) {
  std::map<int64_t, std::string_view> enum_def = px::EnumDefToMap<Color>();

  EXPECT_THAT(enum_def, ElementsAre(Pair(2, "kRed"), Pair(4, "kBlue"), Pair(8, "kGreen")));
}
