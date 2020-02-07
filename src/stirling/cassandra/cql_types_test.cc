#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/cassandra/cql_types.h"

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
  std::string_view buf(kInt);
  ASSERT_OK_AND_EQ(ExtractInt(&buf), 0x01234567);
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractInt, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractInt(&buf));
}

TEST(ExtractInt, Undersized) {
  std::string_view buf(kInt.data(), kInt.size() - 1);
  ASSERT_NOT_OK(ExtractInt(&buf));
}

TEST(ExtractInt, Oversized) {
  std::string_view buf(kInt.data(), kInt.size() + 1);
  ASSERT_OK_AND_EQ(ExtractInt(&buf), 0x01234567);
  ASSERT_FALSE(buf.empty());
}

//------------------------
// ExtractShort
//------------------------

TEST(ExtractShort, Exact) {
  std::string_view buf(kShort);
  ASSERT_OK_AND_EQ(ExtractShort(&buf), 0x0123);
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractShort, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractShort(&buf));
}

TEST(ExtractShort, Undersized) {
  std::string_view buf(kShort.data(), kShort.size() - 1);
  ASSERT_NOT_OK(ExtractShort(&buf));
}

TEST(ExtractShort, Oversized) {
  std::string_view buf(kShort.data(), kShort.size() + 1);
  ASSERT_OK_AND_EQ(ExtractShort(&buf), 0x0123);
  ASSERT_FALSE(buf.empty());
}

//------------------------
// ExtractLong
//------------------------

TEST(ExtractLong, Exact) {
  std::string_view buf(kLong);
  ASSERT_OK_AND_EQ(ExtractLong(&buf), 0x0123456789abcdef);
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractLong, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractLong(&buf));
}

TEST(ExtractLong, Undersized) {
  std::string_view buf(kLong.data(), kLong.size() - 1);
  ASSERT_NOT_OK(ExtractLong(&buf));
}

TEST(ExtractLong, Oversized) {
  std::string_view buf(kLong.data(), kLong.size() + 1);
  ASSERT_OK_AND_EQ(ExtractLong(&buf), 0x0123456789abcdef);
  ASSERT_FALSE(buf.empty());
}

//------------------------
// ExtractByte
//------------------------

TEST(ExtractByte, Exact) {
  std::string_view buf(kByte);
  ASSERT_OK_AND_EQ(ExtractByte(&buf), 0x01);
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractByte, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractByte(&buf));
}

TEST(ExtractByte, Undersized) {
  std::string_view buf(kByte.data(), kByte.size() - 1);
  ASSERT_NOT_OK(ExtractByte(&buf));
}

TEST(ExtractByte, Oversized) {
  std::string_view buf(kByte.data(), kByte.size() + 1);
  ASSERT_OK_AND_EQ(ExtractByte(&buf), 0x01);
  ASSERT_FALSE(buf.empty());
}

//------------------------
// ExtractString
//------------------------

TEST(ExtractString, Exact) {
  std::string_view buf(kString);
  ASSERT_OK_AND_EQ(ExtractString(&buf), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractString, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractString(&buf));
}

TEST(ExtractString, Undersized) {
  std::string_view buf(kString.data(), kString.size() - 1);
  ASSERT_NOT_OK(ExtractString(&buf));
}

TEST(ExtractString, Oversized) {
  std::string_view buf(kString.data(), kString.size() + 1);
  ASSERT_OK_AND_EQ(ExtractString(&buf), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractString, EmptyString) {
  std::string_view buf(kEmptyString);
  ASSERT_OK_AND_THAT(ExtractString(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractLongString
//------------------------

TEST(ExtractLongString, Exact) {
  std::string_view buf(kLongString);
  ASSERT_OK_AND_EQ(ExtractLongString(&buf), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractLongString, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractLongString(&buf));
}

TEST(ExtractLongString, Undersized) {
  std::string_view buf(kLongString.data(), kLongString.size() - 1);
  ASSERT_NOT_OK(ExtractLongString(&buf));
}

TEST(ExtractLongString, Oversized) {
  std::string_view buf(kLongString.data(), kLongString.size() + 1);
  ASSERT_OK_AND_EQ(ExtractLongString(&buf), std::string("abcdefghijklmnopqrstuvwxyz"));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractLongString, EmptyString) {
  std::string_view buf(kEmptyLongString);
  ASSERT_OK_AND_THAT(ExtractLongString(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractLongString, NegativeLengthString) {
  std::string_view buf(kNegativeLengthLongString);
  ASSERT_OK_AND_THAT(ExtractLongString(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractStringList
//------------------------

TEST(ExtractStringList, Exact) {
  std::string_view buf(kStringList);
  ASSERT_OK_AND_THAT(ExtractStringList(&buf),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractStringList, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractStringList(&buf));
}

TEST(ExtractStringList, Undersized) {
  std::string_view buf(kStringList.data(), kStringList.size() - 1);
  ASSERT_NOT_OK(ExtractStringList(&buf));
}

TEST(ExtractStringList, Oversized) {
  std::string_view buf(kStringList.data(), kStringList.size() + 1);
  ASSERT_OK_AND_THAT(ExtractStringList(&buf),
                     ElementsAre(std::string("abcdefghijklmnopqrstuvwxyz"), std::string("abcdef"),
                                 std::string("pixie")));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractStringList, BadElement) {
  std::string buf(kStringList);
  // Change size encoding of first string in the list.
  buf[3] = 1;
  std::string_view buf_view(buf);
  ASSERT_NOT_OK(ExtractStringList(&buf_view));
}

//------------------------
// ExtractBytes
//------------------------

TEST(ExtractBytes, Exact) {
  std::string_view buf(kBytes);
  ASSERT_OK_AND_EQ(ExtractBytes(&buf), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractBytes, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractBytes(&buf));
}

TEST(ExtractBytes, Undersized) {
  std::string_view buf(kBytes.data(), kBytes.size() - 1);
  ASSERT_NOT_OK(ExtractBytes(&buf));
}

TEST(ExtractBytes, Oversized) {
  std::string_view buf(kBytes.data(), kBytes.size() + 1);
  ASSERT_OK_AND_EQ(ExtractBytes(&buf), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractBytes, EmptyBytes) {
  std::string_view buf(kEmptyBytes);
  ASSERT_OK_AND_THAT(ExtractBytes(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractBytes, NegativeLengthString) {
  std::string_view buf(kNegativeLengthBytes);
  ASSERT_OK_AND_THAT(ExtractBytes(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractShortBytes
//------------------------

TEST(ExtractShortBytes, Exact) {
  std::string_view buf(kShortBytes);
  ASSERT_OK_AND_EQ(ExtractShortBytes(&buf), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractShortBytes, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractShortBytes(&buf));
}

TEST(ExtractShortBytes, Undersized) {
  std::string_view buf(kShortBytes.data(), kShortBytes.size() - 1);
  ASSERT_NOT_OK(ExtractShortBytes(&buf));
}

TEST(ExtractShortBytes, Oversized) {
  std::string_view buf(kShortBytes.data(), kShortBytes.size() + 1);
  ASSERT_OK_AND_EQ(ExtractShortBytes(&buf), std::basic_string<uint8_t>({0x01, 0x02, 0x03, 0x04}));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractShortBytes, EmptyBytes) {
  std::string_view buf(kEmptyShortBytes);
  ASSERT_OK_AND_THAT(ExtractShortBytes(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractStringMap
//------------------------

TEST(ExtractStringMap, Exact) {
  std::string_view buf(kStringMap);
  ASSERT_OK_AND_THAT(
      ExtractStringMap(&buf),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractStringMap, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractStringMap(&buf));
}

TEST(ExtractStringMap, Undersized) {
  std::string_view buf(kStringMap.data(), kStringMap.size() - 1);
  ASSERT_NOT_OK(ExtractStringMap(&buf));
}

TEST(ExtractStringMap, Oversized) {
  std::string_view buf(kStringMap.data(), kStringMap.size() + 1);
  ASSERT_OK_AND_THAT(
      ExtractStringMap(&buf),
      UnorderedElementsAre(Pair("key1", "value1"), Pair("k", "v"), Pair("question", "answer")));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractStringMap, EmptyMap) {
  std::string_view buf(kEmptyStringMap);
  ASSERT_OK_AND_THAT(ExtractStringMap(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractStringMultiMap
//------------------------

TEST(ExtractStringMultiMap, Exact) {
  std::string_view buf(kStringMultiMap);
  ASSERT_OK_AND_THAT(
      ExtractStringMultiMap(&buf),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractStringMultiMap, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractStringMultiMap(&buf));
}

TEST(ExtractStringMultiMap, Undersized) {
  std::string_view buf(kStringMultiMap.data(), kStringMultiMap.size() - 1);
  ASSERT_NOT_OK(ExtractStringMultiMap(&buf));
}

TEST(ExtractStringMultiMap, Oversized) {
  std::string_view buf(kStringMultiMap.data(), kStringMultiMap.size() + 1);
  ASSERT_OK_AND_THAT(
      ExtractStringMultiMap(&buf),
      UnorderedElementsAre(Pair("USA", ElementsAre("New York", "San Francisco")),
                           Pair("Canada", ElementsAre("Toronto", "Montreal", "Vancouver"))));
  ASSERT_FALSE(buf.empty());
}

TEST(ExtractStringMultiMap, EmptyMap) {
  std::string_view buf(kEmptyStringMultiMap);
  ASSERT_OK_AND_THAT(ExtractStringMultiMap(&buf), IsEmpty());
  ASSERT_TRUE(buf.empty());
}

//------------------------
// ExtractUUID
//------------------------

TEST(ExtractUUID, Exact) {
  std::string_view buf(kUUID);
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ExtractUUID(&buf));
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_TRUE(buf.empty());
}

TEST(ExtractUUID, Empty) {
  std::string_view buf(kEmpty);
  ASSERT_NOT_OK(ExtractStringMultiMap(&buf));
}

TEST(ExtractUUID, Undersized) {
  std::string_view buf(kUUID.data(), kUUID.size() - 1);
  ASSERT_NOT_OK(ExtractStringMultiMap(&buf));
}

TEST(ExtractUUID, Oversized) {
  std::string_view buf(kUUID.data(), kUUID.size() + 1);
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ExtractUUID(&buf));
  EXPECT_EQ(uuid.str(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
  ASSERT_FALSE(buf.empty());
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
