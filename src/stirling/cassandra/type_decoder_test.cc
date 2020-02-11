#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/cassandra/type_decoder.h"

#include "src/common/testing/testing.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace pl {
namespace stirling {
namespace cass {

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

//------------------------
// ExtractInt
//------------------------

TEST(ExtractInt, Exact) {
  TypeDecoder decoder(kInt);
  ASSERT_OK_AND_EQ(decoder.ExtractInt(), 0x01234567);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractInt, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractInt());
}

TEST(ExtractInt, Undersized) {
  TypeDecoder decoder(std::string_view(kInt.data(), kInt.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractInt());
}

TEST(ExtractInt, Oversized) {
  TypeDecoder decoder(std::string_view(kInt.data(), kInt.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractInt(), 0x01234567);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractShort
//------------------------

TEST(ExtractShort, Exact) {
  TypeDecoder decoder(kShort);
  ASSERT_OK_AND_EQ(decoder.ExtractShort(), 0x0123);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractShort, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractShort());
}

TEST(ExtractShort, Undersized) {
  TypeDecoder decoder(std::string_view(kShort.data(), kShort.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractShort());
}

TEST(ExtractShort, Oversized) {
  TypeDecoder decoder(std::string_view(kShort.data(), kShort.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractShort(), 0x0123);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractLong
//------------------------

TEST(ExtractLong, Exact) {
  TypeDecoder decoder(kLong);
  ASSERT_OK_AND_EQ(decoder.ExtractLong(), 0x0123456789abcdef);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLong, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractLong());
}

TEST(ExtractLong, Undersized) {
  TypeDecoder decoder(std::string_view(kLong.data(), kLong.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractLong());
}

TEST(ExtractLong, Oversized) {
  TypeDecoder decoder(std::string_view(kLong.data(), kLong.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractLong(), 0x0123456789abcdef);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractByte
//------------------------

TEST(ExtractByte, Exact) {
  TypeDecoder decoder(kByte);
  ASSERT_OK_AND_EQ(decoder.ExtractByte(), 0x01);
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractByte, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractByte());
}

TEST(ExtractByte, Undersized) {
  TypeDecoder decoder(std::string_view(kByte.data(), kByte.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractByte());
}

TEST(ExtractByte, Oversized) {
  TypeDecoder decoder(std::string_view(kByte.data(), kByte.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractByte(), 0x01);
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractString
//------------------------

TEST(ExtractString, Exact) {
  TypeDecoder decoder(kString);
  ASSERT_OK_AND_EQ(decoder.ExtractString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractString, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractString());
}

TEST(ExtractString, Undersized) {
  TypeDecoder decoder(std::string_view(kString.data(), kString.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractString());
}

TEST(ExtractString, Oversized) {
  TypeDecoder decoder(std::string_view(kString.data(), kString.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractString, EmptyString) {
  TypeDecoder decoder(kEmptyString);
  ASSERT_OK_AND_THAT(decoder.ExtractString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractLongString
//------------------------

TEST(ExtractLongString, Exact) {
  TypeDecoder decoder(kLongString);
  ASSERT_OK_AND_EQ(decoder.ExtractLongString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLongString, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractLongString());
}

TEST(ExtractLongString, Undersized) {
  TypeDecoder decoder(std::string_view(kLongString.data(), kLongString.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractLongString());
}

TEST(ExtractLongString, Oversized) {
  TypeDecoder decoder(std::string_view(kLongString.data(), kLongString.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractLongString(), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractLongString, EmptyString) {
  TypeDecoder decoder(kEmptyLongString);
  ASSERT_OK_AND_THAT(decoder.ExtractLongString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractLongString, NegativeLengthString) {
  TypeDecoder decoder(kNegativeLengthLongString);
  ASSERT_OK_AND_THAT(decoder.ExtractLongString(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringList
//------------------------

TEST(ExtractStringList, Exact) {
  TypeDecoder decoder(kStringList);
  ASSERT_OK_AND_THAT(decoder.ExtractStringList(),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringList, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

TEST(ExtractStringList, Undersized) {
  TypeDecoder decoder(std::string_view(kStringList.data(), kStringList.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

TEST(ExtractStringList, Oversized) {
  TypeDecoder decoder(std::string_view(kStringList.data(), kStringList.size() + 1));
  ASSERT_OK_AND_THAT(decoder.ExtractStringList(),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringList, BadElement) {
  std::string buf(kStringList);
  // Change size encoding of first string in the list.
  buf[3] = 1;
  TypeDecoder decoder(buf);
  ASSERT_NOT_OK(decoder.ExtractStringList());
}

//------------------------
// ExtractBytes
//------------------------

TEST(ExtractBytes, Exact) {
  TypeDecoder decoder(kBytes);
  ASSERT_OK_AND_EQ(decoder.ExtractBytes(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractBytes, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractBytes());
}

TEST(ExtractBytes, Undersized) {
  TypeDecoder decoder(std::string_view(kBytes.data(), kBytes.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractBytes());
}

TEST(ExtractBytes, Oversized) {
  TypeDecoder decoder(std::string_view(kBytes.data(), kBytes.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractBytes(), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractBytes, EmptyBytes) {
  TypeDecoder decoder(kEmptyBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractBytes, NegativeLengthString) {
  TypeDecoder decoder(kNegativeLengthBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractShortBytes
//------------------------

TEST(ExtractShortBytes, Exact) {
  TypeDecoder decoder(kShortBytes);
  ASSERT_OK_AND_EQ(decoder.ExtractShortBytes(),
                   std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractShortBytes, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractShortBytes());
}

TEST(ExtractShortBytes, Undersized) {
  TypeDecoder decoder(std::string_view(kShortBytes.data(), kShortBytes.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractShortBytes());
}

TEST(ExtractShortBytes, Oversized) {
  TypeDecoder decoder(std::string_view(kShortBytes.data(), kShortBytes.size() + 1));
  ASSERT_OK_AND_EQ(decoder.ExtractShortBytes(),
                   std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractShortBytes, EmptyBytes) {
  TypeDecoder decoder(kEmptyShortBytes);
  ASSERT_OK_AND_THAT(decoder.ExtractShortBytes(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringMap
//------------------------

TEST(ExtractStringMap, Exact) {
  TypeDecoder decoder(kStringMap);
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMap(),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringMap, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringMap());
}

TEST(ExtractStringMap, Undersized) {
  TypeDecoder decoder(std::string_view(kStringMap.data(), kStringMap.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringMap());
}

TEST(ExtractStringMap, Oversized) {
  TypeDecoder decoder(std::string_view(kStringMap.data(), kStringMap.size() + 1));
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMap(),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringMap, EmptyMap) {
  TypeDecoder decoder(kEmptyStringMap);
  ASSERT_OK_AND_THAT(decoder.ExtractStringMap(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractStringMultiMap
//------------------------

TEST(ExtractStringMultiMap, Exact) {
  TypeDecoder decoder(kStringMultiMap);
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMultiMap(),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractStringMultiMap, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractStringMultiMap, Undersized) {
  TypeDecoder decoder(std::string_view(kStringMultiMap.data(), kStringMultiMap.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractStringMultiMap, Oversized) {
  TypeDecoder decoder(std::string_view(kStringMultiMap.data(), kStringMultiMap.size() + 1));
  ASSERT_OK_AND_THAT(
      decoder.ExtractStringMultiMap(),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_FALSE(decoder.eof());
}

TEST(ExtractStringMultiMap, EmptyMap) {
  TypeDecoder decoder(kEmptyStringMultiMap);
  ASSERT_OK_AND_THAT(decoder.ExtractStringMultiMap(), IsEmpty());
  ASSERT_TRUE(decoder.eof());
}

//------------------------
// ExtractUUID
//------------------------

TEST(ExtractUUID, Exact) {
  TypeDecoder decoder(kUUID);
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, decoder.ExtractUUID());
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_TRUE(decoder.eof());
}

TEST(ExtractUUID, Empty) {
  TypeDecoder decoder(kEmpty);
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractUUID, Undersized) {
  TypeDecoder decoder(std::string_view(kUUID.data(), kUUID.size() - 1));
  ASSERT_NOT_OK(decoder.ExtractStringMultiMap());
}

TEST(ExtractUUID, Oversized) {
  TypeDecoder decoder(std::string_view(kUUID.data(), kUUID.size() + 1));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, decoder.ExtractUUID());
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_FALSE(decoder.eof());
}

//------------------------
// ExtractInet
//------------------------

// TODO(oazizi): Add tests.

//------------------------
// ExtractConsistency
//------------------------

// TODO(oazizi): Add tests.

}  // namespace cass
}  // namespace stirling
}  // namespace pl
