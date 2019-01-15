#include <gtest/gtest.h>
#include <type_traits>

#include "src/carnot/udf/udf.h"
#include "src/utils/macros.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace udf {

class ScalarUDF1 : ScalarUDF {
 public:
  Int64Value Exec(FunctionContext *ctx, BoolValue b1, Int64Value b2) {
    PL_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

class ScalarUDF1WithInit : ScalarUDF {
 public:
  Status Init(FunctionContext *ctx, Int64Value v1) {
    PL_UNUSED(ctx);
    PL_UNUSED(v1);
    return Status::OK();
  }
  Int64Value Exec(FunctionContext *ctx, BoolValue b1, BoolValue b2) {
    PL_UNUSED(ctx);
    return b1.val && b2.val ? 3 : 0;
  }
};

TEST(ScalarUDF, basic_tests) {
  EXPECT_EQ(UDFDataType::INT64, ScalarUDFTraits<ScalarUDF1>::ReturnType());
  EXPECT_EQ(std::vector<UDFDataType>({UDFDataType::BOOLEAN, UDFDataType::INT64}),
            ScalarUDFTraits<ScalarUDF1>::ExecArguments());
  EXPECT_FALSE(ScalarUDFTraits<ScalarUDF1>::HasInit());
  EXPECT_TRUE(ScalarUDFTraits<ScalarUDF1WithInit>::HasInit());
}

TEST(UDFDataTypes, valid_tests) {
  EXPECT_TRUE((true == IsValidUDFDataType<BoolValue>::value));
  EXPECT_TRUE((true == IsValidUDFDataType<Int64Value>::value));
  EXPECT_TRUE((true == IsValidUDFDataType<Float64Value>::value));
  EXPECT_TRUE((true == IsValidUDFDataType<StringValue>::value));
}

TEST(BoolValue, value_tests) {
  // Test constructor init.
  BoolValue v(false);
  // Test == overload.
  // NOLINTNEXTLINE(readability/check).
  EXPECT_TRUE(v == false);
  // Test assignment.
  v = true;
  EXPECT_EQ(true, v.val);
  // Check type base type.
  bool base_type_check = (true == std::is_base_of_v<UDFBaseValue, BoolValue>);
  EXPECT_TRUE(base_type_check);
}

TEST(Int64Value, value_tests) {
  // Test constructor init.
  Int64Value v(12);
  // Test == overload.
  // NOLINTNEXTLINE(readability/check).
  EXPECT_TRUE(v == 12);
  // Test assignment.
  v = 24;
  EXPECT_EQ(24, v.val);
  // Check type base type.
  bool base_type_check = (true == std::is_base_of_v<UDFBaseValue, Int64Value>);
  EXPECT_TRUE(base_type_check);
}

TEST(Float64Value, value_tests) {
  // Test constructor init.
  Float64Value v(12.5);
  // Test == overload.
  // NOLINTNEXTLINE(readability/check).
  EXPECT_TRUE(v == 12.5);
  // Test assignment.
  v = 24.2;
  EXPECT_DOUBLE_EQ(24.2, v.val);
  // Check type base type.
  bool base_type_check = (true == std::is_base_of_v<UDFBaseValue, Float64Value>);
  EXPECT_TRUE(base_type_check);
}

TEST(StringValue, value_tests) {
  StringValue sv("abcd");

  // Test == overload.
  // NOLINTNEXTLINE(readability/check).
  EXPECT_TRUE(sv == "abcd");
  // Test assignment.
  sv = "def";
  EXPECT_EQ("def", sv);
  // Check type base type.
  bool base_type_check = (true == std::is_base_of_v<UDFBaseValue, StringValue>);
  EXPECT_TRUE(base_type_check);
}

}  // namespace udf
}  // namespace carnot
}  // namespace pl
