#include "src/stirling/utils/byte_format.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/common/base/types.h"

namespace pl {
namespace stirling {
namespace utils {

class UtilsTest : public ::testing::Test {};

TEST_F(UtilsTest, TestIntToLEBytes) {
  char result[4];
  IntToLEBytes(305419896, result);
  char expected[] = "\x78\x56\x34\x12";
  EXPECT_EQ(result[0], expected[0]);
  EXPECT_EQ(result[1], expected[1]);
  EXPECT_EQ(result[2], expected[2]);
  EXPECT_EQ(result[3], expected[3]);

  // Testing for unsigned correctness.
  char result2[3];
  IntToLEBytes(198, result2);
  char expected2[] = "\xc6\x00\x00";
  EXPECT_EQ(result2[0], expected2[0]);
  EXPECT_EQ(result2[1], expected2[1]);
  EXPECT_EQ(result2[2], expected2[2]);
}

TEST_F(UtilsTest, TestReverseBytes) {
  char result[4];
  char input[] = {'\x12', '\x34', '\x56', '\x78'};
  ReverseBytes(input, result);
  char expected[] = "\x78\x56\x34\x12";
  EXPECT_EQ(result[0], expected[0]);
  EXPECT_EQ(result[1], expected[1]);
  EXPECT_EQ(result[2], expected[2]);
  EXPECT_EQ(result[3], expected[3]);
}

TEST_F(UtilsTest, TestLEStrToInt) {
  EXPECT_EQ(LEStrToInt(std::string(ConstStringView("\x78\x56\x34\x12"))), 305419896);

  // Testing for unsigned correctness.
  EXPECT_EQ(LEStrToInt(std::string(ConstStringView("\xc6\x00\x00"))), 198);
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
