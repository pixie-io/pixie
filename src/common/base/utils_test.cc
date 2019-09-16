#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <vector>

#include "src/common/base/utils.h"

namespace pl {

using ::testing::StrEq;

TEST(ReprTest, ResultsAreAsExpected) {
  EXPECT_THAT(Repr("test\b"), StrEq(R"(test\x08)"));
  EXPECT_THAT(Repr("test\xab"), StrEq(R"(test\xAB)"));
  EXPECT_THAT(Repr("test\b", Radix::kBin, PrintConvPolicy::kToDigit),
              StrEq(R"(\b01110100\b01100101\b01110011\b01110100\b00001000)"));
}

TEST(AsciiHexToBytesTest, CheckConversionToString) {
  {
    std::string val = "0a2435383161353534662d";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {});
    ASSERT_OK(status_or_bytes);
    auto bytes = status_or_bytes.ValueOrDie();
    EXPECT_EQ(bytes, "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a:24:35:38:31:61:35:35:34:66:2d";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {':'});
    ASSERT_OK(status_or_bytes);
    auto bytes = status_or_bytes.ValueOrDie();
    EXPECT_EQ(bytes, "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a_24_35_38_31_61_35_35_34_66_2d";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {':', '_'});
    ASSERT_OK(status_or_bytes);
    auto bytes = status_or_bytes.ValueOrDie();
    EXPECT_EQ(bytes, "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a 24 35 38 31 61 35 35 34 66 2d";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {' '});
    ASSERT_OK(status_or_bytes);
    auto bytes = status_or_bytes.ValueOrDie();
    EXPECT_EQ(bytes, "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = ":0a24:3538:3161:3535:3466:2d";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {':', '_'});
    ASSERT_OK(status_or_bytes);
    auto bytes = status_or_bytes.ValueOrDie();
    EXPECT_EQ(bytes, "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }
}

TEST(AsciiHexToBytesTest, CheckConversionTypes) {
  {
    std::string val = "0a:24:35:38:31:61:35:35:34:66:2d";
    auto status_or_bytes = AsciiHexToBytes<std::vector<uint8_t>>(val, {':'});
    ASSERT_OK(status_or_bytes);
    std::vector<uint8_t> bytes = status_or_bytes.ConsumeValueOrDie();
    std::vector<uint8_t> expected_val = {'\x0a', '\x24', '\x35', '\x38', '\x31', '\x61',
                                         '\x35', '\x35', '\x34', '\x66', '\x2d'};
    EXPECT_EQ(bytes, expected_val);
  }

  // TODO(oazizi): Could add std::u8string once its available (C++20).
}

TEST(AsciiHexToBytesTest, FailureCases) {
  {
    std::string val = "0z:24";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {':'});
    ASSERT_NOT_OK(status_or_bytes);
  }

  {
    std::string val = "00:24";
    auto status_or_bytes = AsciiHexToBytes<std::string>(val, {});
    ASSERT_NOT_OK(status_or_bytes);
  }
}

TEST(Enumerate, LoopsThroughVectorWithIndex) {
  std::vector<int> vals = {0, 2, 4, 6, 8};

  for (const auto& [idx, val] : Enumerate(vals)) {
    EXPECT_EQ(idx * 2, val);
  }
}

TEST(CaseInsensitiveCompare, BasicsWithString) {
  CaseInsensitiveLess str_compare;

  EXPECT_FALSE(str_compare(std::string("foo"), std::string("foo")));
  EXPECT_FALSE(str_compare(std::string("Foo"), std::string("foo")));
  EXPECT_FALSE(str_compare(std::string("foo"), std::string("Foo")));
  EXPECT_FALSE(str_compare(std::string("Foo"), std::string("Foo")));

  EXPECT_FALSE(str_compare(std::string("foo"), std::string("bar")));
  EXPECT_FALSE(str_compare(std::string("Foo"), std::string("bar")));
  EXPECT_FALSE(str_compare(std::string("foo"), std::string("Bar")));
  EXPECT_FALSE(str_compare(std::string("Foo"), std::string("Bar")));

  EXPECT_TRUE(str_compare(std::string("bar"), std::string("foo")));
  EXPECT_TRUE(str_compare(std::string("bar"), std::string("Foo")));
  EXPECT_TRUE(str_compare(std::string("Bar"), std::string("foo")));
  EXPECT_TRUE(str_compare(std::string("Bar"), std::string("Foo")));
}

TEST(CaseInsensitiveCompare, BasicsWithStringView) {
  CaseInsensitiveLess str_compare;

  EXPECT_FALSE(str_compare(std::string_view("foo"), std::string_view("foo")));
  EXPECT_FALSE(str_compare(std::string_view("Foo"), std::string_view("foo")));
  EXPECT_FALSE(str_compare(std::string_view("foo"), std::string_view("Foo")));
  EXPECT_FALSE(str_compare(std::string_view("Foo"), std::string_view("Foo")));

  EXPECT_FALSE(str_compare(std::string_view("foo"), std::string_view("bar")));
  EXPECT_FALSE(str_compare(std::string_view("Foo"), std::string_view("bar")));
  EXPECT_FALSE(str_compare(std::string_view("foo"), std::string_view("Bar")));
  EXPECT_FALSE(str_compare(std::string_view("Foo"), std::string_view("Bar")));

  EXPECT_TRUE(str_compare(std::string_view("bar"), std::string_view("foo")));
  EXPECT_TRUE(str_compare(std::string_view("bar"), std::string_view("Foo")));
  EXPECT_TRUE(str_compare(std::string_view("Bar"), std::string_view("foo")));
  EXPECT_TRUE(str_compare(std::string_view("Bar"), std::string_view("Foo")));
}

TEST(CaseInsensitiveCompare, MapKey) {
  std::map<std::string, int, CaseInsensitiveLess> str_key_map;

  str_key_map["foo"] = 1;
  EXPECT_EQ(str_key_map["foo"], 1);
  EXPECT_EQ(str_key_map["Foo"], 1);
  EXPECT_EQ(str_key_map["fOo"], 1);

  // Re-write key using a different case.
  str_key_map["Foo"] = 2;
  EXPECT_EQ(str_key_map["foo"], 2);
  EXPECT_EQ(str_key_map["Foo"], 2);
  EXPECT_EQ(str_key_map["fOo"], 2);
}

}  // namespace pl
