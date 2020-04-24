#include "src/stirling/common/binary_decoder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pl {
namespace stirling {

TEST(BinaryDecoderTest, ExtractInteger) {
  std::string_view data("\x01\x01\x01\x01\x01\x01\x01");
  BinaryDecoder bin_decoder(data);

  EXPECT_EQ(1, bin_decoder.ExtractInteger<int8_t>());
  EXPECT_EQ(257, bin_decoder.ExtractInteger<int16_t>());
  EXPECT_EQ(16843009, bin_decoder.ExtractInteger<int32_t>());
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
