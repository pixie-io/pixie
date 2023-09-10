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
#include <utility>
#include <vector>

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

TEST(BinaryDecoderTest, ExtractBEInt) {
  std::string_view data("\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractBEInt<int8_t>(), 1);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractBEInt<int16_t>(), 257);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractBEInt<int24_t>(), 65793);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractBEInt<int32_t>(), 16843009);
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractLEInt) {
  std::string_view data("\x10\x10\x01\x10\x01\x01\x10\x01\x01\x01");
  BinaryDecoder bin_decoder(data);

  ASSERT_OK_AND_EQ(bin_decoder.ExtractLEInt<int8_t>(), 16);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractLEInt<int16_t>(), 272);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractLEInt<int24_t>(), 65808);
  ASSERT_OK_AND_EQ(bin_decoder.ExtractLEInt<int32_t>(), 16843024);
  EXPECT_EQ(0, bin_decoder.BufSize());
}

TEST(BinaryDecoderTest, ExtractUVarInt) {
  std::vector<std::pair<std::vector<uint8_t>, uint64_t>> uVarInts = {
      {{}, 0},
      {{0x01}, 1},
      {{0x02}, 2},
      {{0x7f}, 127},
      {{0x80, 0x01}, 128},
      {{0xff, 0x01}, 255},
      {{0x80, 0x02}, 256},
      {{0x80, 0x80, 0x02}, 32768},
      {{0x80, 0x80, 0x80, 0x02}, 4194304},
      {{0x80, 0x80, 0x80, 0x80, 0x02}, 536870912},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x02}, 68719476736},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02}, 8796093022208},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02}, 1125899906842624},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02}, 144115188075855872},
      {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 0},
      {{0xd7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}, 18446744073709551575UL},
  };
  for (auto& p : uVarInts) {
    auto data = p.first;
    std::string_view s(reinterpret_cast<char*>(data.data()), data.size());
    BinaryDecoder bin_decoder(s);
    ASSERT_OK_AND_EQ(bin_decoder.ExtractUVarInt(), p.second);
    EXPECT_EQ(0, bin_decoder.BufSize());
  }
}

TEST(BinaryDecoderTest, ExtractUVarIntOverflow) {
  std::vector<std::vector<uint8_t>> uVarInts = {
      {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02},
      {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02},
      {0xd7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01},
      {0xd7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f},
  };
  for (auto& data : uVarInts) {
    std::string_view s(reinterpret_cast<char*>(data.data()), data.size());
    BinaryDecoder bin_decoder(s);
    EXPECT_NOT_OK(bin_decoder.ExtractUVarInt());
  }
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
