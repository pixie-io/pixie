#include <gtest/gtest.h>

#include "src/carnot/udf/hash_utils.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace udf {
namespace utils {

TEST(HashUtils, HashCombine) { EXPECT_EQ(0xf4ff80ec63c103d4ULL, HashCombine(0, 1)); }

TEST(HashUtils, BoolValue) {
  BoolValue v1(0);
  BoolValue v2(1);

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

}  // namespace utils
}  // namespace udf
}  // namespace carnot
}  // namespace pl
