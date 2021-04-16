#include <gtest/gtest.h>

#include "src/shared/types/hash_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {
namespace utils {

TEST(HashUtils, BoolValue) {
  BoolValue v1(false);
  BoolValue v2(true);

  EXPECT_NE(hash<BoolValue>{}(v1), hash<BoolValue>{}(v2));
}

TEST(HashUtils, Int64Value) {
  Int64Value v1(0);
  Int64Value v2(1);

  EXPECT_NE(hash<Int64Value>{}(v1), hash<Int64Value>{}(v2));
}

TEST(HashUtils, Float64Value) {
  Float64Value v1(0);
  Float64Value v2(1);

  EXPECT_NE(hash<Float64Value>{}(v1), hash<Float64Value>{}(v2));
}

TEST(HashUtils, StringValue) {
  StringValue v1("abc");
  StringValue v2("abcd");

  EXPECT_NE(hash<StringValue>{}(v1), hash<StringValue>{}(v2));
}

TEST(HashUtils, Time64NSValue) {
  Time64NSValue v1(0);
  Time64NSValue v2(1);

  EXPECT_NE(hash<Time64NSValue>{}(v1), hash<Time64NSValue>{}(v2));
}

}  // namespace utils
}  // namespace types
}  // namespace px
