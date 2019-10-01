#include "src/stirling/utils/json.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pl {
namespace stirling {
namespace utils {

using ::testing::StrEq;

TEST(WriteMapAsJSONTest, ResultsAreAsExpected) {
  std::map<std::string, std::string> map = {{"foo", "bar"}, {"tom", "jerry"}};
  EXPECT_THAT(WriteMapAsJSON(map), StrEq(R"({"foo":"bar","tom":"jerry"})"));
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
