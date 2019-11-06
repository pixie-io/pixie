#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/byte_utils.h"
#include "src/common/base/types.h"

namespace pl {
namespace utils {

TEST(UtilsTest, TestIntToLittleEndianByteStr) {
  {
    char result[4];
    IntToLittleEndianByteStr(0x12345678, result);
    char expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }

  {
    char result[3];
    IntToLittleEndianByteStr(198, result);
    char expected[] = "\xc6\x00\x00";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
  }

  {
    uint8_t result[4];
    IntToLittleEndianByteStr(0x12345678, result);
    uint8_t expected[] = "\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
  }

  {
    uint8_t result[8];
    IntToLittleEndianByteStr(0x123456789abcdef0, result);
    uint8_t expected[] = "\xf0\xde\xbc\x9a\x78\x56\x34\x12";
    EXPECT_EQ(result[0], expected[0]);
    EXPECT_EQ(result[1], expected[1]);
    EXPECT_EQ(result[2], expected[2]);
    EXPECT_EQ(result[3], expected[3]);
    EXPECT_EQ(result[4], expected[4]);
    EXPECT_EQ(result[5], expected[5]);
    EXPECT_EQ(result[6], expected[6]);
    EXPECT_EQ(result[7], expected[7]);
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

TEST(UtilsTest, TestLittleEndianByteStrToInt) {
  // Note that int32_t vs uint32_t doesn't really make a difference.
  // So just use the more convenient receiver type.

  // uint32_t cases.
  EXPECT_EQ(LittleEndianByteStrToInt<uint32_t>(ConstString("\x78\x56\x34\x12")), 0x12345678);
  EXPECT_EQ(LittleEndianByteStrToInt<uint32_t>(ConstString("\xc6\x00\x00")), 0x0000c6);
  EXPECT_EQ(LittleEndianByteStrToInt<uint32_t>(ConstString("\x33\x77\xbb\xff")), 0xffbb7733);
  EXPECT_EQ(LittleEndianByteStrToInt<uint32_t>(ConstString("\x33\x77\xbb\xff")), -0x4488cd);

  // int32_t cases.
  EXPECT_EQ(LittleEndianByteStrToInt<int32_t>(ConstString("\x78\x56\x34\x12")), 0x12345678);
  EXPECT_EQ(LittleEndianByteStrToInt<int32_t>(ConstString("\xc6\x00\x00")), 0x0000c6);
  EXPECT_EQ(LittleEndianByteStrToInt<int32_t>(ConstString("\x33\x77\xbb\xff")), 0xffbb7733);
  EXPECT_EQ(LittleEndianByteStrToInt<int32_t>(ConstString("\x33\x77\xbb\xff")), -0x4488cd);

  // 64-bit case.
  EXPECT_EQ(LittleEndianByteStrToInt<int64_t>(
                std::string(ConstStringView("\xf0\xde\xbc\x9a\x78\x56\x34\x12"))),
            0x123456789abcdef0);
}

}  // namespace utils
}  // namespace pl
