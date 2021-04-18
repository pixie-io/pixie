/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/utils/binary_decoder.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {

using ::testing::StrEq;

TEST(BinaryDecoderTest, ExtractChar) {
  std::string_view data("\xff\x02");
  BinaryDecoder bin_decoder(data);

  constexpr uint8_t k255 = 255;
  ASSERT_OK_AND_EQ(bin_decoder.ExtractChar<uint8_t>(), k255);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractChar<char>(), 2);
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractInt) {
  std::string_view data("\x01\x01\x01\x01\x01\x01\x01");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int8_t>(), 1);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int16_t>(), 257);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractInt<int32_t>(), 16843009);
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractString) {
  std::string_view data("abc123");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractString(3), "abc");
  ASSERT_OK_AND_EQ(bin_decoder.ExtractString(3), "123");
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractStringUntil) {
  std::string_view data("name!value!name");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractStringUntil('!'), "name");
  EXPECT_EQ("value!name", bin_decoder.Buf());
  ASSERT_OK_AND_EQ(bin_decoder.ExtractStringUntil('!'), "value");
  EXPECT_EQ("name", bin_decoder.Buf());
  EXPECT_NOT_OK(bin_decoder.ExtractStringUntil('!'));
  EXPECT_EQ("name", bin_decoder.Buf());
}

TEST(BinaryDecoderTest, ExtractStringUntilStr) {
  std::string_view data("name!!value@@name");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractStringUntil("!!"), "name");
  EXPECT_EQ("value@@name", bin_decoder.Buf());
  ASSERT_OK_AND_EQ(bin_decoder.ExtractStringUntil("@@"), "value");
  EXPECT_EQ("name", bin_decoder.Buf());
  EXPECT_NOT_OK(bin_decoder.ExtractStringUntil("!"));
  EXPECT_EQ("name", bin_decoder.Buf());
}

TEST(BinaryDecoderTest, TooShortText) {
  {
    std::string_view data("");
    BinaryDecoder bin_decoder(data);

    EXPECT_NOT_OK(bin_decoder.ExtractStringUntil("aaa"));
    EXPECT_THAT(std::string(bin_decoder.Buf()), StrEq(""));
  }
  {
    std::string_view data("a");
    BinaryDecoder bin_decoder(data);

    EXPECT_NOT_OK(bin_decoder.ExtractStringUntil("aaa"));
    EXPECT_THAT(std::string(bin_decoder.Buf()), StrEq("a"));
  }
  {
    std::string_view data("aa");
    BinaryDecoder bin_decoder(data);

    EXPECT_NOT_OK(bin_decoder.ExtractStringUntil("aaa"));
    EXPECT_THAT(std::string(bin_decoder.Buf()), StrEq("aa"));
  }
}

}  // namespace stirling
}  // namespace px
