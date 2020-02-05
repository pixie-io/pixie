#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/system/uid.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace system {

using ::testing::IsEmpty;
using ::testing::Not;

TEST(NameForUIDTest, NameIsNotEmpty) { EXPECT_OK_AND_THAT(NameForUID(getuid()), Not(IsEmpty())); }

}  // namespace system
}  // namespace pl
