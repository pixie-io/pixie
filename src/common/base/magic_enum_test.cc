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

#include <magic_enum.hpp>
#include "src/common/testing/testing.h"

// The following test cases are mostly from MagicEnum docs: https://github.com/Neargye/magic_enum.
// They serve as a good example of the capabilities of MagicEnum.
// Note that pretty much all the tests/examples below can be run as constexpr too.

enum class Color { RED = 2, BLUE = 4, GREEN = 8 };
constexpr std::size_t kColorCount = magic_enum::enum_count<Color>();

// Tests values that go beyond MAGIC_ENUM_RANGE_MAX.
enum class WideColor { INFRARED = -1024, RED = 2, VIOLET = 256, ULTRAVIOLET = 1024 };

TEST(MagicEnum, num_elements) { EXPECT_EQ(kColorCount, 3); }

TEST(MagicEnum, enum_to_string) {
  Color color = Color::RED;
  std::string_view color_name = magic_enum::enum_name(color);
  EXPECT_EQ(color_name, "RED");
}

TEST(MagicEnum, unknown_enum_to_string) {
  Color color = static_cast<Color>(1);
  std::string_view color_name = magic_enum::enum_name(color);
  EXPECT_EQ(color_name, "");
}

// This tests that we have MAGIC_ENUM_RANGE_MAX set properly.
TEST(MagicEnum, large_enum_to_string) {
  // We can see VIOLET because its value is exactly MAGIC_ENUM_RANGE_MAX.
  {
    WideColor color = WideColor::VIOLET;
    std::string_view color_name = magic_enum::enum_name(color);
    EXPECT_EQ(color_name, "VIOLET");
  }

  // We can't see ULTRAVIOLET because the value is beyond MAGIC_ENUM_RANGE_MAX.
  {
    WideColor color = WideColor::ULTRAVIOLET;
    std::string_view color_name = magic_enum::enum_name(color);
    EXPECT_EQ(color_name, "");
  }

  // We can't see INFRARED because the value is below MAGIC_ENUM_RANGE_MIN.
  {
    WideColor color = WideColor::INFRARED;
    std::string_view color_name = magic_enum::enum_name(color);
    EXPECT_EQ(color_name, "");
  }
}

// Like enum_to_string above, but passing name through template parameter.
// Good for constexpr.
TEST(MagicEnum, static_enum_to_string) {
  constexpr Color color = Color::BLUE;
  std::string_view color_name = magic_enum::enum_name<color>();
  CHECK_EQ(color_name, "BLUE");
}

TEST(MagicEnum, enum_to_integer) {
  Color color = Color::RED;
  EXPECT_EQ(magic_enum::enum_integer(color), 2);
}

TEST(MagicEnum, valid_string_to_enum) {
  std::string_view color_name("GREEN");
  std::optional<Color> color = magic_enum::enum_cast<Color>(color_name);
  ASSERT_TRUE(color.has_value());
  EXPECT_EQ(color.value(), Color::GREEN);
}

TEST(MagicEnum, invalid_string_to_enum) {
  std::string_view color_name("YURPLE");
  std::optional<Color> color = magic_enum::enum_cast<Color>(color_name);
  ASSERT_FALSE(color.has_value());
}

TEST(MagicEnum, valid_integer_to_enum) {
  std::optional<Color> color = magic_enum::enum_cast<Color>(2);
  ASSERT_TRUE(color.has_value());
  EXPECT_EQ(color.value(), Color::RED);
}

TEST(MagicEnum, invalid_integer_to_enum) {
  std::optional<Color> color = magic_enum::enum_cast<Color>(999);
  ASSERT_FALSE(color.has_value());
}

TEST(MagicEnum, valid_indexed_access) {
  Color color = magic_enum::enum_value<Color>(1);
  EXPECT_EQ(color, Color::BLUE);
}

// Note that this test must be in a Test Suite than the rest, according to:
// https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#death-test-naming
TEST(MagicEnumDeathTest, invalid_indexed_access) {
#if !defined(NDEBUG)
  EXPECT_DEATH((void)magic_enum::enum_value<Color>(999), "");
#endif
}

TEST(MagicEnum, enum_value_sequence) {
  std::array<Color, kColorCount> colors = magic_enum::enum_values<Color>();
  std::array<Color, kColorCount> expected_values = {Color::RED, Color::BLUE, Color::GREEN};
  EXPECT_EQ(colors, expected_values);
}

TEST(MagicEnum, enum_names_sequence) {
  std::array<std::string_view, kColorCount> color_names = magic_enum::enum_names<Color>();
  std::array<std::string_view, kColorCount> expected_values = {"RED", "BLUE", "GREEN"};
  EXPECT_EQ(color_names, expected_values);
}

TEST(MagicEnum, enum_entries_sequence) {
  std::array<std::pair<Color, std::string_view>, kColorCount> color_entries =
      magic_enum::enum_entries<Color>();
  std::array<std::pair<Color, std::string_view>, kColorCount> expected_entries = {
      std::pair<Color, std::string_view>{Color::RED, "RED"},
      std::pair<Color, std::string_view>{Color::BLUE, "BLUE"},
      std::pair<Color, std::string_view>{Color::GREEN, "GREEN"}};
  EXPECT_EQ(color_entries, expected_entries);
}

TEST(MagicEnum, ostream_operator) {
  using magic_enum::ostream_operators::operator<<;
  Color color = Color::BLUE;
  std::ostringstream buffer;
  buffer << color << std::endl;
  EXPECT_EQ(buffer.str(), "BLUE\n");
}

TEST(MagicEnum, bitwise_operators) {
  enum class Flags { A = 1 << 0, B = 1 << 1, C = 1 << 2, D = 1 << 3 };
  using magic_enum::bitwise_operators::operator|;
  using magic_enum::bitwise_operators::operator&;
  using magic_enum::bitwise_operators::operator~;
  Flags flagsAB = Flags::A | Flags::B;
  Flags flagsBC = Flags::B | Flags::C;
  EXPECT_TRUE((flagsAB & Flags::A) == Flags::A);
  EXPECT_FALSE((flagsAB & Flags::C) == Flags::C);
  EXPECT_TRUE(((flagsAB & flagsBC) & Flags::B) == Flags::B);
  EXPECT_FALSE(((flagsAB & flagsBC) & Flags::C) == Flags::C);
  EXPECT_TRUE(((flagsAB | flagsBC) & Flags::C) == Flags::C);
  EXPECT_TRUE(((flagsAB & ~flagsBC) & Flags::A) == Flags::A);
  EXPECT_FALSE(((flagsAB & ~flagsBC) & Flags::B) == Flags::B);
}

TEST(MagicEnum, is_unscoped_enum) {
  enum color { red, green, blue };
  enum class direction { left, right };

  EXPECT_TRUE(magic_enum::is_unscoped_enum<color>::value);
  EXPECT_FALSE(magic_enum::is_unscoped_enum<direction>::value);
  EXPECT_FALSE(magic_enum::is_unscoped_enum<int>::value);

  EXPECT_TRUE(magic_enum::is_unscoped_enum_v<color>);
  EXPECT_FALSE(magic_enum::is_unscoped_enum_v<direction>);
  EXPECT_FALSE(magic_enum::is_unscoped_enum_v<int>);

  EXPECT_FALSE(magic_enum::is_scoped_enum<color>::value);
  EXPECT_TRUE(magic_enum::is_scoped_enum<direction>::value);
  EXPECT_FALSE(magic_enum::is_scoped_enum<int>::value);

  EXPECT_FALSE(magic_enum::is_scoped_enum_v<color>);
  EXPECT_TRUE(magic_enum::is_scoped_enum_v<direction>);
  EXPECT_FALSE(magic_enum::is_scoped_enum_v<int>);
}
