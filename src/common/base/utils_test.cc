#include <map>
#include <vector>

#include "src/common/base/utils.h"
#include "src/common/testing/testing.h"

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

TEST(CreateStringView, FromArrayToCharString) {
  const char char_array[] = "abcdef";
  EXPECT_EQ("abcdef", CreateStringView<char>(char_array));
}

TEST(CreateStringView, FromArrayToU8String) {
  const char char_array[] = "abcdef";
  std::basic_string<uint8_t> expected = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ(expected, CreateStringView<uint8_t>(char_array));
}

TEST(CreateStringView, FromContainerToCharString) {
  std::string char_str = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ("abcdef", CreateStringView(char_str));

  std::basic_string<uint8_t> uint8_str = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ("abcdef", CreateStringView(uint8_str));

  std::vector<uint8_t> uint8_vec = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ("abcdef", CreateStringView(uint8_vec));

  std::vector<uint32_t> uint32_vec = {0x64636261, 0x68676665};
  EXPECT_EQ("abcdefgh", CreateStringView(uint32_vec));
}

TEST(CreateStringView, FromContainerToU8String) {
  std::basic_string<uint8_t> expected = {'a', 'b', 'c', 'd', 'e', 'f'};

  std::string char_str = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ(expected, CreateStringView<uint8_t>(char_str));

  std::basic_string<uint8_t> uint8_str = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ(expected, CreateStringView<uint8_t>(uint8_str));

  std::vector<uint8_t> uint8_vec = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ(expected, CreateStringView<uint8_t>(uint8_vec));

  expected = {'a', 'b', 'c', 'd', 'e', 'f', 0, 0};
  std::vector<uint32_t> uint32_vec = {0x64636261, 0x6665};
  EXPECT_EQ(expected, CreateStringView<uint8_t>(uint32_vec));
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

TEST(MakeArray, IntArrayTest) {
  constexpr auto arr = MakeArray(1, 2, 3, 4);
  static_assert(std::is_same_v<const std::array<int, 4>, decltype(arr)>, "array type check");
  static_assert(arr.size() == 4, "array size");
}

TEST(ArrayTransform, LambdaFunc) {
  constexpr auto arr = MakeArray(1, 2, 3, 4);
  constexpr auto arr2 = ArrayTransform(arr, [](int x) { return x + 1; });
  static_assert(std::is_same_v<const std::array<int, 4>, decltype(arr2)>, "array type check");
  static_assert(arr2.size() == 4, "array size");
  static_assert(arr2[0] == 2, "correct value");
}

}  // namespace pl
