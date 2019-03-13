#include <gtest/gtest.h>

#include "src/carnot/builtins/json_ops.h"
#include "src/carnot/udf/test_utils.h"

namespace pl {
namespace carnot {
namespace builtins {

const char* kTestJSONStr = R"(
{
  "str_key": {"abc": "def"},
  "int64_key": 34243242341,
  "float64_key": 123423.5234
})";

TEST(JSONOps, PluckUDF) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput(kTestJSONStr, "str_key").Expect(R"({"abc":"def"})");
}

TEST(JSONOps, PluckAsInt64UDF) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput(kTestJSONStr, "int64_key").Expect(34243242341);
}

TEST(JSONOps, PluckAsFloat4UDF) {
  auto udf_tester = udf::UDFTester<PluckAsFloat4UDF>();
  udf_tester.ForInput(kTestJSONStr, "float64_key").Expect(123423.5234);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
