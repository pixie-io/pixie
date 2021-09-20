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

#include <map>
#include <vector>

#include "src/common/base/utils.h"
#include "src/common/testing/testing.h"

namespace px {

using ::testing::StrEq;

TEST(BytesToStringTest, Hex) {
  EXPECT_THAT(BytesToString<bytes_format::Hex>("\x03\x74\x32\xaa"), StrEq(R"(\x03\x74\x32\xAA)"));
  EXPECT_THAT(BytesToString<bytes_format::HexCompact>("\x03\x74\x32\xaa"), StrEq("037432AA"));
}

TEST(BytesToStringTest, HexAsciiMix) {
  EXPECT_THAT(BytesToString<bytes_format::HexAsciiMix>("test\b"), StrEq(R"(test\x08)"));
  EXPECT_THAT(BytesToString<bytes_format::HexAsciiMix>("test\xab"), StrEq(R"(test\xAB)"));
  EXPECT_THAT(BytesToString<bytes_format::HexAsciiMix>("\x74"
                                                       "est"
                                                       "\xab"),
              StrEq(R"(test\xAB)"));
}

TEST(BytesToStringTest, Bin) {
  EXPECT_THAT(BytesToString<bytes_format::Bin>("test\b"),
              StrEq(R"(\b01110100\b01100101\b01110011\b01110100\b00001000)"));
}

TEST(AsciiHexToBytesTest, CheckConversionToString) {
  {
    std::string val = "0a2435383161353534662d";
    EXPECT_OK_AND_EQ(AsciiHexToBytes<std::string>(val, {}),
                     "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a:24:35:38:31:61:35:35:34:66:2d";
    EXPECT_OK_AND_EQ(AsciiHexToBytes<std::string>(val, {':'}),
                     "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a_24_35_38_31_61_35_35_34_66_2d";
    EXPECT_OK_AND_EQ(AsciiHexToBytes<std::string>(val, {':', '_'}),
                     "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = "0a 24 35 38 31 61 35 35 34 66 2d";
    EXPECT_OK_AND_EQ(AsciiHexToBytes<std::string>(val, {' '}),
                     "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }

  {
    std::string val = ":0a24:3538:3161:3535:3466:2d";
    EXPECT_OK_AND_EQ(AsciiHexToBytes<std::string>(val, {':', '_'}),
                     "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d");
  }
}

TEST(AsciiHexToBytesTest, CheckConversionTypes) {
  {
    std::string val = "0a:24:35:38:31:61:35:35:34:66:2d";
    ASSERT_OK_AND_ASSIGN(auto bytes, AsciiHexToBytes<std::vector<uint8_t>>(val, {':'}));
    std::vector<uint8_t> expected_val = {'\x0a', '\x24', '\x35', '\x38', '\x31', '\x61',
                                         '\x35', '\x35', '\x34', '\x66', '\x2d'};
    EXPECT_EQ(bytes, expected_val);
  }

  // TODO(oazizi): Could add std::u8string once its available (C++20).
}

TEST(AsciiHexToBytesTest, FailureCases) {
  {
    std::string val = "0z:24";
    ASSERT_NOT_OK(AsciiHexToBytes<std::string>(val, {':'}));
  }

  {
    std::string val = "00:24";
    ASSERT_NOT_OK(AsciiHexToBytes<std::string>(val, {}));
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

TEST(IntOps, IntRoundUpDivide) {
  EXPECT_EQ(IntRoundUpDivide(0, 2), 0);
  EXPECT_EQ(IntRoundUpDivide(1, 2), 1);
  EXPECT_EQ(IntRoundUpDivide(2, 2), 1);
  EXPECT_EQ(IntRoundUpDivide(3, 2), 2);

  EXPECT_EQ(IntRoundUpDivide(0, 10), 0);
  EXPECT_EQ(IntRoundUpDivide(9, 10), 1);
  EXPECT_EQ(IntRoundUpDivide(10, 10), 1);
  EXPECT_EQ(IntRoundUpDivide(11, 10), 2);
}

TEST(IntOps, IntRoundUpToPow2) {
  EXPECT_EQ(IntRoundUpToPow2(1), 1);
  EXPECT_EQ(IntRoundUpToPow2(5), 8);
  EXPECT_EQ(IntRoundUpToPow2(7), 8);
  EXPECT_EQ(IntRoundUpToPow2(8), 8);
  EXPECT_EQ(IntRoundUpToPow2(9), 16);
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

TEST(MakeArray, StringArrayTest) {
  {
    constexpr auto kArray = MakeArray("one", "two", "three", "four");
    static_assert(std::is_same_v<decltype(kArray), const std::array<const char*, 4>>,
                  "By default--and not surprisingly--MakeArray defaults to C-style strings.");
  }

  {
    constexpr auto kArray = MakeArray<std::string_view>("one", "two", "three", "four");
    static_assert(std::is_same_v<decltype(kArray), const std::array<std::string_view, 4>>,
                  "You can make MakeArray use std::string_view.");
  }
}

TEST(MakeArray, ComplexTypeArrayTest) {
  struct Foo {
    int a;
    int b;
  };

  struct Bar {
    int a;
    int b;
    constexpr Bar(int a, int b) : a(a), b(b) {}
  };

  {
    constexpr auto kArray = MakeArray<Bar>(Bar(1, 1), Bar(2, 2));
    static_assert(std::is_same_v<decltype(kArray), const std::array<Bar, 2>>,
                  "Where type is explicit, things work as expected.");
  }

  {
    constexpr auto kArray = MakeArray<Foo>({1, 1});
    static_assert(std::is_same_v<decltype(kArray), const std::array<Foo, 1>>,
                  "One can use the template argument to force the type on the first element.");
  }

  //  {
  //    // The following is not valid, unfortunately:
  //     constexpr auto kArray = MakeArray<Foo>({1, 1}, {2, 2});
  //     static_assert(std::is_same_v<decltype(kArray), const std::array<Foo, 1>>, "MakeArray cannot
  //     force the type on all elements.");
  //  }

  {
    // Instead one must specify the type of every element.
    constexpr auto kArray = MakeArray(Foo{1, 1}, Foo{2, 2});
    static_assert(
        std::is_same_v<decltype(kArray), const std::array<Foo, 2>>,
        "One must declare the type of every element for MakeArray to work with hard-coded values.");
  }

  {
    // Alternatively, use the overloaded implementation of MakeArray that accepts C-arrays.
    // Note the extra set of braces around all the elements.
    constexpr auto kArray = MakeArray<Foo>({{1, 1}, {2, 2}});
    static_assert(std::is_same_v<decltype(kArray), const std::array<Foo, 2>>,
                  "This version makes what we're looking for");
  }
}

TEST(ArrayTransform, LambdaFunc) {
  constexpr auto arr = MakeArray(1, 2, 3, 4);
  constexpr auto arr2 = ArrayTransform(arr, [](int x) { return x + 1; });
  static_assert(std::is_same_v<const std::array<int, 4>, decltype(arr2)>, "array type check");
  static_assert(arr2.size() == 4, "array size");
  static_assert(arr2[0] == 2, "correct value");
}

enum class Color { kRed = 2, kBlue = 4, kGreen = 8 };

TEST(EnumCastOrReturn, Basic) {
  ASSERT_OK_AND_EQ(EnumCast<Color>(2), Color::kRed);
  ASSERT_NOT_OK(EnumCast<Color>(1));
}

struct NameValue {
  std::string name;
  int value;

  std::string ToString() const { return absl::Substitute("name=$0 value=$1", name, value); }
};

namespace nvspace {
struct NameValue {
  std::string name;
  int value;

  std::string ToString() const { return absl::Substitute("name=$0 value=$1", name, value); }
};
}  // namespace nvspace

TEST(AutoStreamOperator, Basic) {
  {
    NameValue nv{"pixie", 1};
    std::stringstream buffer;
    buffer << nv;
    EXPECT_EQ(buffer.str(), "name=pixie value=1");
  }

  {
    nvspace::NameValue nv{"pixienaut", 2};
    std::stringstream buffer;
    buffer << nv;
    EXPECT_EQ(buffer.str(), "name=pixienaut value=2");
  }
}

TEST(FloorTest, Basic) {
  std::map<int, int> v = {{1, 100}, {3, 300}, {5, 500}};
  EXPECT_EQ(Floor(v, 0), v.end());
  EXPECT_EQ(Floor(v, 1), v.begin());
  EXPECT_EQ(Floor(v, 2), v.begin());
  EXPECT_EQ(Floor(v, 3), v.find(3));
  EXPECT_EQ(Floor(v, 4), v.find(3));
  EXPECT_EQ(Floor(v, 5), v.find(5));
  EXPECT_EQ(Floor(v, 6), v.find(5));
}

TEST(LinearInterpolate, BasicUInt64) {
  uint64_t x_a = 0;
  uint64_t x_b = 10;
  int64_t y_a = 10;
  int64_t y_b = 20;
  uint64_t value = 5;

  EXPECT_EQ(15, LinearInterpolate(x_a, x_b, y_a, y_b, value));
}

TEST(LinearInterpolate, ZeroIntervalUInt64) {
  uint64_t x_a = 0;
  uint64_t x_b = 0;
  int64_t y_a = 1111;
  // y_b and value are unused when x_a and x_b are the same, so there values should have no
  // influence.
  int64_t y_b = 123423143243214123;
  uint64_t value = 99999999999;

  EXPECT_EQ(y_a, LinearInterpolate(x_a, x_b, y_a, y_b, value));
}

TEST(LinearInterpolate, OutOfIntervalUInt64) {
  // value less than x_a
  {
    uint64_t x_a = 10;
    uint64_t x_b = 20;
    int64_t y_a = 50;
    int64_t y_b = 60;
    uint64_t value = 0;

    EXPECT_EQ(40, LinearInterpolate(x_a, x_b, y_a, y_b, value));
  }
  // value greater than x_b
  {
    uint64_t x_a = 10;
    uint64_t x_b = 20;
    int64_t y_a = 50;
    int64_t y_b = 60;
    uint64_t value = 30;

    EXPECT_EQ(70, LinearInterpolate(x_a, x_b, y_a, y_b, value));
  }
}

TEST(LinearInterpolate, SymmetryUInt64) {
  uint64_t x_a = 0;
  uint64_t x_b = 10;
  int64_t y_a = 10;
  int64_t y_b = 20;
  uint64_t value = 5;

  // if x_a > x_b, LinearInterpolate should still work and should be equivalent if the y_a and y_b
  // values are also swapped.
  EXPECT_EQ(LinearInterpolate(x_b, x_a, y_b, y_a, value),
            LinearInterpolate(x_a, x_b, y_a, y_b, value));
}

TEST(LinearInterpolate, LargeIntPrecision) {
  uint64_t x_a = 0;
  uint64_t x_b = 10;
  int64_t y_a = 1ULL << 63;
  int64_t y_b = y_a + 10000;
  uint64_t value = 5;
  EXPECT_EQ(y_a + 5000, LinearInterpolate(x_a, x_b, y_a, y_b, value));
}

}  // namespace px
