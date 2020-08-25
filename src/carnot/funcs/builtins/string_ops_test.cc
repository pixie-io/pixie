#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/funcs/builtins/string_ops.h"
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
  udf_tester.ForInput("pixielabs", 9, 20).Expect("");
}

TEST(StringOps, basic_string_tolower_test) {
  auto udf_tester = udf::UDFTester<ToLowerUDF>();
  udf_tester.ForInput("pIXiE").Expect("pixie");
}

TEST(StringOps, basic_string_toupper_test) {
  auto udf_tester = udf::UDFTester<ToUpperUDF>();
  udf_tester.ForInput("pIXiE").Expect("PIXIE");
}

TEST(StringOps, basic_string_trim_test) {
  auto udf_tester = udf::UDFTester<TrimUDF>();
  udf_tester.ForInput("   ").Expect("");
  udf_tester.ForInput(" pixieLabs ").Expect("pixieLabs");
  udf_tester.ForInput("pixie").Expect("pixie");
  udf_tester.ForInput("p   ixie").Expect("p   ixie");
}

TEST(StringOps, basic_string_strip_prefix) {
  auto udf_tester = udf::UDFTester<StripPrefixUDF>();
  udf_tester.ForInput("sock-shop/", "sock-shop/carts").Expect("carts");
  udf_tester.ForInput("sock-shop/carts", "sock-shop/carts").Expect("");
  udf_tester.ForInput("sock-shop/carts123", "sock-shop/carts").Expect("sock-shop/carts");
}

TEST(StringOps, HexToASCII) {
  auto udf_tester = udf::UDFTester<HexToASCII>();
  udf_tester.ForInput("36623330303663622d393632612d343030302d616235652d333636383564616634383030")
      .Expect("6b3006cb-962a-4000-ab5e-36685daf4800");
  // Missing last nibble.
  udf_tester.ForInput("333").Expect("");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
