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

#include <gtest/gtest.h>

#include "src/common/base/types.h"

namespace px {
namespace const_types_test {

TEST(ConstStringViewTest, const_string_view) {
  EXPECT_EQ(ConstStringView("This is a string"), std::string_view("This is a string"));

  // String views on string literals can be dangerous when there is a \x00 character.
  EXPECT_NE(ConstStringView("\xff\x00\x00"), std::string_view("\xff\x00\x00"));

  // But ConstStringView is smart about it.
  EXPECT_EQ(ConstStringView("\xff\x00\x00"), std::string_view("\xff\x00\x00", 3));
}

TEST(ConstStringTest, const_string) {
  EXPECT_EQ(ConstString("This is a string"), std::string("This is a string"));

  // Creating strings from string literals can be dangerous when there is a \x00 character.
  EXPECT_NE(ConstString("\xff\x00\x00"), std::string("\xff\x00\x00"));

  // But ConstString is smart about inferring the length.
  EXPECT_EQ(ConstString("\xff\x00\x00"), std::string("\xff\x00\x00", 3));
}

TEST(ConstStringViewTest, char_array_string_view) {
  // An array with a zero byte somewhere in the middle.
  char val[] = {1, 0, 2, 4};

  // Whoa...don't use ConstStringView on char arrays, because it strips off the last character.
  EXPECT_NE(ConstStringView(val), std::string_view(val, 4));
  EXPECT_EQ(ConstStringView(val).length(), 3);

  // Use CharArrayStringView and you'll get what you expect.
  EXPECT_EQ(CharArrayStringView(val), std::string_view(val, 4));
  EXPECT_EQ(CharArrayStringView(val).length(), 4);
}

TEST(ConstStringViewTest, compile_time_functions) {
  static constexpr std::string_view const_str0 = "This is a constant string";
  static constexpr std::string_view const_str1 = "It's really just a pointer and a size";
  static constexpr std::string_view const_str0_again = ConstStringView("This is a constant string");
  static constexpr std::string_view const_str2 = ConstStringView("\x00null\x23\x00");
  static constexpr std::string_view str2_strview = std::string_view("\x00null\x23\x00", 7);
  std::string str2_string = std::string("\x00null\x23\x00", 7);

  // First, test in ways that may or may not be used at compile-time.
  EXPECT_EQ(25, const_str0.size());
  EXPECT_EQ(37, const_str1.size());
  EXPECT_EQ(7, const_str2.size());
  EXPECT_EQ("This is a constant string", const_str0.data());
  EXPECT_EQ("This is a constant string", const_str0_again.data());
  EXPECT_EQ(std::string("It's really just a pointer and a size"), const_str1.data());
  EXPECT_FALSE(const_str0 == const_str1);
  EXPECT_FALSE(const_str1 == const_str0);
  EXPECT_TRUE(const_str0 == const_str0_again);
  EXPECT_TRUE(const_str2 == str2_string);
  EXPECT_TRUE(const_str2 == str2_strview);

  // Second, test in ways that must be used at compile-time.
  static_assert(25 == const_str0.size());
  static_assert(37 == const_str1.size());
  static_assert(7 == const_str2.size());
  static_assert(const_str0 != const_str1);
  static_assert(const_str1 != const_str0);
  static_assert(const_str0 == const_str0_again);
}

struct StrIntStruct {
  std::string_view str;
  uint64_t val;
};

TEST(ConstVectorTest, compile_time_functions) {
  static constexpr StrIntStruct values[] = {
      {"value0", 0},
      {"value1", 2},
      {"value2", 4},
  };
  constexpr ArrayView<StrIntStruct> elements = ArrayView(values);

  EXPECT_EQ(3, elements.size());
  EXPECT_EQ(2, elements[1].val);
  EXPECT_EQ(4, elements[2].val);
  EXPECT_EQ("value2", elements[2].str.data());

  static_assert(3 == elements.size());
  static_assert(2 == elements[1].val);
  static_assert(4 == elements[2].val);
  static_assert('v' == elements[2].str.data()[0]);
  static_assert('a' == elements[2].str.data()[1]);
  static_assert('l' == elements[2].str.data()[2]);
  static_assert('u' == elements[2].str.data()[3]);
  static_assert('e' == elements[2].str.data()[4]);
  static_assert('2' == elements[2].str.data()[5]);
}

TEST(ConstVectorTest, iterator_functions) {
  static constexpr StrIntStruct values[] = {
      {"value0", 0},
      {"value1", 2},
      {"value2", 4},
  };
  constexpr ArrayView<StrIntStruct> elements = ArrayView(values);

  uint32_t sum = 0;
  std::string s;
  for (auto& e : elements) {
    sum += e.val;
    s += e.str.data();
  }
  EXPECT_EQ(6, sum);
  EXPECT_EQ("value0value1value2", s);
}

TEST(ConstVectorTest, compile_time_lookup) {
  struct StrIntStructVector {
    ArrayView<StrIntStruct> elements;

    // NOLINTNEXTLINE
    constexpr explicit StrIntStructVector(ArrayView<StrIntStruct> elements) : elements(elements) {}

    // Compile-time lookup function within ArrayView<T>.
    constexpr uint32_t ValueIndex(const uint64_t key) const {
      uint32_t i = 0;
      for (i = 0; i < elements.size(); i++) {
        if (elements[i].val == key) {
          break;
        }
      }
      return i;
    }

    // Compile-time lookup function within ArrayView<T>.
    constexpr uint32_t StringIndex(std::string_view key) const {
      uint32_t i = 0;
      for (i = 0; i < elements.size(); i++) {
        if (elements[i].str == key) {
          break;
        }
      }
      return i;
    }
  };

  static constexpr StrIntStruct values[] = {
      {"value0", 0},
      {"value1", 1},
      {"value2", 2},
  };
  constexpr StrIntStructVector foo = StrIntStructVector(values);

  static_assert(2 == foo.ValueIndex(2));
  static_assert(1 == foo.StringIndex(ConstStringView("value1")));
  static_assert(foo.elements.size() == foo.ValueIndex(9));
  static_assert(foo.elements.size() == foo.StringIndex(("value")));
  static_assert(foo.elements.size() == foo.StringIndex(("value10")));
}

TEST(ContainerView, basics) {
  std::vector<int> vec = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  VectorView<int> vec_view_front(vec, 0, 5);
  EXPECT_EQ(vec_view_front.size(), 5);
  EXPECT_FALSE(vec_view_front.empty());
  EXPECT_EQ(vec_view_front.front(), 0);
  EXPECT_EQ(vec_view_front[3], 3);

  VectorView<int> vec_view_mid(vec, 2, 6);
  EXPECT_EQ(vec_view_mid.size(), 6);
  EXPECT_FALSE(vec_view_mid.empty());
  EXPECT_EQ(vec_view_mid.front(), 2);
  EXPECT_EQ(vec_view_mid[3], 5);

  VectorView<int> vec_view_empty(vec, 2, 0);
  EXPECT_EQ(vec_view_empty.size(), 0);
  EXPECT_TRUE(vec_view_empty.empty());
  // front() and operator[] have undefined behavior, just like std::vector.
}

TEST(ContainerView, iterator) {
  std::vector<int> vec = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  int expected_val = 2;
  for (const auto& val : VectorView<int>(vec, 2, 6)) {
    EXPECT_EQ(val, expected_val);
    ++expected_val;
  }
  EXPECT_EQ(expected_val, 8);

  for (const auto& val : VectorView<int>(vec, 0, 0)) {
    // Should never enter the loop.
    EXPECT_TRUE(false);
    (void)(val);
  }
}

TEST(int24_t, VerifyInitializationAndBitShifting) {
  EXPECT_EQ(sizeof(int24_t), 3);

  int24_t t1 = 1;
  EXPECT_EQ(t1 << 8, 256);

  // Assign an int that uses each of the 3 bytes
  int24_t t2 = 0x10111;
  EXPECT_EQ(t2 << 8, 0x11100);

  // Test negative numbers.
  int24_t t3 = 0xffffff;
  EXPECT_EQ(t3, -1);
  EXPECT_NE(t3, 16777215);
}

TEST(uint24_t, VerifyInitializationAndBitShifting) {
  EXPECT_EQ(sizeof(uint24_t), 3);

  uint24_t t1 = 1;
  EXPECT_EQ(t1 << 8, 256);

  // Assign an int that uses each of the 3 bytes
  uint24_t t2 = 0x10111;
  EXPECT_EQ(t2 << 8, 0x11100);

  // Test negative numbers.
  uint24_t val = 0xffffff;
  EXPECT_EQ(val, 0xffffff);
  EXPECT_NE(val, -1);
}

}  // namespace const_types_test
}  // namespace px
