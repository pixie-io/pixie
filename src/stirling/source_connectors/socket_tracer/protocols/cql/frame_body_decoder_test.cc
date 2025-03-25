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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using ::px::stirling::protocols::cass::testutils::CreateFrame;

//-----------------------------------------------------------------------------
// Test Data
//-----------------------------------------------------------------------------

// The following are binary representations of selected values, in the CQL binary protocol.

constexpr std::string_view kEmpty = ConstStringView("");
constexpr std::string_view kByte = ConstStringView("\x01");
constexpr std::string_view kShort = ConstStringView("\x01\x23");
constexpr std::string_view kInt = ConstStringView("\x01\x23\x45\x67");
constexpr std::string_view kLong = ConstStringView("\x01\x23\x45\x67\x89\xab\xcd\xef");
constexpr std::string_view kString = ConstStringView(
    "\x00\x1a"
    "abcdefghijklmnopqrstuvwxyz");
constexpr std::string_view kEmptyString = ConstStringView("\x00\x00");
constexpr std::string_view kLongString = ConstStringView(
    "\x00\x00\x00\x1a"
    "abcdefghijklmnopqrstuvwxyz");
constexpr std::string_view kEmptyLongString = ConstStringView("\x00\x00\x00\x00");
constexpr std::string_view kNegativeLengthLongString = ConstStringView("\xf0\x00\x00\x00");
constexpr std::string_view kStringList = ConstStringView(
    "\x00\x03"
    "\x00\x1a"
    "abcdefghijklmnopqrstuvwxyz"
    "\x00\x06"
    "abcdef"
    "\x00\x05"
    "pixie");
constexpr std::string_view kBytes = ConstStringView(
    "\x00\x00\x00\x04"
    "\x01\x02\x03\x04");
constexpr std::string_view kEmptyBytes = ConstStringView("\x00\x00\x00\x00");
constexpr std::string_view kNegativeLengthBytes = ConstStringView("\xf0\x00\x00\x00");
constexpr std::string_view kShortBytes = ConstStringView(
    "\x00\x04"
    "\x01\x02\x03\x04");
constexpr std::string_view kEmptyShortBytes = ConstStringView("\x00\x00");
constexpr std::string_view kStringMap = ConstStringView(
    "\x00\x03"
    "\x00\x04"
    "key1"
    "\x00\x06"
    "value1"
    "\x00\x01"
    "k"
    "\x00\x01"
    "v"
    "\x00\x08"
    "question"
    "\x00\x06"
    "answer");
constexpr std::string_view kEmptyStringMap = ConstStringView("\x00\x00");
constexpr std::string_view kStringMultiMap = ConstStringView(
    "\x00\x02"
    "\x00\x03"
    "USA"
    "\x00\x02"
    "\x00\x08"
    "New York"
    "\x00\x0d"
    "San Francisco"
    "\x00\x06"
    "Canada"
    "\x00\x03"
    "\x00\x07"
    "Toronto"
    "\x00\x08"
    "Montreal"
    "\x00\x09"
    "Vancouver");
constexpr std::string_view kEmptyStringMultiMap = ConstStringView("\x00\x00");
constexpr std::string_view kUUID =
    ConstStringView("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f");
constexpr std::string_view kInet4 = ConstStringView("\x04\x01\x02\x03\x04\x00\x00\x00\x50");
constexpr std::string_view kInet6 = ConstStringView(
    "\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x50");
constexpr std::string_view kIntOption = ConstStringView("\x00\x09");
constexpr std::string_view kVarcharOption = ConstStringView("\x00\x0d");
constexpr std::string_view kCustomOption = ConstStringView(
    "\x00\x00"
    "\x00\x05"
    "pixie");

// The following are binary representations of more complex types in the CQL binary protocol.

const uint8_t kNameValuePair[] = {0x00, 0x00, 0x00, 0x0a, 0x31, 0x32, 0x37,
                                  0x34, 0x4c, 0x36, 0x33, 0x50, 0x31, 0x31};

const uint8_t kNameValuePairList[] = {
    0x00, 0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x0a, 0x31, 0x32, 0x37, 0x34, 0x4c, 0x36, 0x33, 0x50, 0x31, 0x31};

constexpr uint8_t kQueryParams[] = {
    0x00, 0x0a, 0x25, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x0a, 0x31, 0x32, 0x37, 0x34, 0x4c, 0x36, 0x33, 0x50, 0x31, 0x31, 0x00,
    0x00, 0x13, 0x88, 0x00, 0x05, 0x9e, 0x78, 0x90, 0xa3, 0x2b, 0x71};

constexpr uint8_t kResultMetadata[] = {
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x06, 0x73, 0x79, 0x73, 0x74, 0x65,
    0x6d, 0x00, 0x05, 0x70, 0x65, 0x65, 0x72, 0x73, 0x00, 0x04, 0x70, 0x65, 0x65, 0x72, 0x00,
    0x10, 0x00, 0x0b, 0x64, 0x61, 0x74, 0x61, 0x5f, 0x63, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x00,
    0x0d, 0x00, 0x07, 0x68, 0x6f, 0x73, 0x74, 0x5f, 0x69, 0x64, 0x00, 0x0c, 0x00, 0x0c, 0x70,
    0x72, 0x65, 0x66, 0x65, 0x72, 0x72, 0x65, 0x64, 0x5f, 0x69, 0x70, 0x00, 0x10, 0x00, 0x04,
    0x72, 0x61, 0x63, 0x6b, 0x00, 0x0d, 0x00, 0x0f, 0x72, 0x65, 0x6c, 0x65, 0x61, 0x73, 0x65,
    0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0d, 0x00, 0x0b, 0x72, 0x70, 0x63,
    0x5f, 0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x00, 0x10, 0x00, 0x0e, 0x73, 0x63, 0x68,
    0x65, 0x6d, 0x61, 0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0c, 0x00, 0x06,
    0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x73, 0x00, 0x22, 0x00, 0x0d};

constexpr uint8_t kResultPreparedMetadata[] = {
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x00, 0x09,
    0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x31, 0x00, 0x08, 0x63, 0x6f, 0x75, 0x6e, 0x74,
    0x65, 0x72, 0x31, 0x00, 0x02, 0x43, 0x30, 0x00, 0x05, 0x00, 0x02, 0x43, 0x31, 0x00, 0x05, 0x00,
    0x02, 0x43, 0x32, 0x00, 0x05, 0x00, 0x02, 0x43, 0x33, 0x00, 0x05, 0x00, 0x02, 0x43, 0x34, 0x00,
    0x05, 0x00, 0x03, 0x6b, 0x65, 0x79, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04};

constexpr uint8_t kSchemaChange[]{0x00, 0x07, 0x44, 0x52, 0x4f, 0x50, 0x50, 0x45, 0x44, 0x00,
                                  0x05, 0x54, 0x41, 0x42, 0x4c, 0x45, 0x00, 0x0e, 0x74, 0x75,
                                  0x74, 0x6f, 0x72, 0x69, 0x61, 0x6c, 0x73, 0x70, 0x6f, 0x69,
                                  0x6e, 0x74, 0x00, 0x03, 0x65, 0x6d, 0x70};

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

//------------------------
// ExtractInt
//------------------------

TEST(ExtractInt, Exact) {
  FrameBodyDecoder decoder(kInt);
  ASSERT_OK_AND_EQ(decoder.ExtractInt(), 0x01234567);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractInt, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractInt());
}

TEST(ExtractInt, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kInt.data(), kInt.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractInt());
}

TEST(ExtractInt, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kInt.data(), kInt.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractInt(), 0x01234567);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractShort
//------------------------

TEST(ExtractShort, Exact) {
  FrameBodyDecoder decoder(kShort);
  ASSERT_OK_AND_EQ(decoder.ExtractShort(), 0x0123);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractShort, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractShort());
}

TEST(ExtractShort, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kShort.data(), kShort.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractShort());
}

TEST(ExtractShort, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kShort.data(), kShort.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractShort(), 0x0123);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractLong
//------------------------

TEST(ExtractLong, Exact) {
  FrameBodyDecoder decoder(kLong);
  ASSERT_OK_AND_EQ(decoder.ExtractLong(), 0x0123456789abcdef);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLong, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractLong());
}

TEST(ExtractLong, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kLong.data(), kLong.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractLong());
}

TEST(ExtractLong, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kLong.data(), kLong.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractLong(), 0x0123456789abcdef);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractByte
//------------------------

TEST(ExtractByte, Exact) {
  FrameBodyDecoder decoder(kByte);
  ASSERT_OK_AND_EQ(decoder.ExtractByte(), 0x01);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractByte, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractByte());
}

TEST(ExtractByte, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kByte.data(), kByte.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractByte());
}

TEST(ExtractByte, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kByte.data(), kByte.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractByte(), 0x01);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractString
//------------------------

TEST(ExtractString, Exact) {
  FrameBodyDecoder decoder(kString);
  ASSERT_OK_AND_EQ(decoder.ExtractString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractString, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractString());
}

TEST(ExtractString, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kString.data(), kString.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractString());
}

TEST(ExtractString, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kString.data(), kString.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractString, EmptyString) {
  FrameBodyDecoder decoder(kEmptyString);
  ASSERT_OK_AND_THAT(decoder.ExtractString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractLongString
//------------------------

TEST(ExtractLongString, Exact) {
  FrameBodyDecoder decoder(kLongString);
  ASSERT_OK_AND_EQ(decoder.ExtractLongString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLongString, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractLongString());
}

TEST(ExtractLongString, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kLongString.data(), kLongString.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractLongString());
}

TEST(ExtractLongString, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kLongString.data(), kLongString.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractLongString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractLongString, EmptyString) {
  FrameBodyDecoder decoder(kEmptyLongString);
  ASSERT_OK_AND_THAT(decoder.ExtractLongString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLongString, NegativeLengthString) {
  FrameBodyDecoder decoder(kNegativeLengthLongString);
  ASSERT_OK_AND_THAT(decoder.ExtractLongString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringList
//------------------------

TEST(ExtractStringList, Exact) {
  FrameBodyDecoder decoder(kStringList);
  ASSERT_OK_AND_THAT(decoder.ExtractStringList(),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringList, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

TEST(ExtractStringList, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kStringList.data(), kStringList.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

TEST(ExtractStringList, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kStringList.data(), kStringList.size() + 1));
  ASSERT_OK_AND_THAT(decoder.ExtractStringList(),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringList, BadElement) {
  std::string buf(kStringList);
  // Change size encoding of first string in the list.
  buf[3] = 1;
  FrameBodyDecoder decoder(buf);
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

//------------------------
// ExtractBytes
//------------------------

TEST(ExtractBytes, Exact) {
  FrameBodyDecoder decoder(kBytes);
  ASSERT_OK_AND_EQ(decoder.ExtractBytes(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractBytes, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractBytes());
}

TEST(ExtractBytes, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kBytes.data(), kBytes.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractBytes());
}

TEST(ExtractBytes, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kBytes.data(), kBytes.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractBytes(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractBytes, EmptyBytes) {
  FrameBodyDecoder decoder(kEmptyBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractBytes, NegativeLengthString) {
  FrameBodyDecoder decoder(kNegativeLengthBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractValue
//------------------------

TEST(ExtractValue, Exact) {
  FrameBodyDecoder decoder(kBytes);
  ASSERT_OK_AND_EQ(decoder.ExtractValue(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractValue, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractValue());
}

TEST(ExtractValue, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kBytes.data(), kBytes.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractValue());
}

TEST(ExtractValue, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kBytes.data(), kBytes.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractValue(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractValue, EmptyBytes) {
  FrameBodyDecoder decoder(kEmptyBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractValue(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractValue, NegativeLengthString) {
  {
    constexpr std::string_view kValue = ConstStringView("\xff\xff\xff\xff");
    FrameBodyDecoder decoder(kValue);
    ASSERT_OK_AND_EQ(decoder.ExtractValue(), std::basic_string<uint8_t>());
    ASSERT_TRUE(decoder.eof());
  }

  {
    constexpr std::string_view kValue = ConstStringView("\xff\xff\xff\xfe");
    FrameBodyDecoder decoder(kValue);
    ASSERT_OK_AND_EQ(decoder.ExtractValue(), std::basic_string<uint8_t>());
    ASSERT_TRUE(decoder.eof());
  }

  {
    constexpr std::string_view kValue = ConstStringView("\xff\xff\xff\xfd");
    FrameBodyDecoder decoder(kValue);
    ASSERT_NOT_OK(decoder.ExtractValue());
    ASSERT_TRUE(decoder.eof());
  }
}

//------------------------
// ExtractShortBytes
//------------------------

TEST(ExtractShortBytes, Exact) {
  FrameBodyDecoder decoder(kShortBytes);
  ASSERT_OK_AND_EQ(decoder.ExtractShortBytes(),
                   std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractShortBytes, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractShortBytes());
}

TEST(ExtractShortBytes, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kShortBytes.data(), kShortBytes.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractShortBytes());
}

TEST(ExtractShortBytes, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kShortBytes.data(), kShortBytes.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractShortBytes(),
                   std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractShortBytes, EmptyBytes) {
  FrameBodyDecoder decoder(kEmptyShortBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractShortBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringMap
//------------------------

TEST(ExtractStringMap, Exact) {
  FrameBodyDecoder decoder(kStringMap);
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMap(),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringMap, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringMap());
}

TEST(ExtractStringMap, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kStringMap.data(), kStringMap.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringMap());
}

TEST(ExtractStringMap, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kStringMap.data(), kStringMap.size() + 1));
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMap(),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringMap, EmptyMap) {
  FrameBodyDecoder decoder(kEmptyStringMap);
  ASSERT_OK_AND_THAT(decoder.ExtractStringMap(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringMultiMap
//------------------------

TEST(ExtractStringMultiMap, Exact) {
  FrameBodyDecoder decoder(kStringMultiMap);
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMultiMap(),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringMultiMap, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractStringMultiMap, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kStringMultiMap.data(), kStringMultiMap.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractStringMultiMap, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kStringMultiMap.data(), kStringMultiMap.size() + 1));
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMultiMap(),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringMultiMap, EmptyMap) {
  FrameBodyDecoder decoder(kEmptyStringMultiMap);
  ASSERT_OK_AND_THAT(decoder.ExtractStringMultiMap(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractUUID
//------------------------

TEST(ExtractUUID, Exact) {
  FrameBodyDecoder decoder(kUUID);
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, decoder.ExtractUUID());
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractUUID, Empty) {
  FrameBodyDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractUUID());
}

TEST(ExtractUUID, Undersized) {
  FrameBodyDecoder decoder(std::string_view(kUUID.data(), kUUID.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractUUID());
}

TEST(ExtractUUID, Oversized) {
  FrameBodyDecoder decoder(std::string_view(kUUID.data(), kUUID.size() + 1));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, decoder.ExtractUUID());
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractInet
//------------------------

TEST(ExtractInet, V4Exact) {
  FrameBodyDecoder decoder(kInet4);
  ASSERT_OK_AND_ASSIGN(SockAddr addr, decoder.ExtractInet());
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv4);
  const auto& addr4 = std::get<SockAddrIPv4>(addr.addr);
  EXPECT_EQ(addr4.AddrStr(), "1.2.3.4");
  EXPECT_EQ(addr4.port, 80);
  ASSERT_TRUE(decoder.eof());

  PX_UNUSED(kInet6);
}

TEST(ExtractInet, V6Exact) {
  FrameBodyDecoder decoder(kInet6);
  ASSERT_OK_AND_ASSIGN(SockAddr addr, decoder.ExtractInet());
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv6);
  const auto& addr6 = std::get<SockAddrIPv6>(addr.addr);
  EXPECT_EQ(addr6.AddrStr(), "::1");
  EXPECT_EQ(addr6.port, 80);
  ASSERT_TRUE(decoder.eof());

  PX_UNUSED(kInet6);
}

//------------------------
// ExtractOption
//------------------------

TEST(ExtractOption, Exact) {
  {
    FrameBodyDecoder decoder(kIntOption);
    ASSERT_OK_AND_ASSIGN(Option option, decoder.ExtractOption());
    EXPECT_EQ(option.type, DataType::kInt);
    EXPECT_THAT(option.value, IsEmpty());
    ASSERT_TRUE(decoder.eof());
  }

  {
    FrameBodyDecoder decoder(kVarcharOption);
    ASSERT_OK_AND_ASSIGN(Option option, decoder.ExtractOption());
    EXPECT_EQ(option.type, DataType::kVarchar);
    EXPECT_THAT(option.value, IsEmpty());
    ASSERT_TRUE(decoder.eof());
  }

  {
    FrameBodyDecoder decoder(kCustomOption);
    ASSERT_OK_AND_ASSIGN(Option option, decoder.ExtractOption());
    EXPECT_EQ(option.type, DataType::kCustom);
    EXPECT_EQ(option.value, "pixie");
    ASSERT_TRUE(decoder.eof());
  }
}

//------------------------
// ExtractNameValuePair
//------------------------

TEST(ExtractNameValuePair, Exact) {
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kNameValuePair));
  ASSERT_OK_AND_ASSIGN(NameValuePair nv, decoder.ExtractNameValuePair(false));

  EXPECT_EQ(nv.name, "");
  EXPECT_EQ(CreateStringView<char>(nv.value), "1274L63P11");
}

//------------------------
// ExtractNameValuePairList
//------------------------

TEST(ExtractNameValuePairList, Exact) {
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kNameValuePairList));
  ASSERT_OK_AND_ASSIGN(std::vector<NameValuePair> nv_list, decoder.ExtractNameValuePairList(false));

  ASSERT_EQ(nv_list.size(), 6);
  EXPECT_EQ(nv_list[0].name, "");
  EXPECT_EQ(CreateStringView<char>(nv_list[0].value),
            ConstStringView("\x00\x00\x00\x00\x00\x00\x00\x01"));
  EXPECT_EQ(nv_list[5].name, "");
  EXPECT_EQ(CreateStringView<char>(nv_list[5].value), "1274L63P11");
}

//------------------------
// ExtractQueryParams
//------------------------

TEST(ExtractQueryParams, Exact) {
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kQueryParams));
  ASSERT_OK_AND_ASSIGN(QueryParameters qp, decoder.ExtractQueryParameters());

  EXPECT_EQ(qp.consistency, 10);  // LOCAL_ONE
  EXPECT_EQ(qp.flags, 0x25);
  ASSERT_EQ(qp.values.size(), 6);
  EXPECT_EQ(CreateStringView(qp.values[5].value), "1274L63P11");
  EXPECT_EQ(qp.page_size, 5000);
  EXPECT_THAT(qp.paging_state, IsEmpty());
  EXPECT_EQ(qp.serial_consistency, 0);
  EXPECT_EQ(qp.timestamp, 1581615543430001);
}

//------------------------
// ExtractQueryParams
//------------------------

TEST(ExtractResultMetadata, Basic) {
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kResultMetadata));
  ASSERT_OK_AND_ASSIGN(ResultMetadata md, decoder.ExtractResultMetadata());

  EXPECT_EQ(md.flags, 1);
  EXPECT_EQ(md.columns_count, 9);
  EXPECT_THAT(md.paging_state, IsEmpty());
  EXPECT_EQ(md.gts_keyspace_name, "system");
  EXPECT_EQ(md.gts_table_name, "peers");
  ASSERT_EQ(md.col_specs.size(), md.columns_count);
  EXPECT_EQ(md.col_specs[0].name, "peer");
  EXPECT_EQ(md.col_specs[0].type.type, DataType::kInet);
  EXPECT_EQ(md.col_specs[7].name, "schema_version");
  EXPECT_EQ(md.col_specs[7].type.type, DataType::kUuid);
  EXPECT_EQ(md.col_specs[8].name, "tokens");
  EXPECT_EQ(md.col_specs[8].type.type, DataType::kSet);
}

TEST(ExtractResultMetdata, Prepared) {
  constexpr int kProtocolVersion = 4;
  constexpr bool kResultKindPrepared = true;
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kResultPreparedMetadata), kProtocolVersion);
  ASSERT_OK_AND_ASSIGN(ResultMetadata md, decoder.ExtractResultMetadata(kResultKindPrepared));

  EXPECT_EQ(md.flags, 1);
  EXPECT_EQ(md.columns_count, 6);
  EXPECT_THAT(md.paging_state, IsEmpty());
  EXPECT_EQ(md.gts_keyspace_name, "keyspace1");
  EXPECT_EQ(md.gts_table_name, "counter1");
  ASSERT_EQ(md.col_specs.size(), md.columns_count);
  EXPECT_EQ(md.col_specs[0].name, "C0");
  EXPECT_EQ(md.col_specs[0].type.type, DataType::kCounter);
  EXPECT_EQ(md.col_specs[4].name, "C4");
  EXPECT_EQ(md.col_specs[4].type.type, DataType::kCounter);
  EXPECT_EQ(md.col_specs[5].name, "key");
  EXPECT_EQ(md.col_specs[5].type.type, DataType::kBlob);
}

//------------------------
// ExtractSchemaChange
//------------------------

TEST(ExtractSchemaChange, Basic) {
  FrameBodyDecoder decoder(CreateCharArrayView<char>(kSchemaChange));
  ASSERT_OK_AND_ASSIGN(SchemaChange sc, decoder.ExtractSchemaChange());

  EXPECT_EQ(sc.change_type, "DROPPED");
  EXPECT_EQ(sc.target, "TABLE");
  EXPECT_THAT(sc.keyspace, "tutorialspoint");
  EXPECT_EQ(sc.name, "emp");
  EXPECT_THAT(sc.arg_types, IsEmpty());
}

TEST(ParseResultResp, RowsResult) {
  ResultResp expected_resp;
  expected_resp.kind = ResultRespKind::kRows;

  ResultRowsResp expected_rows_resp;
  expected_rows_resp.metadata.flags = 0;
  expected_rows_resp.metadata.columns_count = 2;
  ColSpec col_spec;
  col_spec.ks_name = "keyspace";
  col_spec.table_name = "table";
  col_spec.name = "col1";
  col_spec.type = {
      .type = protocols::cass::DataType::kVarchar,
      .value = "",
  };
  expected_rows_resp.metadata.col_specs.push_back(col_spec);
  col_spec.name = "col2";
  expected_rows_resp.metadata.col_specs.push_back(col_spec);

  // Estimate body size (without rows) by settings rows_count to 0.
  expected_rows_resp.rows_count = 0;
  expected_resp.resp = expected_rows_resp;
  auto resp_body = testutils::ResultRespToByteString(expected_resp);

  uint16_t stream = 1;
  uint64_t ts = 1;
  auto frame = CreateFrame(stream, Opcode::kResult, resp_body, ts);

  ASSERT_OK_AND_ASSIGN(ResultResp resp, ParseResultResp(&frame));

  EXPECT_EQ(resp, expected_resp);
}

TEST(ParseResultResp, VoidResult) {
  ResultResp expected_resp;
  expected_resp.kind = ResultRespKind::kVoid;

  auto resp_body = testutils::ResultRespToByteString(expected_resp);

  uint16_t stream = 1;
  uint64_t ts = 1;
  auto frame = CreateFrame(stream, Opcode::kResult, resp_body, ts);

  ASSERT_OK_AND_ASSIGN(ResultResp resp, ParseResultResp(&frame));

  EXPECT_EQ(resp, expected_resp);
}

TEST(ParseQueryReq, Basic) {
  QueryReq expected_req;
  expected_req.query = "SELECT * FROM table;";
  expected_req.qp.flags = 0;
  expected_req.qp.consistency = 1;

  auto body = testutils::QueryReqToByteString(expected_req);

  uint16_t stream = 1;
  uint64_t ts = 1;
  auto frame = CreateFrame(stream, Opcode::kResult, body, ts);

  ASSERT_OK_AND_ASSIGN(QueryReq req, ParseQueryReq(&frame));
  EXPECT_EQ(req, expected_req);
}

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
