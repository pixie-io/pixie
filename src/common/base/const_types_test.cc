#include <gtest/gtest.h>

#include "src/common/base/const_types.h"

namespace pl {
namespace const_types_test {

TEST(ConstStringTest, compile_time_functions) {
  static constexpr ConstStrView const_str0 = "This is a constant string";
  static constexpr ConstStrView const_str1 = "It's really just a pointer and a size";
  static constexpr ConstStrView const_str0_again = "This is a constant string";

  // First, test in ways that may or may not be used at compile-time.
  EXPECT_EQ(25, const_str0.size());
  EXPECT_EQ(37, const_str1.size());
  EXPECT_EQ("This is a constant string", const_str0.get());
  EXPECT_EQ(std::string("It's really just a pointer and a size"), const_str1.get());
  EXPECT_FALSE(const_str0.equals(const_str1));
  EXPECT_FALSE(const_str1.equals(const_str0));
  EXPECT_TRUE(const_str0.equals(const_str0_again));

  // Second, test in ways that must be used at compile-time.
  static_assert(25 == const_str0.size());
  static_assert(37 == const_str1.size());
  static_assert(!const_str0.equals(const_str1));
  static_assert(!const_str1.equals(const_str0));
  static_assert(const_str0.equals(const_str0_again));
}

struct StrIntStruct {
  ConstStrView str;
  uint64_t val;
};

TEST(ConstVectorTest, compile_time_functions) {
  static constexpr StrIntStruct values[] = {
      {"value0", 0},
      {"value1", 2},
      {"value2", 4},
  };
  constexpr ConstVectorView<StrIntStruct> elements = ConstVectorView(values);

  EXPECT_EQ(3, elements.size());
  EXPECT_EQ(2, elements[1].val);
  EXPECT_EQ(4, elements[2].val);
  EXPECT_EQ("value2", elements[2].str.get());

  static_assert(3 == elements.size());
  static_assert(2 == elements[1].val);
  static_assert(4 == elements[2].val);
  static_assert('v' == elements[2].str.get()[0]);
  static_assert('a' == elements[2].str.get()[1]);
  static_assert('l' == elements[2].str.get()[2]);
  static_assert('u' == elements[2].str.get()[3]);
  static_assert('e' == elements[2].str.get()[4]);
  static_assert('2' == elements[2].str.get()[5]);
}

TEST(ConstVectorTest, iterator_functions) {
  static constexpr StrIntStruct values[] = {
      {"value0", 0},
      {"value1", 2},
      {"value2", 4},
  };
  constexpr ConstVectorView<StrIntStruct> elements = ConstVectorView(values);

  uint32_t sum = 0;
  std::string s;
  for (auto& e : elements) {
    sum += e.val;
    s += e.str.get();
  }
  EXPECT_EQ(6, sum);
  EXPECT_EQ("value0value1value2", s);
}

TEST(ConstVectorTest, compile_time_lookup) {
  struct StrIntStructVector {
    ConstVectorView<StrIntStruct> elements;

    // NOLINTNEXTLINE
    constexpr explicit StrIntStructVector(ConstVectorView<StrIntStruct> elements)
        : elements(elements) {}

    // Compile-time lookup function within ConstVectorView<T>.
    constexpr uint32_t ValueIndex(const uint64_t key) const {
      uint32_t i = 0;
      for (i = 0; i < elements.size(); i++) {
        if (elements[i].val == key) {
          break;
        }
      }
      return i;
    }

    // Compile-time lookup function within ConstVectorView<T>.
    constexpr uint32_t StringIndex(ConstStrView key) const {
      uint32_t i = 0;
      for (i = 0; i < elements.size(); i++) {
        if (elements[i].str.equals(key)) {
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
  static_assert(1 == foo.StringIndex(ConstStrView("value1")));
  static_assert(foo.elements.size() == foo.ValueIndex(9));
  static_assert(foo.elements.size() == foo.StringIndex((ConstStrView("value"))));
  static_assert(foo.elements.size() == foo.StringIndex((ConstStrView("value10"))));
}

}  // namespace const_types_test
}  // namespace pl
