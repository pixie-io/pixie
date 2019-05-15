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

TEST(StringOps, basic_int64_add_test) {
  auto udf_tester = udf::UDFTester<ContainsUDF>();
  udf_tester.ForInput("apple", "pl").Expect(true);
  udf_tester.ForInput("apple", "z").Expect(false);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
