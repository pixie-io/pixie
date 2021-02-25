#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/carnot/funcs/builtins/conditionals.h"
#include "src/carnot/udf/test_utils.h"

namespace pl {
namespace carnot {
namespace builtins {

TEST(ConditionalsTest, SelectUDF) {
  auto udf_tester = udf::UDFTester<SelectUDF<types::Int64Value>>();
  udf_tester.ForInput(false, 20, 21).Expect(21);
  udf_tester.ForInput(true, 20, 21).Expect(20);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
