#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/builtins/string_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace builtins {

TEST(StringOps, basic_string_contains_test) {
  auto udf_tester = udf::UDFTester<ContainsUDF>();
  udf_tester.ForInput("apple", "pl").Expect(true);
  udf_tester.ForInput("apple", "z").Expect(false);
}

TEST(StringOps, basic_string_length_test) {
  auto udf_tester = udf::UDFTester<LengthUDF>();
  udf_tester.ForInput("").Expect(0);
  udf_tester.ForInput("apple").Expect(5);
}

TEST(StringOps, basic_string_find_test) {
  auto udf_tester = udf::UDFTester<FindUDF>();
  udf_tester.ForInput("pixielabs", "xie").Expect(2);
  udf_tester.ForInput("pixielabs", "hello").Expect(-1);
}

TEST(StringOps, basic_string_substr_test) {
  auto udf_tester = udf::UDFTester<SubstringUDF>();
  udf_tester.ForInput("pixielabs", 3, 4).Expect("iela");
  udf_tester.ForInput("pixielabs", 5, 10).Expect("labs");
}

TEST(StringOps, basic_string_tolower_test) {
  auto udf_tester = udf::UDFTester<ToLowerUDF>();
  udf_tester.ForInput("pIXiE").Expect("pixie");
}

TEST(StringOps, basic_string_toupper_test) {
  auto udf_tester = udf::UDFTester<ToUpperUDF>();
  udf_tester.ForInput("pIXiE").Expect("PIXIE");
}
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
