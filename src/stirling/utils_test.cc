#include "src/stirling/utils.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pl {
namespace stirling {

class UtilsTest : public ::testing::Test {};

TEST_F(UtilsTest, TestIntToLEBytes) {
  char result[4];
  IntToLEBytes<4>(305419896, result);
  char expected[] = "\x78\x56\x34\x12";
  EXPECT_EQ(result[0], expected[0]);
  EXPECT_EQ(result[1], expected[1]);
  EXPECT_EQ(result[2], expected[2]);
  EXPECT_EQ(result[3], expected[3]);
}

TEST_F(UtilsTest, TestBEBytesToInt) {
  char input[] = "\x12\x34\x56\x78";
  int result = BEBytesToInt(input, 4);
  EXPECT_EQ(result, 305419896);
}

TEST_F(UtilsTest, TestEndianSwap) {
  char result[4];
  char input[] = "\x12\x34\x56\x78";
  EndianSwap<4>(input, result);
  char expected[] = "\x78\x56\x34\x12";
  EXPECT_EQ(result[0], expected[0]);
  EXPECT_EQ(result[1], expected[1]);
  EXPECT_EQ(result[2], expected[2]);
  EXPECT_EQ(result[3], expected[3]);
}

TEST_F(UtilsTest, TestLEStrToInt) {
  std::string input = "\x78\x56\x34\x12";
  EXPECT_EQ(LEStrToInt(input), 305419896);
}

}  // namespace stirling
}  // namespace pl
