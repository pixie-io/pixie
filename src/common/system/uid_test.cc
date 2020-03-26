#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/system/uid.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace system {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

TEST(NameForUIDTest, NameIsNotEmpty) { EXPECT_OK_AND_THAT(NameForUID(getuid()), Not(IsEmpty())); }

TEST(ParsePasswdTest, ResultsAreAsExpected) {
  const char kSamplePasswd[] =
      "root:x:0:0:root:/root:/bin/bash\n"
      "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
      "bin:x:2:2:bin:/bin:/usr/sbin/nologin";
  EXPECT_THAT(ParsePasswd(kSamplePasswd),
              ElementsAre(Pair(0, "root"), Pair(1, "daemon"), Pair(2, "bin")));
}

}  // namespace system
}  // namespace pl
