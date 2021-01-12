#include "src/stirling/source_connectors/jvm_stats/utils/hsperfdata.h"

#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/testing/test_environment.h"

namespace pl {
namespace stirling {
namespace java {
namespace hsperf {

using ::pl::testing::TestFilePath;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(PerfDataHeaderTest, ReadFromBytes) {
  ASSERT_OK_AND_ASSIGN(const std::string content,
                       ReadFileToString(TestFilePath(
                           "src/stirling/source_connectors/jvm_stats/utils/test_hsperfdata")));

  HsperfData data;
  EXPECT_OK(ParseHsperfData(std::move(content), &data));
  EXPECT_THAT(data.data_entries, SizeIs(199));
}

// Tests that error is returned if there is not enough data.
TEST(PerfDataHeaderTest, NotEnoughData) {
  {
    HsperfData data = {};
    auto status = ParseHsperfData("", &data);
    EXPECT_NOT_OK(status);
    EXPECT_EQ("Not enough data", status.msg());
  }
  {
    HsperfData data = {};
    constexpr uint8_t buf[] = {0xCA,
                               0xFE,
                               0xC0,
                               0xC0,
                               /*byte_order*/ 0x00,
                               /*major_version*/ 0x00,
                               /*minor_version*/ 0x00,
                               /*accessible*/ 0x00,
                               /*used*/ 0x00,
                               0x00,
                               0x00,
                               0x01,
                               /*overflow*/ 0x00,
                               0x00,
                               0x00,
                               0x01,
                               /*mod_timestamp*/ 0x00,
                               0x00,
                               0x00,
                               0x01,
                               0x00,
                               0x00,
                               0x00,
                               0x01,
                               /*entry_offset*/ 0xff,
                               0x00,
                               0x00,
                               0x00};
    // entry_offset is set to 255, which is larger than the buf size.
    auto status =
        ParseHsperfData(std::string(reinterpret_cast<const char*>(buf), sizeof(buf)), &data);
    EXPECT_NOT_OK(status);
    EXPECT_EQ("Entry offset 255 is beyond the buffer size 28", status.msg());
  }
}

}  // namespace hsperf
}  // namespace java
}  // namespace stirling
}  // namespace pl
