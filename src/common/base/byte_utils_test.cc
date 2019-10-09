#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/byte_utils.h"
#include "src/common/base/types.h"

namespace pl {
namespace utils {

TEST(UtilsTest, TestIntToLEBytes) {
  {
    char result[4];
    IntToLEBytes(0x12345678, result);
    char expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }

  {
    char result[3];
    IntToLEBytes(198, result);
    char expected[] = "\xc6\x00\x00";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
  }

  {
    uint8_t result[4];
    IntToLEBytes(0x12345678, result);
    uint8_t expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }
}

TEST(UtilsTest, TestReverseBytes) {
  {
    char result[4];
    char input[] = {'\x12', '\x34', '\x56', '\x78'};
    ReverseBytes(input, result);
    char expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }

  {
    uint8_t result[4];
    uint8_t input[] = {'\x12', '\x34', '\x56', '\x78'};
    ReverseBytes(input, result);
    uint8_t expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }
}

TEST(UtilsTest, TestLEStrToInt) {
  EXPECT_EQ(LEStrToInt(std::string(ConstStringView("\x78\x56\x34\x12"))), 305419896);

  // Testing for unsigned correctness.
  EXPECT_EQ(LEStrToInt(std::string(ConstStringView("\xc6\x00\x00"))), 198);
}

}  // namespace utils
}  // namespace pl
