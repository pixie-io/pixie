#include <gtest/gtest.h>

#include "src/carnot/funcs/builtins/json_ops.h"
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

TEST(JSONOps, PluckUDF_non_existent_key_return_empty) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput(kTestJSONStr, "blah").Expect("");
}

TEST(JSONOps, PluckUDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckUDF>();
  udf_tester.ForInput("asdad", "str_key").Expect("");
}

TEST(JSONOps, PluckAsInt64UDF) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput(kTestJSONStr, "int64_key").Expect(34243242341);
}

TEST(JSONOps, PluckAsInt64UDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsInt64UDF>();
  udf_tester.ForInput("sdasdsa", "int64_key").Expect(0);
}

TEST(JSONOps, PluckAsFloat64UDF) {
  auto udf_tester = udf::UDFTester<PluckAsFloat64UDF>();
  udf_tester.ForInput(kTestJSONStr, "float64_key").Expect(123423.5234);
}

TEST(JSONOps, PluckAsFloat64UDF_bad_input_return_empty) {
  auto udf_tester = udf::UDFTester<PluckAsFloat64UDF>();
  udf_tester.ForInput("sdadasd", "float64_key").Expect(0.0);
}
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
