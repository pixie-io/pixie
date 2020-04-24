#include "src/stirling/common/binary_decoder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

TEST(BinaryDecoderTest, ExtractInt) {
  std::string_view data("\x01\x01\x01\x01\x01\x01\x01");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int8_t>(), 1);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int16_t>(), 257);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int32_t>(), 16843009);
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractStringUtil) {
  std::string_view data("name!value!name");
  BinaryDecoder bin_decoder(data);

  EXPECT_EQ("name", bin_decoder.ExtractStringUtil('!'));
  EXPECT_EQ("value!name", bin_decoder.Buf());
  EXPECT_EQ("value", bin_decoder.ExtractStringUtil('!'));
  EXPECT_EQ("name", bin_decoder.Buf());
  EXPECT_EQ("", bin_decoder.ExtractStringUtil('!'));
  EXPECT_EQ("name", bin_decoder.Buf());
}

}  // namespace stirling
}  // namespace pl
